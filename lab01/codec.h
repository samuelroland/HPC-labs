// Our DTMF codec implementation
#ifndef CODEC_H
#define CODEC_H

#define LINES_FREQ (int[]){697, 770, 852, 941}
#define COLUMNS_FREQ (int[]){1209, 1336, 1477}
#define AMPLITUDE 100
#define SAMPLING_TIME 10               // number of ms for a sample
#define SAMPLE_RATE (1 / SAMPLING_TIME)// the audio frequency -> number of sample per second
// 1. Une durée d’un son de 0.2 secondes par caractère.
#define CHAR_SOUND_DURATION 0.2
// 2. Une pause de 0.2 secondes entre deux caractères.
#define CHAR_SOUND_BREAK 0.2
// 3. Une pause de 0.05 secondes entre plusieurs pressions pour un même caractère.
#define SAME_CHAR_SOUND_BREAK 0.05

// DTMF encoding from given text and store results in audio_result
// It doesn't free text but allocates audio_result to the correct size
void dtmf_encode(const char *text, float *audio_result);

void dtmf_decode(const float *audio_buffer, char *result_text);

#endif// !CODEC_H
