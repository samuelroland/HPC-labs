#include "decoder.h"
#include "const.h"
#include "encoder.h"
#include <assert.h>
#include <math.h>
#include <sndfile-64.h>
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

int get_near_score(float *audio_chunk, float *reference_tone) {
    double sum = 0;
    for (sf_count_t i = 0; i < TONE_SAMPLES_COUNT; i++) {
        sum += fabs(audio_chunk[i] - reference_tone[i]);
    }
    return sum / TONE_SAMPLES_COUNT;
}

// Calculate the average of scores to detect outliers
u_int8_t detect_button(const float *audio_chunk, float **freqs_buffers) {
    float scores[BTN_NUMBER] = {};
    float sum = 0;
    for (int i = 0; i < BTN_NUMBER; i++) {
        float score = get_near_score(audio_chunk, freqs_buffers[i]);
        scores[i] = score;
        sum += score;
    }

    float avg = sum / BTN_NUMBER;

    u_int8_t farthest_btn_from_avg_score = BTN_NOT_FOUND;
    u_int8_t farthest_distance_to_avg_score = 0;
    for (int i = 0; i < BTN_NUMBER; i++) {
        float distance = fabs(scores[i] - avg);
        if (distance > farthest_distance_to_avg_score) {
            farthest_distance_to_avg_score = distance;
            farthest_btn_from_avg_score = i;
        }
    }

    return farthest_btn_from_avg_score;
}

int8_t dtmf_decode(const float *audio_buffer, sf_count_t samples_count, char *result_text) {
    const float *cursor = audio_buffer;
    sf_count_t cursor_index = 0;
    float **freqs_buffers = generate_all_frequencies_buffers();
    long maximum_text_buffer_size = samples_count / (TONE_SAMPLES_COUNT + SILENCE_SAMPLES_COUNT) + 2;// having only 1 tone letter would produce the maximum of letter count + 2 (ceil + \0)
    result_text = calloc(maximum_text_buffer_size, sizeof(char));
    if (!result_text)
        printf("Error: couldn't allocate result text buffer\n");

    while (cursor_index < samples_count) {
        u_int8_t found_btn = detect_button(audio_buffer, freqs_buffers);
        if (found_btn == BTN_NOT_FOUND) {
        }
    }
}
