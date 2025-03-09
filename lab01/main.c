#include "audio.h"
#include "codec.h"
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
        if (argc != 4) {
            print_usage(argv[0]);
            return 1;
        }
        char *text = read_text_file(argv[2]);
        float *audio_result = NULL;
        if (text) {
            sf_count_t samples_count = dtmf_encode(text, &audio_result);
            // TODO: manage encoding errors
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

    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
