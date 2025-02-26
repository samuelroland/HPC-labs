// Mix 2 frequencies following this formula: s(t) = A × (sin(2πflow t) + sin(2πfhigh t))
#include "codec.h"
#include <cstddef>
#include <math.h>
#include <string.h>
float mix_frequency(float first, float second) {
    return AMPLITUDE * (sin(2 * M_PI * first * SAMPLING_TIME) + sin(2 * M_PI * second * SAMPLING_TIME));
}

// Cell number starts at 0 !!
float cell_number_to_mix(char cell_number) {
    return mix_frequency(LINES_FREQ[cell_number % 3], COLUMNS_FREQ[cell_number / 3]);
}

// Convert given letter char to cell number, because it's an irregular schema of sometimes 3 groups, sometimes 4, sometimes outside of a-z
char letter_to_cell_number(char c) {
    if (c >= 'a' && c <= 'r') {
        return (c - 'a') / 3 + 1;// + 1 because the cell 0 doesn't have any letters
    } else if (c >= 's' && c <= 'y') {
        return (c - 'a' - 1) / 3 + 1;// -1 because s to y are shifted to left, + 1 same reason as above
    } else if (c == 'z') {
        return 8;// 9th cell
    } else if (c == '.' || c == '?' || c == '!') {
        return 9;// 10th cell
    } else if (c == ' ') {
        return 10;// 11th cell
    }
    return -1;
}


// Convert a given char to a floating value as the final frequency
float char_to_sound(char c) {
    // Digits encoding
    if (c >= '0' && c <= '9') {
        char n = c - '0';
        if (n == 0) {
            return mix_frequency(LINES_FREQ[3], COLUMNS_FREQ[1]);
        } else {
            cell_number_to_mix(n - 1);
        }
    } else if (c >= 'a' && c <= 'z') {
        return cell_number_to_mix(letter_to_cell_number(c));
    }
    return -1;
}

void dtmf_encode(const char *text, float *audio_result) {
    size_t text_length = strlen(text);
    size_t audio_size = text_length * 1212;
}

void dtmf_decode(const float *audio_buffer, char *result_text) {
}
