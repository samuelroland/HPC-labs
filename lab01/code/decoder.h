// Our DTMF codec implementation
#ifndef DECODER_H
#define DECODER_H

#include <sndfile-64.h>
#include <sys/types.h>
int8_t dtmf_decode(const float *audio_buffer, sf_count_t samples_count, char *result_text);

#endif
