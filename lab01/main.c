#include "audio.h"
#include "codec.h"
#include "file.h"
#include <stddef.h>
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
        char *text = read_text_file(argv[1]);
        float *audio_result = NULL;
        if (text) {
            dtmf_encode(text, audio_result);
            // TODO: manage encoding errors
            write_wav_file(argv[1], audio_result);
            // TODO: manage write errors
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
