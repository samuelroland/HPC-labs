#ifndef CONST_H
#define CONST_H

#define LINES_FREQ (int[]){697, 770, 852, 941}
#define COLUMNS_FREQ (int[]){1209, 1336, 1477}
#define BTN_NUMBER 11    // 11 buttons (the last one is not used)
#define AMPLITUDE 1      // TODO tester avec 1
#define SAMPLE_RATE 44100// the audio frequency -> number of sample per second
// 1. Une durée d’un son de 0.2 secondes par caractère.
#define TONE_SAMPLES_COUNT 8820// 44100 * 0.2 = 8820
// 2. Une pause de 0.2 secondes entre deux caractères.
#define SILENCE_SAMPLES_COUNT 8820// same as above
// 3. Une pause de 0.05 secondes entre plusieurs pressions pour un même caractère.
#define SHORT_BREAK_SAMPLES_COUNT 2205// 44100 * 0.05 = 2205
#define SUPPORTED_ALPHABET "0123456789abcdefghijklmnopqrstuvwxyz.!?# "

#endif
