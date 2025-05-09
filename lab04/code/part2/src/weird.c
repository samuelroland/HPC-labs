#include "weird.h"
#include "immintrin.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define RGB_MAX 255
#define ENABLE_SIMD

void weirdimg_transform(struct img_t *image, int brightness_factor) {
    int surface = image->height * image->width;
    int surfaceTimesComponents = surface * image->components;
    int loopTour = surfaceTimesComponents;
    uint8_t *data = image->data;
    bool factor_is_darker = brightness_factor > 0;// 2 = 2 times darker not ligther !
    int opposite_factor = -brightness_factor;

#ifdef ENABLE_SIMD
    int incr = 32;
    int remaining_surface_start = surfaceTimesComponents - (surfaceTimesComponents % incr);
    __m256i max_values = _mm256_set1_epi32(0xFFF);
    for (int i = 0; i < remaining_surface_start; i += incr) {
        // let's load 32 channel values
        __m256i values = _mm256_loadu_si256((__m256i *) (data + i));
        // let's invert them
        _mm256_sub_epi8(max_values, values);

        _mm256_storeu_si256((__m256i *) (data + i), values);
    }

    // Manage the remaining channel values that are after the last multiple of 32
    // by reusing the the following code
    if (remaining_surface_start < surfaceTimesComponents) {
        loopTour = remaining_surface_start;
    } else {
        return;
    }
#endif

    // Treat each channel of each pixel independantly
    for (int i = 0; i < loopTour; i++) {
        uint8_t v = data[i];
        v = RGB_MAX - v;// invert the color

        int16_t bigger_v = v;// a bigger copy so we can detect overflows

        // Apply the brightness_factor by multiplication
        if (factor_is_darker) {
            bigger_v = bigger_v * brightness_factor;
            v = bigger_v > RGB_MAX ? RGB_MAX : bigger_v;// we don't want to take the overflow version in case it goes outside
        } else {
            // printf("%d / %d\n", bigger_v, abs_factor);
            v = v / opposite_factor;// no risk to be under 0 as division will not be under 0
        }

        data[i] = v;
    }
}
