#include "decoder.h"
#include "const.h"
#include "encoder.h"
#include <assert.h>
#include <math.h>
#include <omp.h>
#include <sndfile.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// First decoder implementation based on comparing each possible tone with possible sounds to detect
// how these tones are near of raw samples buffers for each button
//
// To calculate the "near score", I calculate the sum of absolute difference of float values in the buffer and extracted section.
// the more this score is low, the more it's probable this is the played tone

#define BTN_NOT_FOUND 100

inline float get_near_score(const float *audio_chunk, float *reference_tone) {
    double sum = 0;
    for (sf_count_t i = 0; i < TONE_SAMPLES_COUNT; i++) {
        sum += fabs(audio_chunk[i] - reference_tone[i]);
    }
    return sum / TONE_SAMPLES_COUNT;
}

// Calculate the average of scores to detect outliers
inline uint8_t detect_button(const float *audio_chunk, float **freqs_buffers) {

    float min_distance_btn = 200;
    u_int8_t min_btn = BTN_NOT_FOUND;

#pragma omp parallel for
    for (int i = 0; i < BTN_NUMBER; i++) {
        float score = get_near_score(audio_chunk, freqs_buffers[i]);
#pragma omp critical
        {
            if (score < min_distance_btn) {
                min_distance_btn = score;
                min_btn = i;
            }
        }
    }

    if (min_distance_btn < MIN_DISTANCE_MAX) {
        LOG("Found button %d with score %f\n", min_btn, min_distance_btn);
        return min_btn;
    } else {
        LOG("BTN_NOT_FOUND\n");

        return BTN_NOT_FOUND;
    }

    // Variant 2 with FFT deleted
}

int8_t dtmf_decode(const float *audio_buffer, const sf_count_t samples_count, char **result_text) {

    float **freqs_buffers = generate_all_frequencies_buffers();
    // having only 1 tone letter would produce the maximum of letter count + 2 (ceil + \0)
    long maximum_text_buffer_size = samples_count / (TONE_SAMPLES_COUNT + SILENCE_SAMPLES_COUNT) + 2;

    *result_text = calloc(maximum_text_buffer_size, sizeof(char));
    if (!*result_text) {
        LOG("Error: couldn't allocate result text buffer\n");
        return 1;
    }

    sf_count_t cursor_index = 0;
    long letter_index = 0;
    long tone_repetition = 0;
    bool searching_next_tone = false;//we didn't find the first tone at the very first
    uint8_t current_btn = BTN_NOT_FOUND;

    // We progress by trying with SHORT_BREAK_SAMPLES_COUNT step, if we don't find a another tone for the same letter
    // we know we reached the end of a letter, so we can skip SILENCE_SAMPLES_COUNT - SHORT_BREAK_SAMPLES_COUNT
    // and except if we reached the end, we are sure to find a tone.
    while (cursor_index < samples_count) {
        // LOG("Searching btn on chunk at cursor_index = %lu, with tone_index = %ld and tone_repetition = %ld\n", cursor_index, tone_index, tone_repetition);
        uint8_t found_btn = detect_button(audio_buffer + cursor_index, freqs_buffers);
        if (found_btn == BTN_NOT_FOUND) {

            if (tone_repetition == 0) {
                LOG("Skipping silence\n");
                cursor_index += SILENCE_SAMPLES_COUNT;// or maybe SHORT_BREAK_SAMPLES_COUNT ?
                long tone_repetition = 0;
                continue;
            }

            // that's a silence, so we just finished must calculate the current letter based on tone_repetition
            char c = LETTERS_BY_BTN[current_btn][tone_repetition - 1];
            if (c != '\0') {
                (*result_text)[letter_index++] = c;
                LOG(">> FOUND LETTER: %c: under btn %d, with %lu repetition\n", c, current_btn, tone_repetition);
            } else {
                LOG("\n>> FOUND INVALID CHAR: under btn %d, with %lu repetition\n", current_btn, tone_repetition);
            }

            // jumping at the end of the silence duration to search for next tone after that
            cursor_index += SILENCE_SAMPLES_COUNT - SHORT_BREAK_SAMPLES_COUNT;
            // Resetting state for a future letter
            tone_repetition = 0;
            searching_next_tone = false;
            current_btn = BTN_NOT_FOUND;
        } else {
            // Changed to another tone so this is the end of the letter and the start of the next one
            if (current_btn != BTN_NOT_FOUND && current_btn != found_btn) {
                char c = LETTERS_BY_BTN[current_btn][tone_repetition - 1];
                if (c != '\0') {
                    (*result_text)[letter_index++] = c;
                    LOG("\n>> FOUND LETTER: %c: under btn %d, with %lu repetition\n\n", c, current_btn, tone_repetition);
                }
                cursor_index += TONE_SAMPLES_COUNT + SHORT_BREAK_SAMPLES_COUNT;
                current_btn = found_btn;
                tone_repetition = 1;// for next round
            } else {
                searching_next_tone = true;
                current_btn = found_btn;// TODO: what to do if we find another tone after short break ?
                tone_repetition++;
                cursor_index += TONE_SAMPLES_COUNT + SHORT_BREAK_SAMPLES_COUNT;
            }
        }
    }

    // Manage last letter
    (*result_text)[letter_index++] = LETTERS_BY_BTN[current_btn][tone_repetition - 1];

    (*result_text)[letter_index] = '\0';

    return 0;
}
