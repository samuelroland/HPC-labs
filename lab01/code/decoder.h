// Our DTMF codec implementation
#ifndef DECODER_H
#define DECODER_H

#include "const.h"
#include <sndfile-64.h>
#include <sys/types.h>

#define LETTERS_BY_BTN (char[BTN_NUMBER][6]){"1", "2abc", "3def", "4ghi", "5jkl", "6mno", "7pqrs", "8tuv", "9wxyz", "0.!?", "0 "}

int8_t dtmf_decode(const float *audio_buffer, const sf_count_t samples_count, char **result_text);

#endif
