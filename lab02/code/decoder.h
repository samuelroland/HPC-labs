// Our DTMF codec implementation
#ifndef DECODER_H
#define DECODER_H

#include "const.h"
#include <sndfile.h>

#define MIN_DISTANCE_MAX 0.8// threshold for the variant 1 of distance of float diff allowed to detect silence or button
#define FFT_FREQ_THRESHOLD 5// 5 hz of diff is allowed

// Note: this is now defined in the CMakeLists.txt to generate 2 targets.
// #define DECODER_VARIANT 1// 1 is float buffer comparison, 2 is via fft

#define LETTERS_BY_BTN (char[BTN_NUMBER][6]){"1", "2abc", "3def", "4ghi", "5jkl", "6mno", "7pqrs", "8tuv", "9wxyz", "#.!?", "0 "}

int8_t dtmf_decode(const float *audio_buffer, const sf_count_t samples_count, char **result_text);

#endif
