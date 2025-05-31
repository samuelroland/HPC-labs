#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

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

void *manageKmersOnFileThreaded(void *args) {
    ThreadInfo *info = (ThreadInfo *) args;
    manageKmersOnFile(info->content, info->file_size, info->k, info->tables, info->minCharIndex, info->maxCharIndex);
    return NULL;
}

void runKmersAlgo(char *content, size_t file_size, int k, KmerTable *tables, int nbThreads) {
    for (int i = 0; i < ASCII_COUNT; i++) {
        init_kmer_table(tables + i);
    }

    // If k is small, just run in single thread
    if (k <= 2) {
        manageKmersOnFile(content, file_size, k, tables, 0, ASCII_COUNT - 1);
        return;
    }
    // Otherwise start nbThreads or less

    // Run a first time with k=1 to get statistics about present chars
    KmerTable tablesStats[ASCII_COUNT];
    runKmersAlgo(content, file_size, 1, tablesStats, 1);

    // DUmp stats
    for (int j = 0; j < ASCII_COUNT; j++) {
        size_t count = tablesStats[j].count > 0 ? tablesStats[j].entries[0].count : 0;// the counter of occurence is in the first entry !
        printf("%d '%c': %zu\n", j, j, count);
    }

    int counts[ASCII_COUNT];// counts of each char in the file
    int nonZeroCharsCount = 0;
    for (int i = 0; i < ASCII_COUNT; i++) {
        int count = tablesStats[i].count > 0 ? tablesStats[i].entries[0].count : 0;// the counter of occurence is in the first entry !
        counts[i] = count;
        if (count > 0)
            nonZeroCharsCount++;
    }

    // Adjust number of threads to run
    if (nonZeroCharsCount < nbThreads) {
        nbThreads = nonZeroCharsCount;
    }

    float concreteCharsPerThread = (float) file_size / nbThreads;
    int min[nbThreads];    // min indexes for each thread
    int max[nbThreads];    // same for max
    size_t sums[nbThreads];// sums of concrete chars assigned per thread, only for printing purpose

    int j = 0;// index of ASCII_COUNT

    size_t lastNonZeroCountIndex = 0;
    for (int i = 0; i < nbThreads; i++) {
        // Skip holes of unused chars, to define the next min value on a used char
        while (j < ASCII_COUNT && counts[j] == 0) {
            j++;
        }
        min[i] = j;

        size_t sum = 0;
        while (j < ASCII_COUNT) {
            size_t count = tablesStats[j].count > 0 ? tablesStats[j].entries[0].count : 0;// the counter of occurence is in the first entry !
            if (count > 0) {
                lastNonZeroCountIndex = j;
                // printf("Found a lastNonZeroCountIndex at %d\n", j);
            }
            // printf("count for char %d is %zu\n", j, count);

            size_t sumPlusCount = sum + count;

            // printf("file_size %zu, sumPlusCount %zu, concreteCharsPerThread %f, sum %zu, count %zu\n", file_size, sumPlusCount, concreteCharsPerThread, sum, count);
            // If the difference to the sum when adding the count is bigger than without adding it, don't add it
            if (concreteCharsPerThread - sum <= sumPlusCount - concreteCharsPerThread) {
                break;
            }

            sum = sumPlusCount;
            j++;
        }
        sums[i] = sum;

        max[i] = j - 1;
        if (i != nbThreads - 1)
            min[i + 1] = j;
    }

    max[nbThreads - 1] = lastNonZeroCountIndex;

    printf("Printing multi-threading strategy on %d threads\n", nbThreads);

    for (int i = 0; i < nbThreads; i++) {
        printf("Thread %d: [%d '%c'; %d '%c'] -> total of %d different chars (%zu concrete chars) (%.2f%%)\n", i, min[i], min[i], max[i], max[i], max[i] - min[i] + 1, sums[i], ((float) sums[i]) / (float) file_size * 100);
    }
    exit(2);

    // Static cut of ASCII space
    int charsPerThread = ASCII_COUNT / nbThreads;

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
        free(tablesStats[i].entries);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <k>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    printf("INPUT_FILE: %s\n", input_file);
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

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);

    // Map the file to a memory region to be able to access it and move a pointer on it
    char *content = (char *) mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);

    // Init all tables
    KmerTable tables[ASCII_COUNT];
    int nbThreads = 20;
    runKmersAlgo(content, file_size, k, tables, nbThreads);

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
