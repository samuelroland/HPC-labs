// Implementation of the float party, measuring the energy used for 2 implementations
// Just loading a big buffer float values with value 1 and doing the sum of these values
// This code is made of 2 implementation, one basic and a second one with SIMD
#include "perf.h"
#include <assert.h>
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>

#define CODE_VERSION 1// 1 is normal, 2 is SIMD
#if CODE_VERSION == 1
int benchmark(int size) {
    float *bigBuffer = malloc(size * sizeof(float));
    if (!bigBuffer) return 0;

    for (int i = 0; i < size; i++) {
        bigBuffer[i] = 1;
    }

    PerfManager pmon;
    PerfManager_init(&pmon);
    PerfManager_resume(&pmon);
    float sum = 0;
    for (int i = 0; i < size; i++) {
        sum += bigBuffer[i];
    }
    PerfManager_pause(&pmon);
    free(bigBuffer);
    return sum;// it will convert to int, as we want a round value to compare with that's good
}
#endif

#if CODE_VERSION == 2
// This SIMD variant is using an accumulator initialized to zero, to sum groups of 8 float together.
// At the end, we just sum the 8 float and this gives us the final sum
int benchmark(int size) {
    float *bigBuffer = malloc(size * sizeof(float));
    if (!bigBuffer) return 0;
    for (int i = 0; i < size; i++) {
        bigBuffer[i] = 1;
    }

    PerfManager pmon;
    PerfManager_init(&pmon);
    PerfManager_resume(&pmon);
    __m256 accumulator = _mm256_set1_ps(0);
    int incr = 8;// 8 float per 256 bits registers
    int partialMaximum = size - (size % incr);

    float *ptr = bigBuffer;
    float sum = 0;
    for (int i = 0; i < partialMaximum; i += incr) {
        __m256 current8floats = _mm256_loadu_ps(ptr);
        accumulator = _mm256_add_ps(current8floats, accumulator);
    }

    // Sum values into the accumulator
    float accFloats[incr];
    _mm256_store_ps(accFloats, accumulator);
    for (int i = 0; i < incr; i++) {
        sum += accFloats[i];
    }

    // Manage the rest
    for (int i = partialMaximum; i < size; i++) {
        sum += bigBuffer[i];
    }
    PerfManager_pause(&pmon);

    free(bigBuffer);
    return sum;// it will convert to int, as we want a round value to compare with that's good
}
#endif

int main(int argc, char *argv[]) {
    int MB_size;
    if (argc < 2) {
        printf("Taking default value for size of float buffer\n");
        MB_size = 10000000;// 10millions of float = 40millions of bytes
    } else {
        MB_size = atoi(argv[1]);
    }

    printf("Starting benchmark with size = %d\n", MB_size);
    int result = benchmark(MB_size);
    if (result == MB_size) {
        printf("Result is correct\n");
    } else {
        printf("Error: sum should be %d but got %d\n", MB_size, result);
    }
    printf("Benchmark done\n");
}
