#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

#define DEFAULT_THREADS_NUMBER 10
#define MINIMUM_FILE_SIZE_TO_ENABLE_MULTITHREADING 10000// 10k at minimum
#define MINIMUM_K_TO_ENABLE_MULTITHREADING 3

typedef struct {
    const char *kmer;// a pointer on the first char, of a string of length k, without a null byte as we are pointing on the original buffer
    unsigned count;
} KmerEntry;

typedef struct {
    KmerEntry *entries;
    unsigned count;
    unsigned capacity;
} KmerTable;

#define ASCII_COUNT 128

void init_kmer_table(KmerTable *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void add_kmer(KmerTable *table, const char *kmer, const int k) {
    KmerEntry *entries = table->entries;
    size_t tableCount = table->count;
    if (k >= 4) {
        // This pattern is very similar to SIMD pattern, just by comparing 8 bytes and 4 bytes at a time instead of one after another.
        u_int8_t rest8 = 0;
        u_int8_t rest4 = 0;
        if (k >= 8) {
            rest8 = k - (k % 8);
        }
        rest4 = k - (k % 4);

        for (int i = 0; i < tableCount; i++) {
            // Manage first part that is multiple of 8 with longs comparison
            for (size_t j = 0; j < rest8; j += 8)
                // Note: this hard to read line just compare the 8 bytes from left to right
                if (*(const u_int64_t *) (entries[i].kmer + j) != *(const u_int64_t *) (kmer + j))
                    goto next_round;

            // Manage multiple of 4 (only one round, precedent rounds would be made by previous loop for 8 multiple)
            if (rest8 < rest4)
                // Note: this hard to read line just compare the 4 bytes from left to right
                if (*(const u_int32_t *) (entries[i].kmer + rest8) != *(const u_int32_t *) (kmer + rest8))
                    goto next_round;

            // Manage the rest (< 4 chars)
            for (size_t j = rest4; j < k; j++)
                if (entries[i].kmer[j] != kmer[j])
                    goto next_round;

            // All chars have been compared at this point, if we didn't followed the goto, all chars are equal
            entries[i].count++;
            return;
        next_round:
        }
        // TODO: fix warning about C23 extension ???
    } else {
        // Normal execution for k < 4, compare each character one by one
        // This is separated from above to avoid the unnecessary branching overhead
        for (int i = 0; i < tableCount; i++) {
            for (size_t j = 0; j < k; j++)
                if (entries[i].kmer[j] != kmer[j])
                    goto next_round2;

            entries[i].count++;
            return;
        next_round2:
        }
    }

    if (table->count >= table->capacity) {
        table->capacity = (table->capacity == 0) ? 1 : table->capacity * 2;
        table->entries = realloc(table->entries, table->capacity * sizeof(KmerEntry));
        if (!table->entries) {
            perror("Error reallocating memory for k-mer table");
            exit(1);
        }
    }

    table->entries[table->count].kmer = kmer;
    table->entries[table->count].count = 1;
    table->count++;
}
typedef struct {
    // Specific args
    pthread_t thread_id;
    unsigned minCharIndex;
    unsigned maxCharIndex;
    // Common args (equal for all threads)
    char *content;
    size_t file_size;
    int k;
    KmerTable *tables;
} ThreadInfo;

// Researching and inserting for a specific set of chars between given min and max index
// this can be run in a separated thread or not, depending on the needs
void manageKmersOnFile(char *content, size_t file_size, int k, KmerTable *tables, int min, int max) {

    // Start extracting, searching k-mer and saving them
    for (long i = 0; i <= file_size - k; i++) {
        // Pick among one of the 37 available subtable
        char firstChar = content[i];
        if (firstChar < min || firstChar > max) {
            continue;
        }
        KmerTable *subtable = tables + firstChar;

        // Research in this subtable or insert it at the end
        add_kmer(subtable, content + i, k);
    }
}

// Wrapper around manageKmersOnFile with unpacking of values inside the ThreadInfo passed as (void *)
void *manageKmersOnFileThreaded(void *args) {
    ThreadInfo *info = (ThreadInfo *) args;
    manageKmersOnFile(info->content, info->file_size, info->k, info->tables, info->minCharIndex, info->maxCharIndex);
    return NULL;
}

// The calculations to dynamically cut the ASCII chars space in nbThreads or less threads
void calculateThreadRepartitionStrategy(KmerTable *tablesStats, char *content, size_t file_size, int nbThreads, bool printTheStrategy, int performantCoresMax, int *min, int *max, bool *priority) {
    // Dump stats
    if (printTheStrategy) {
        printf("Printing stats of file:\n");
        for (int j = 0; j < ASCII_COUNT; j++) {
            size_t count = tablesStats[j].count > 0 ? tablesStats[j].entries[0].count : 0;// the counter of occurence is in the first entry !
            printf("%d '%c': %zu\n", j, j, count);
        }
    }

    // This logic has been debugged with the help of Copilot as it is pretty easy to get wrong
    int counts[ASCII_COUNT];
    int nonZeroCharsCount = 0;
    for (int i = 0; i < ASCII_COUNT; i++) {
        int count = tablesStats[i].count > 0 ? tablesStats[i].entries[0].count : 0;
        counts[i] = count;
        if (count > 0)
            nonZeroCharsCount++;
    }
    // If there are less chars than threads, we have to decrease the number of threads
    if (nonZeroCharsCount < nbThreads) nbThreads = nonZeroCharsCount;

    size_t sums[nbThreads];
    int attributedPriorityCount = 0;// will be <= performantCoresMax

    size_t remainingChars = file_size;// equivalent to the sum of remaining counts
    int remainingThreads = nbThreads;
    int charIndex = 0, threadIndex = 0;

    // Continue while all chars have not been considered and we still have threads to assign for
    while (threadIndex < nbThreads && charIndex < ASCII_COUNT) {
        bool applyBiggerRangeFirstThread = threadIndex == 0 && nonZeroCharsCount > nbThreads;

        // Calculate dynamically for remaining work
        float concreteCharsPerThread = (float) remainingChars / remainingThreads;

        // Skip unused chars before assigning the min index
        while (charIndex < ASCII_COUNT && counts[charIndex] == 0)
            charIndex++;
        if (charIndex >= ASCII_COUNT) break;
        min[threadIndex] = charIndex;

        // Calculate the max
        size_t sum = 0;
        int lastNonZeroCountCharIndex = charIndex;// the last char where the count is not zero
        while (charIndex < ASCII_COUNT) {
            size_t count = counts[charIndex];

            // For the last thread, Take all chars until the last non zero index assigned to lastj
            if (threadIndex == nbThreads - 1) {
                sum += count;
                while (charIndex < ASCII_COUNT) {
                    sum += counts[charIndex];
                    if (counts[charIndex] > 0) {
                        lastNonZeroCountCharIndex = charIndex;
                    }
                    charIndex++;
                }
                break;
            }

            // If we are the first thread, just take chars until we go over the concreteCharsPerThread
            if (applyBiggerRangeFirstThread) {
                if (sum > concreteCharsPerThread)
                    break;
            } else {// Otherwise, we want to minimize the distance to concreteCharsPerThread when deciding if we go over or not
                if (sum > 0 && (fabsf(concreteCharsPerThread - sum) <= fabsf(concreteCharsPerThread - (sum + count))))
                    break;
            }
            sum += count;
            lastNonZeroCountCharIndex = charIndex;
            charIndex++;
        }
        max[threadIndex] = lastNonZeroCountCharIndex;
        sums[threadIndex] = sum;
        remainingChars -= sum;
        remainingThreads--;

        // Make it a priority thread or not
        priority[threadIndex] = false;
        if (attributedPriorityCount < performantCoresMax) {
            if (sums[threadIndex] > concreteCharsPerThread) {
                priority[threadIndex] = true;
                attributedPriorityCount++;
            }
        }
        threadIndex++;
    }

    nbThreads = threadIndex;// in case fewer threads we needed

    if (printTheStrategy) {
        printf("Printing multi-threading strategy on %d threads \n", nbThreads);

        for (int i = 0; i < nbThreads; i++) {
            printf("Thread %d: [%d '%c'; %d '%c'] -> %d different chars (%zu concrete chars - %.2f%%)%s\n",
                   i, min[i],
                   min[i],
                   max[i],
                   max[i],
                   max[i] - min[i] + 1,
                   sums[i],
                   ((float) sums[i]) / (float) file_size * 100,
                   priority[i] ? " - PRIORITY" : "");
        }

        exit(2);// we don't want to run if we print the strategy
    }
}

// Run the general algorithm, decide repartition of work for each thread, start them and wait them
void runKmersAlgo(char *content, size_t file_size, int k, KmerTable *tables, int nbThreads, bool printTheStrategy) {
    for (int i = 0; i < ASCII_COUNT; i++) {
        init_kmer_table(tables + i);
    }

    // If k is small or the file is too small, just run in the current thread
    if (k < MINIMUM_K_TO_ENABLE_MULTITHREADING || file_size < MINIMUM_FILE_SIZE_TO_ENABLE_MULTITHREADING) {
        if (printTheStrategy) {
            printf("The strategy is to NOT use multi-threading for this small file or small k.\n");
            return;
        } else {
            manageKmersOnFile(content, file_size, k, tables, 0, ASCII_COUNT - 1);
            return;
        }
    }

    int performantCoresMax = 4;// 4 performance cores
    // Run a first time with k=1 to get statistics about present chars
    KmerTable charPresenceStats[ASCII_COUNT];
    runKmersAlgo(content, file_size, 1, charPresenceStats, 1, false);

    int min[nbThreads], max[nbThreads];
    bool priority[nbThreads];
    calculateThreadRepartitionStrategy(charPresenceStats, content, file_size, nbThreads, printTheStrategy, performantCoresMax, min, max, priority);

    ThreadInfo tinfo[nbThreads];

    for (int i = 0; i < nbThreads; i++) {
        // Copy common read only fields
        tinfo[i].content = content;
        tinfo[i].file_size = file_size;
        tinfo[i].k = k;
        tinfo[i].tables = tables;

        tinfo[i].minCharIndex = min[i];
        tinfo[i].maxCharIndex = max[i];

        int result = pthread_create(&tinfo[i].thread_id, NULL, &manageKmersOnFileThreaded, tinfo + i);

        // Set affinity to a performant core if it's a priority work
        if (priority[i]) {
            cpu_set_t cpuset;
            int core_id = --performantCoresMax;// Use 3,2,1,0 for P-cores
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            pthread_setaffinity_np(tinfo[i].thread_id, sizeof(cpu_set_t), &cpuset);
        }
        if (result != 0) {
            printf("Error: creating thread %d \n", i);
            exit(2);
        }
    }

    for (int i = 0; i < nbThreads; i++) {
        pthread_join(tinfo[i].thread_id, NULL);
    }

    // Free tablesStats list
    for (int i = 0; i < ASCII_COUNT; i++) {
        free(charPresenceStats[i].entries);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <k> [--strategy]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    int k = atoi(argv[2]);
    if (k <= 0) {
        fprintf(stderr, "Error: k must be a positive integer.\n");
        return EXIT_FAILURE;
    }

    FILE *file = fopen(input_file, "r");
    if (!file) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }
    bool printStrategy = false;
    if (argc >= 4 && strcmp(argv[3], "--strategy") == 0) {
        printf("INPUT_FILE: %s\n", input_file);
        printStrategy = true;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);

    // Map the file to a memory region to be able to access it and move a pointer on it
    char *content = (char *) mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);

    // Init all tables
    KmerTable tables[ASCII_COUNT];
    runKmersAlgo(content, file_size, k, tables, DEFAULT_THREADS_NUMBER, printStrategy);

    // Show the results and free entries list as we go
    printf("Results:\n");
    for (int i = 0; i < ASCII_COUNT; i++) {
        KmerTable *table = tables + i;
        int count = table->count;
        for (int i = 0; i < count; i++) {
            printf("%.*s: %d\n", k, table->entries[i].kmer, table->entries[i].count);
        }
        free(table->entries);
    }
    // Unmap memory and close the file
    munmap(content, file_size);
    fclose(file);

    return 0;
}
