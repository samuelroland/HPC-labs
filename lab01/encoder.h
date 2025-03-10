// Our DTMF codec implementation
#ifndef ENCODEC_H
#define ENCODEC_H

#include <sys/types.h>

// Repeated button struct to store both values together
typedef struct {
    u_int8_t btn_index; // index of the button from 0 to 11 for the 12 buttons
    u_int8_t repetition;// number of times this frequence must be repeated depending on the letter
} RepeatedBtn;

// DTMF encoding from given text and store results in audio_result
// It expects the text to encode and pointer to a pointer on float set to NULL
// It will allocate audio_result to the correct size and fill the float values
// It returns the number of samples allocated
int64_t dtmf_encode(const char *text, float **audio_result);

// Utils functions exposed for testing
RepeatedBtn char_to_repeated_btn(char c);
#endif
