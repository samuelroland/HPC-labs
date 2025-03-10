// File module to easily write and read text files
#ifndef FILE_H
#define FILE_H

void write_text_file(const char *filename, const char *content);

char *read_text_file(const char *filename);

#endif// !FILE_H
