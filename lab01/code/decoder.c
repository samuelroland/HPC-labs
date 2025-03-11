#include "decoder.h"
#include "const.h"
#include "encoder.h"
#include "fft.h"
#include <assert.h>
#include <math.h>
#include <sndfile-64.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

// First decoder implementation based on comparing each possible tone with possible sounds to detect
// how these tones are near of raw samples buffers for each button
//
// To calculate the "near score", I calculate the sum of absolute difference of float values in the buffer and extracted section.
// the more this score is low, the more it's probable this is the played tone

#define BTN_NOT_FOUND 100

float get_near_score(const float *audio_chunk, float *reference_tone) {
    double sum = 0;

    for (sf_count_t i = 0; i < TONE_SAMPLES_COUNT; i++) {
        sum += fabs(audio_chunk[i] - reference_tone[i]);
    }
    return sum / TONE_SAMPLES_COUNT;
}

// Calculate the average of scores to detect outliers
u_int8_t detect_button(const float *audio_chunk, float **freqs_buffers) {

#if DECODER_VARIANT == 1

    float min_distance_btn = 200;
    u_int8_t min_btn = BTN_NOT_FOUND;
    for (int i = 0; i < BTN_NUMBER; i++) {
        float score = get_near_score(audio_chunk, freqs_buffers[i]);
        printf("btn %d, score: %f\n", i, score);
        if (score < min_distance_btn) {
            min_distance_btn = score;
            min_btn = i;
        }
    }
    if (min_distance_btn < MIN_DISTANCE_MAX) {
        printf("Found button %d with score %f\n", min_btn, min_distance_btn);
        return min_btn;
    } else {
        printf("BTN_NOT_FOUND\n");
        return BTN_NOT_FOUND;
    }
#endif

#if DECODER_VARIANT == 2
    float f1, f2;
    find_main_frequencies(audio_chunk, TONE_SAMPLES_COUNT, SAMPLE_RATE, &f1, &f2);
    printf("Main frequencies: %.1f Hz and %.1f Hz\n", f1, f2);
    int col = -1;
    int line = -1;
    for (int i = 0; i < 4; i++) {
        float diff = LINES_FREQ[i] - f1;
        if (diff < FFT_FREQ_THRESHOLD && diff > -FFT_FREQ_THRESHOLD) {
            line = i;
            break;
        }
    }
    if (line == -1) {
        return BTN_NOT_FOUND;
    }
    for (int i = 0; i < 3; i++) {
        float diff = COLUMNS_FREQ[i] - f2;
        if (diff < FFT_FREQ_THRESHOLD && diff > -FFT_FREQ_THRESHOLD) {
            col = i;
            break;
        }
    }
    if (col == -1) {
        return BTN_NOT_FOUND;
    }
    u_int8_t found_btn_index = col / 3 + line % 3;

    return found_btn_index;
#endif
}

int8_t dtmf_decode(const float *audio_buffer, const sf_count_t samples_count, char **result_text) {
    float **freqs_buffers = generate_all_frequencies_buffers();
    // having only 1 tone letter would produce the maximum of letter count + 2 (ceil + \0)
    long maximum_text_buffer_size = samples_count / (TONE_SAMPLES_COUNT + SILENCE_SAMPLES_COUNT) + 2;

    *result_text = calloc(maximum_text_buffer_size, sizeof(char));
    printf("Allocated %ld for result_text", maximum_text_buffer_size);
    if (!*result_text) {
        printf("Error: couldn't allocate result text buffer\n");
        return 1;
    }

    sf_count_t cursor_index = 0;
    long tone_index = 0;
    long letter_index = 0;
    long tone_repetition = 0;
    bool searching_next_tone = false;//we didn't find the first tone at the very first
    u_int8_t current_btn = BTN_NOT_FOUND;

    // We progress by trying with SHORT_BREAK_SAMPLES_COUNT step, if we don't find a another tone for the same letter
    // we know we reached the end of a letter, so we can skip SILENCE_SAMPLES_COUNT - SHORT_BREAK_SAMPLES_COUNT
    // and except if we reached the end, we are sure to find a tone.
    while (cursor_index < samples_count) {
        printf("Searching btn on chunk at cursor_index = %lu, with tone_index = %ld and tone_repetition = %ld\n", cursor_index, tone_index, tone_repetition);
        u_int8_t found_btn = detect_button(audio_buffer + cursor_index, freqs_buffers);
        if (found_btn == BTN_NOT_FOUND) {
            if (tone_index == 0) {
                printf("First %d samples must be a tone ! The tone was not detected.\n", TONE_SAMPLES_COUNT);
            }
            if (tone_repetition == 0) {
                printf("Skipping silence\n");
                cursor_index += SILENCE_SAMPLES_COUNT;// or maybe SHORT_BREAK_SAMPLES_COUNT ?
                long tone_repetition = 0;
                continue;
            }

            // printf("\n>> SHOULD found letter: under btn %d, with %lu repetition\n\n", current_btn, tone_repetition);
            // that's a silence, so we just finished must calculate the current letter based on tone_repetition
            char c = LETTERS_BY_BTN[current_btn][tone_repetition - 1];
            if (c != '\0') {
                (*result_text)[letter_index++] = c;
                printf("\n>> FOUND LETTER: %c: under btn %d, with %lu repetition\n\n", c, current_btn, tone_repetition);
            } else {
                printf("\n>> FOUND INVALID CHAR: under btn %d, with %lu repetition\n\n", current_btn, tone_repetition);
            }

            // jumping at the end of the silence duration to search for next tone after that
            cursor_index += SILENCE_SAMPLES_COUNT - SHORT_BREAK_SAMPLES_COUNT;
            // Resetting state for a future letter
            tone_repetition = 0;
            searching_next_tone = false;
            tone_index++;
        } else {
            searching_next_tone = true;
            current_btn = found_btn;// TODO: what to do if we find another tone after short break ?
            tone_repetition++;
            tone_index++;
            cursor_index += TONE_SAMPLES_COUNT + SHORT_BREAK_SAMPLES_COUNT;
        }
    }

    // Manage last letter
    (*result_text)[letter_index++] = LETTERS_BY_BTN[current_btn][tone_repetition - 1];

    (*result_text)[letter_index] = '\0';

    return 0;
}
