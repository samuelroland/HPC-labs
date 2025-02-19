#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void write_text_file(const char *filename, const char *content) {
    size_t size = strlen(content);
    FILE *file = fopen(filename, "w");
    fwrite(content, size, 1, file);
}

// Read a file given by filename and return a pointer to a
// dynamically allocated data with file content or NULL if it fails
char *read_text_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    char *buff = malloc(size);
    if (!buff) return NULL;

    size_t read_count = fread(buff, 1, size, file);
    if (read_count != size) {
        fprintf(stderr, "Error: only read %lu bytes from %s, but file size is %lu.", read_count, filename, size);
        return NULL;
    }
    return buff;
}
