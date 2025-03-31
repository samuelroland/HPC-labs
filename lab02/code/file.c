#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void write_file(const char *filename, const char *content) {
    size_t size = strlen(content);
    FILE *file = fopen(filename, "w");
    fwrite(content, size, 1, file);
    fclose(file);
}

// Read a file given by filename and return a pointer to a
// dynamically allocated data with file content or NULL if it fails
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "File %s couldn't be opened", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    char *buff = malloc(size);
    if (!buff) {
        fprintf(stderr, "Could not allocate memory in read_text_file");
        return NULL;
    }

    fseek(file, 0, SEEK_SET);
    size_t read_count = fread(buff, sizeof(char), size, file);
    if (read_count != size) {
        printf("Content is %s\n", buff);
        fprintf(stderr, "Error: only read %lu bytes from %s, but file size is %lu.\n", read_count, filename, size);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return buff;
}
