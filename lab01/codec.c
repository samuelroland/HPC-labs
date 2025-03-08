#include "codec.h"
#include "audio.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// Mix 2 frequencies following this formula: s(t) = A × (sin(2πflow t) + sin(2πfhigh t))
float mix_frequency(float first, float second, int t) {
    return AMPLITUDE * (sin(2 * M_PI * first * t) + sin(2 * M_PI * second * t));
}

// Convert given letter char to cell number, because it's an irregular schema of sometimes 3 groups, sometimes 4, sometimes outside of a-z
RepeatedBtn char_to_repeated_btn(char c) {
    // Managing digits first
    if (c >= '0' && c <= '9') {
        char n = c - '0';
        if (n == 0) {
            return (RepeatedBtn) {.btn_index = 10, .repetition = 1};
        } else {
            return (RepeatedBtn) {.btn_index = n - 1, .repetition = 1};
        }
    }

    // And all letters and special chars after
    if (c >= 'a' && c <= 'r') {
        return (RepeatedBtn) {.btn_index = (c - 'a') / 3 + 1, .repetition = (c - 'a') % 3 + 1};// + 1 because the cell 0 doesn't have any letters
    } else if (c >= 's' && c <= 'y') {
        return (RepeatedBtn) {.btn_index = (c - 'a' - 1) / 3 + 1, .repetition = ((c - 'a') - 1) % 3 + 1};// -1 because s to y are shifted to left, + 1 same reason as above
    } else if (c == 'z') {
        return (RepeatedBtn) {.btn_index = 8, .repetition = 4};// 9th cell
    } else if (c == '.') {
        return (RepeatedBtn) {.btn_index = 9, .repetition = 1};// 10th cell
    } else if (c == '!') {
        return (RepeatedBtn) {.btn_index = 9, .repetition = 2};// 10th cell
    } else if (c == '?') {
        return (RepeatedBtn) {.btn_index = 9, .repetition = 3};// 10th cell
    } else if (c == '#') {
        return (RepeatedBtn) {.btn_index = 9, .repetition = 1};// 10th cell
    } else if (c == ' ') {
        return (RepeatedBtn) {.btn_index = 10, .repetition = 1};// 11th cell
    }
    return (RepeatedBtn) {.btn_index = -1, .repetition = 0};// invalid character
}

float **generate_all_frequencies_buffers() {
    float **buffers = malloc(BTN_NUMBER * sizeof(float *));
    for (int i = 0; i < BTN_NUMBER; i++) {
        buffers[i] = malloc(SAMPLES_NUMBER_PER_BTN * sizeof(float));
        float first = LINES_FREQ[i % 3];
        float second = COLUMNS_FREQ[i / 3];
        for (int j = 0; j < SAMPLES_NUMBER_PER_BTN; j++) {
            buffers[i][j] = mix_frequency(first, second, j / SAMPLE_RATE);// TODO: / SAMPLE_RATE or by SAMPLES_NUMBER_PER_BTN ??
        }
    }
    return buffers;
}

int8_t dtmf_encode(const char *text, float *audio_result) {
    size_t text_length = strlen(text);
    RepeatedBtn *repeated_btns_for_text = calloc(text_length, sizeof(RepeatedBtn));
    size_t tones_count = 0;               // number of tones to play (the 's' key will generate 5 tones of the 8th button)
    size_t short_breaks_count = 0;        // number of short breaks between repeated tones
    size_t breaks_count = text_length - 1;// number of breaks, there is no break at the end

    for (int i = 0; i < text_length; i++) {
        char c = text[i];
        RepeatedBtn btn = char_to_repeated_btn(c);
        assert(btn.btn_index > 0);
        tones_count += btn.repetition;
        short_breaks_count += btn.repetition - 1;
        repeated_btns_for_text[i] = btn;
    }

    size_t audio_size = tones_count * SAMPLES_NUMBER_PER_BTN + short_breaks_count * SHORT_BREAK_SAMPLES_COUNT + breaks_count * SAMPLES_NUMBER_PER_BTN;
    audio_result = calloc(audio_size, sizeof(float));
    if (!audio_result) return -1;

    // Prepare audio_result by copying floats array from generate_all_frequencies_buffers() to easily apply tones
    float **freqs_buffers = generate_all_frequencies_buffers();
    float *cursor = audio_result;
    for (int i = 0; i < text_length; i++) {
        RepeatedBtn *btn = &repeated_btns_for_text[i];
        float *src = freqs_buffers[btn->btn_index];
        for (u_int8_t j = 0; j < btn->repetition; j++) {
            if (j > 0) cursor += SHORT_BREAK_SAMPLES_COUNT;// skipping short break silence between repeated tones
            memcpy(cursor, src, SAMPLES_NUMBER_PER_BTN);
            cursor += SAMPLES_NUMBER_PER_BTN;// skipping just copied values
        }
        cursor += SILENCE_SAMPLES_COUNT;// skipping the silence after the complete letter
    }

    // Free everything allocated locally
    for (int i = 0; i < BTN_NUMBER; i++) {
        free(freqs_buffers[i]);
    }
    free(freqs_buffers);

    return 0;
}

int8_t dtmf_decode(const float *audio_buffer, char *result_text) {
}
