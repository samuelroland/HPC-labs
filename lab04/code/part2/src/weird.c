#include "weird.h"
#include "immintrin.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define RGB_MAX 255
#define ENABLE_SIMD

void weirdimg_transform(struct img_t *image, int16_t brightness_factor) {
    int surface = image->height * image->width;
    int surfaceTimesComponents = surface * image->components;
    int loopTour = surfaceTimesComponents;
    uint8_t *data = image->data;
    bool factor_is_darker = brightness_factor >= 0;// 2 = 2 times darker not ligther ! the = is important, so it does a "* 0" not a "/ 0" !
    int8_t opposite_factor = -brightness_factor;   // when we need to divide instead of multiply we need the positive value

#ifdef ENABLE_SIMD
    int incr = 32;
    int remaining_surface_start = surfaceTimesComponents - (surfaceTimesComponents % incr);
    __m256i max_values = _mm256_set1_epi32(0xFFFFFFFF);
    __m256i brightness_factor_mult = _mm256_set1_epi16(brightness_factor);

    if (factor_is_darker) {
        for (int i = 0; i < remaining_surface_start; i += incr) {
            // let's load 32 channel values
            __m256i values = _mm256_loadu_si256((__m256i *) (data + i));
            // let's invert them
            values = _mm256_sub_epi8(max_values, values);
            // Apply the brightness_factor by multiplication
            // Unpack values into 16 bits for the multiplication
            __m256i lo = _mm256_unpacklo_epi8(values, _mm256_setzero_si256());
            __m256i hi = _mm256_unpackhi_epi8(values, _mm256_setzero_si256());
            // Do the multiplication
            lo = _mm256_mullo_epi16(lo, brightness_factor_mult);
            hi = _mm256_mullo_epi16(hi, brightness_factor_mult);
            // Make sure the 16 bits don't go over the RGB_MAX because taking the 8 low bits will not be RGB_MAX if they are above !
            lo = _mm256_max_epi16(lo, max_values);
            hi = _mm256_max_epi16(hi, max_values);
            __m256i packed = _mm256_packus_epi16(lo, hi);
            _mm256_storeu_si256((__m256i *) (data + i), packed);
        }

        // Manage the remaining channel values that are after the last multiple of 32
        // by reusing the the following code
        if (remaining_surface_start < surfaceTimesComponents) {
            loopTour = remaining_surface_start;// let the next loop manage the rest
        } else {
            return;// we are done
        }
    } else {
        // NOTE: not possible to implement division in SIMD without going back to float, that's probably not worth to do the conversion
        // let's just use the non SIMD version
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
            v = v / opposite_factor;// no risk to be under 0 as division will not be under 0
        }

        data[i] = v;
    }
}
