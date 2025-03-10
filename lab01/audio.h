// Little audio module using libsndfile to write and read wav files in float arrays
#ifndef AUDIO_H
#define AUDIO_H

// Use libsndfile to write an array of audio frequency values to encode in the .wav file
// It doesn't do any dynamic allocations
#include <sndfile.h>

void write_wav_file(const char *filename, const float *freqs, sf_count_t samples_count);

// Use libsndfile to write an array of audio frequency values to encode in the .wav file
// It dynamically allocates freqs
void read_wav_file(const char *filename, const float *freqs);

#endif// !AUDIO_H
