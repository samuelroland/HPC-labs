#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define MAX_KMER 100

typedef struct {
    int count;
    char kmer[MAX_KMER];
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
    for (int i = 0; i < table->count; i++) {
        if (memcmp(table->entries[i].kmer, kmer, k) == 0) {
            table->entries[i].count++;
            return;
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
