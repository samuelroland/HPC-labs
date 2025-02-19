// Our DTMF codec implementation
#ifndef CODEC_H
#define CODEC_H

// DTMF encoding from given text and store results in audio_result
// It doesn't free text but allocates audio_result to the correct size
void dtmf_encode(const char *text, float *audio_result);

void dtmf_decode(const float *audio_buffer, char *result_text);

#endif// !CODEC_H
