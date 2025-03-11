#include "audio.h"
#include "decoder.h"
#include "encoder.h"
#include "file.h"
#include <sndfile-64.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *prog) {
    printf("Usage :\n  %s encode input.txt output.wav\n  %s decode input.wav\n", prog, prog);
}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "encode") == 0) {
        char *text = read_file(argv[2]);
        float *audio_result = NULL;
        if (text) {
            sf_count_t samples_count = dtmf_encode(text, &audio_result);
            if (samples_count < 1) {
                printf("Error on dtmf encoding %lu\n", samples_count);
            } else {
                write_wav_file(argv[3], audio_result, samples_count);
                // TODO: manage write errors
                printf("Successfully written encoded audio in %s", argv[3]);
            }
        }
        free(text);
        free(audio_result);
    } else if (strcmp(argv[1], "decode") == 0) {
        float *audio_freqs = NULL;
        sf_count_t samples_count = 0;
        char *result_text = NULL;
        int result = read_wav_file(argv[2], &audio_freqs, &samples_count);
        if (result == 0 && samples_count > 0) {
            if (audio_freqs != NULL) {
                int decode_result = dtmf_decode(audio_freqs, samples_count, &result_text);
                if (decode_result != 0) {
                    printf("Error on dtmf decoding for %lu samples on %s\n", samples_count, argv[2]);
                } else {
                    printf("Text decoded to: %s\n", result_text);
                }
            } else {
                printf("Failed to read file");
            }
        } else {
            printf("Error somewhere\n");
        }
        free(result_text);
        free(audio_freqs);
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
