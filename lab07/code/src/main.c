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
    size_t fileSize;
    int k;
    KmerTable *tables;
} ThreadInfo;

// Researching and inserting for a specific set of chars between given min and max index
// this can be run in a separated thread or not, depending on the needs
void manageKmersOnFile(ThreadInfo *info) {
    char *content = info->content;
    size_t file_size = info->fileSize;
    int k = info->k;
    KmerTable *tables = info->tables;
    int min = info->minCharIndex;
    int max = info->maxCharIndex;

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
    manageKmersOnFile(info);
    return NULL;
}

void runKmersAlgo(char *content, size_t file_size, int k, KmerTable *tables, int nbThreads) {
    for (int i = 0; i < ASCII_COUNT; i++) {
        init_kmer_table(tables + i);
    }

    // Static cut of ASCII space
    int charsPerThread = ASCII_COUNT / nbThreads;

    ThreadInfo tinfo[nbThreads];
    for (int i = 0; i < nbThreads; i++) {
        // Copy common read only fields
        tinfo[i].content = content;
        tinfo[i].fileSize = file_size;
        tinfo[i].k = k;
        tinfo[i].tables = tables;

        tinfo[i].minCharIndex = i * charsPerThread;
        if (i == nbThreads - 1) {                   // last has the remaining, in case the rounding has made the last thread just a bit more
            tinfo[i].maxCharIndex = ASCII_COUNT - 1;// the last one is 127
        } else {
            tinfo[i].maxCharIndex = (i + 1) * charsPerThread - 1;
        }

        int result = pthread_create(&tinfo[i].thread_id, NULL, &manageKmersOnFileThreaded, tinfo + i);
        if (result != 0) {
            printf("Error: creating thread %d \n", i);
            exit(2);
        }
    }

    for (int i = 0; i < nbThreads; i++) {
        pthread_join(tinfo[i].thread_id, NULL);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <k>\n", argv[0]);
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

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);

    // Map the file to a memory region to be able to access it and move a pointer on it
    char *content = (char *) mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);

    // Init all tables
    KmerTable tables[ASCII_COUNT];
    int nbThreads = 10;
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
