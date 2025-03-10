// Our DTMF codec implementation
#ifndef DECODER_H
#define DECODER_H

#include <sys/types.h>
int8_t dtmf_decode(const float *audio_buffer, char *result_text);

#endif
