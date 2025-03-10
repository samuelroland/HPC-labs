// Our DTMF codec implementation
#ifndef CODEC_H
#define CODEC_H

#include <sys/types.h>
#define LINES_FREQ (int[]){697, 770, 852, 941}
#define COLUMNS_FREQ (int[]){1209, 1336, 1477}
#define BTN_NUMBER 11    // 11 buttons (the last one is not used)
#define AMPLITUDE 1      // TODO tester avec 1
#define SAMPLE_RATE 44100// the audio frequency -> number of sample per second
// 1. Une durée d’un son de 0.2 secondes par caractère.
#define CHAR_SOUND_DURATION 0.2
#define SAMPLES_NUMBER_PER_BTN 8820// (SAMPLE_RATE * CHAR_SOUND_DURATION) = 44100 * 0.2 = 8820
// 2. Une pause de 0.2 secondes entre deux caractères.
#define CHAR_SOUND_BREAK 0.2
#define SILENCE_SAMPLES_COUNT 8820// same as above
// 3. Une pause de 0.05 secondes entre plusieurs pressions pour un même caractère.
#define SAME_CHAR_SOUND_BREAK 0.05
#define SHORT_BREAK_SAMPLES_COUNT 2205// (SAMPLE_RATE * SAME_CHAR_SOUND_BREAK)// 44100 * 0.05 = 2205
#define SUPPORTED_ALPHABET "0123456789abcdefghijklmnopqrstuvwxyz.!?# "

// Repeated button struct to store both values together
typedef struct {
    char btn_index;     // index of the button from 0 to 11 for the 12 buttons
    u_int8_t repetition;// number of times this frequence must be repeated depending on the letter
} RepeatedBtn;

// DTMF encoding from given text and store results in audio_result
// It expects the text to encode and pointer to a pointer on float set to NULL
// It will allocate audio_result to the correct size and fill the float values
// It returns the number of samples allocated
int64_t dtmf_encode(const char *text, float **audio_result);

int8_t dtmf_decode(const float *audio_buffer, char *result_text);

// Utils functions exposed for testing
RepeatedBtn char_to_repeated_btn(char c);
#endif// !CODEC_H
