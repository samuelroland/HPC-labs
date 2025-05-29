#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

#define MAX_KMER 100

typedef struct {
    int count;
    char kmer[MAX_KMER] __attribute__((aligned(8)));
} KmerEntry;

typedef struct {
    KmerEntry *entries;
    int count;
    int capacity;
} KmerTable;

#define LETTERS_COUNT 26
#define NUMBERS_COUNT 10
typedef struct {
    KmerTable letters[LETTERS_COUNT];// 26 KmerTable for words starting with the 26 alphabet letters (case insensitive)
    KmerTable numbers[NUMBERS_COUNT];// 10 KmerTable words starting with for numbers
    KmerTable rest;                  // all other words (starting with special chars)
} KmerTables;

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

            entries[i].count++;
            return;
        next_round:
        }
        // TODO: fix warning about C23 extension ???
    } else {
        // normal execution for k < 4, compare each character one by one
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

    memcpy(table->entries[table->count].kmer, kmer, k);
    table->entries[table->count].kmer[k] = '\0';
    table->entries[table->count].count = 1;
    table->count++;
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
    long file_size = ftell(file);

    // Map the file to a memory region to be able to access it and move a pointer on it
    char *content = (char *) mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);

    // Init all tables
    KmerTables tables;
    for (int i = 0; i < LETTERS_COUNT; i++) {
        init_kmer_table(tables.letters + i);
    }
    for (int i = 0; i < NUMBERS_COUNT; i++) {
        init_kmer_table(tables.numbers + i);
    }
    init_kmer_table(&tables.rest);

    // Start extracting, searching k-mer and saving them
    for (long i = 0; i <= file_size - k; i++) {

        // Pick among one of the 37 available subtable
        char firstChar = tolower(content[i]);
        KmerTable *subtable = NULL;
        if (firstChar >= '0' && firstChar <= '9') {
            subtable = tables.numbers + (firstChar - '0');// get the table of the first digit
        } else if (firstChar >= 'a' && firstChar <= 'z') {
            subtable = tables.letters + (firstChar - 'a');// get the table of the first letter
        } else {
            subtable = &tables.rest;
        }

        // Research in this subtable or insert it at the end
        add_kmer(subtable, content + i, k);
    }

    // Show the results and free entries list as we go
    printf("Results:\n");
    for (int i = 0; i < LETTERS_COUNT; i++) {
        KmerTable *table = tables.letters + i;
        int count = table->count;
        for (int i = 0; i < count; i++) {
            printf("%s: %d\n", table->entries[i].kmer, table->entries[i].count);
        }
        free(table->entries);
    }
    for (int i = 0; i < NUMBERS_COUNT; i++) {
        KmerTable *table = tables.numbers + i;
        int count = table->count;
        for (int i = 0; i < count; i++) {
            printf("%s: %d\n", table->entries[i].kmer, table->entries[i].count);
        }
        free(table->entries);
    }
    KmerTable *table = &tables.rest;
    int count = tables.rest.count;
    for (int i = 0; i < count; i++) {
        KmerEntry *entry = &table->entries[i];
        printf("%s: %d\n", entry->kmer, entry->count);
    }
    free(tables.rest.entries);

    // Unmap memory and close the file
    munmap(content, file_size);
    fclose(file);

    return 0;
}
