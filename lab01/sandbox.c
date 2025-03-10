// Sandbox to manually test the results of some functions

#include "audio.h"
#include "const.h"
#include "encoder.h"
#include "file.h"
#include <sndfile-64.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    for (int i = 0; i < strlen(SUPPORTED_ALPHABET); i++) {
        char c = SUPPORTED_ALPHABET[i];
        RepeatedBtn btn = char_to_repeated_btn(c);
        printf("char: %c: btn index = %d, repetition = %d\n", c, btn.btn_index, btn.repetition);
    }
}
