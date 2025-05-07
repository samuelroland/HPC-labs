#include "k-means.h"
#include "immintrin.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This function will calculate the SQUARED euclidean distance between two pixels.
// Instead of using coordinates, we use the RGB value for evaluating distance.
unsigned distance(uint8_t *p1, uint8_t *p2) {
    // No SIMD here, because the cost of setup is too high...
    unsigned r_diff = abs(p1[0] - p2[0]);
    unsigned g_diff = abs(p1[1] - p2[1]);
    unsigned b_diff = abs(p1[2] - p2[2]);
    return r_diff + g_diff + b_diff;
}

// Function to initialize cluster centers using the k-means++ algorithm
void kmeans_pp(struct img_t *image, int num_clusters, uint8_t *centers) {
    const int surface = image->width * image->height;
    const int sizeOfComponents = image->components * sizeof(uint8_t);
    // Select a random pixel as the first cluster center
    int first_center = (rand() % surface) * image->components;

    // Set the RGB values of the first center
    centers[R_OFFSET] = image->data[first_center + R_OFFSET];
    centers[G_OFFSET] = image->data[first_center + G_OFFSET];
    centers[B_OFFSET] = image->data[first_center + B_OFFSET];

    u_int16_t *distances = (u_int16_t *) malloc(surface * sizeof(u_int16_t));

    // Copy the image data as another buffer organized in 3 contiguous arrays, the first for red pixels, second for green and last for blue
    u_int8_t *image_as_3_pixels_arrays = (u_int8_t *) malloc(surface * sizeOfComponents * sizeof(u_int8_t));
    if (!image_as_3_pixels_arrays) {
        printf("error, failed to allocate image_as_3_pixels_list");
        exit(2);
    }
    u_int8_t *data = image->data;
    u_int8_t *data_r = image_as_3_pixels_arrays;
    u_int8_t *data_g = image_as_3_pixels_arrays + surface;
    u_int8_t *data_b = data_g + surface;
    for (int i = 0; i < surface; i++) {
        data_r[i] = data[i * sizeOfComponents + R_OFFSET];
        data_g[i] = data[i * sizeOfComponents + G_OFFSET];
        data_b[i] = data[i * sizeOfComponents + B_OFFSET];
    }

    // Calculate distances from each pixel to the first center
    // This is a vectorized version that try to manage 32 pixels at a time
    u_int8_t *first_center_data = image->data + first_center;
    __m256i reds, greens, blues;         // 32 times 8 bits
    __m256i center_r, center_g, center_b;// 32 times the same value of red, green and blue of the first center
    __m256i dists_lo;                    // 16 final distances values of 16bits each, for the 16 low pixels
    __m256i dists_hi;                    // 16 final distances values of 16bits each, for the 16 high pixels
    center_r = _mm256_set1_epi8(first_center_data[R_OFFSET]);
    center_g = _mm256_set1_epi8(first_center_data[G_OFFSET]);
    center_b = _mm256_set1_epi8(first_center_data[B_OFFSET]);
    dists_lo = _mm256_setzero_si256();
    dists_hi = _mm256_setzero_si256();

    int incr = 32;// 256 / 8 = 32
    int remaining_start = surface - (surface % incr);
    for (int i = 0; i < remaining_start; i += incr) {

        // Compute abs(mm256onechannel_color - mm256onechannel_center) for on channel on 32 pixels
        // To avoid needing larger types than u_int8_t to store the results required by squared values,
        // and to avoid underflow during the substraction in u_int8_t, we are going to do the substraction
        // of the bigger value by the smaller value and skip the square as we already have positive values
        // This strategy comes from GPT4o as I had no idea on how to that
#define COMPUTE_32PIXELS_DISTANCE_ONECHANNEL(mm256onechannel_color, mm256onechannel_center, dists_hi, dists_lo, distances, i, incr) \
    do {                                                                                                                            \
        __m256i max_ab = _mm256_max_epu8(mm256onechannel_color, mm256onechannel_center);                                            \
        __m256i min_ab = _mm256_min_epu8(mm256onechannel_color, mm256onechannel_center);                                            \
        __m256i abs_diff = _mm256_subs_epu8(max_ab, min_ab);                                                                        \
        /* Make the 16 low 8 bits integers into 16 times 16 bits integers */                                                        \
        __m256i partial_abs_diff = _mm256_unpacklo_epi8(abs_diff, _mm256_setzero_si256());                                          \
        dists_lo = _mm256_add_epi8(partial_abs_diff, dists_lo);                                                                     \
        /* Same for the 16 high 8 bits integers */                                                                                  \
        partial_abs_diff = _mm256_unpackhi_epi8(abs_diff, _mm256_setzero_si256());                                                  \
        dists_hi = _mm256_add_epi8(partial_abs_diff, dists_hi);                                                                     \
    } while (0)

        reds = _mm256_loadu_si256((__m256i *) (data_r + i));
        greens = _mm256_loadu_si256((__m256i *) (data_g + i));
        blues = _mm256_loadu_si256((__m256i *) (data_b + i));
        COMPUTE_32PIXELS_DISTANCE_ONECHANNEL(reds, center_r, dists_hi, dists_lo, distances, i, incr);
        COMPUTE_32PIXELS_DISTANCE_ONECHANNEL(greens, center_g, dists_hi, dists_lo, distances, i, incr);
        COMPUTE_32PIXELS_DISTANCE_ONECHANNEL(blues, center_b, dists_hi, dists_lo, distances, i, incr);

        // We just need to save the 32 distances buffer in memory
        // We are storing the 16 times 16 bits = 16 times 2 bytes = 32 bytes of dists_lo then the 32 bytes of dists_hi
        // First dists_hi is at byte 0, then 64, then 128, ...
        // First dists_lo is at byte 32, then 96, ...
        u_int8_t *ptri = ((u_int8_t *) distances) + 2 * i;
        u_int8_t *ptrj = ((u_int8_t *) distances) + 2 * i + 32;
        _mm256_storeu_si256((__m256i *) ptri, dists_hi);
        _mm256_storeu_si256((__m256i *) ptrj, dists_lo);
    }

    uint8_t cr = first_center_data[R_OFFSET];
    uint8_t cg = first_center_data[G_OFFSET];
    uint8_t cb = first_center_data[B_OFFSET];
    // Manage the remaining pixels that are after the last multiple of 32
    for (int i = remaining_start; i < surface; i++) {
        int16_t r_diff = data_r[i] < cr ? cr - data_r[i] : data_r[i] - cr;
        int16_t g_diff = data_g[i] < cg ? cg - data_g[i] : data_g[i] - cg;
        int16_t b_diff = data_b[i] < cb ? cb - data_b[i] : data_b[i] - cb;
        distances[i] = r_diff + g_diff + b_diff;
    }

    // Loop to find remaining cluster centers
    int iTimesComponents = 0;// will be = i * image->components;
    for (int i = 1; i < num_clusters; ++i) {
        iTimesComponents += image->components;
        unsigned total_weight = 0;

        // Calculate total weight (sum of distances)
        for (int j = 0; j < surface; ++j) {
            total_weight += distances[j];
        }

        unsigned r = ((float) rand() / (float) RAND_MAX) * total_weight;
        int index = 0;

        // Choose the next center based on weighted probability
        while (index < surface && r > distances[index]) {
            r -= distances[index];
            index++;
        }

        // Set the RGB values of the selected center
        int indexTimesComponents = index * image->components;
        // Copy the 3 pixels directly as there are contiguous !
        memcpy(centers + iTimesComponents, image->data + indexTimesComponents, sizeOfComponents);

        // Update distances based on the new center
        for (int j = 0; j < surface; j++) {
            unsigned dist = distance(image->data + j * image->components, centers + iTimesComponents);

            if (dist < distances[j]) {
                distances[j] = dist;
            }
        }
    }

    free(image_as_3_pixels_arrays);
    free(distances);
}

// This function performs k-means clustering on an image.
// It takes as input the image, its dimensions (width and height), and the number of clusters to find.
void kmeans(struct img_t *image, int num_clusters) {
    const int surface = image->width * image->height;
    const int sizeOfComponents = image->components * sizeof(uint8_t);

    uint8_t *centers = calloc(num_clusters * image->components, sizeof(uint8_t));

    // Initialize the cluster centers using the k-means++ algorithm.
    kmeans_pp(image, num_clusters, centers);

    int *assignments = (int *) malloc(surface * sizeof(int));

    // Assign each pixel in the image to its nearest cluster.
    for (int i = 0; i < surface; ++i) {
        unsigned min_dist = UINT_MAX;
        int best_cluster = -1;
        int assigned_to = -1;

        for (int c = 0; c < num_clusters; c++) {
            unsigned dist = distance(image->data + i * image->components, centers + c * image->components);

            if (dist < min_dist) {
                min_dist = dist;
                best_cluster = c;
            }

            assigned_to = best_cluster;
        }
        if (assigned_to != -1)
            assignments[i] = assigned_to;
    }

    ClusterData *cluster_data = (ClusterData *) calloc(num_clusters, sizeof(ClusterData));

    // Compute the sum of the pixel values for each cluster.
    for (int i = 0; i < surface; ++i) {
        int cluster = assignments[i];
        cluster_data[cluster].count++;
        const int iTimesComponents = i * image->components;
        cluster_data[cluster].sum_r += (int) image->data[iTimesComponents + R_OFFSET];
        cluster_data[cluster].sum_g += (int) image->data[iTimesComponents + G_OFFSET];
        cluster_data[cluster].sum_b += (int) image->data[iTimesComponents + B_OFFSET];
    }

    // Update cluster centers based on the computed sums
    for (int c = 0; c < num_clusters; ++c) {
        const int cTimesComponents = c * image->components;
        if (cluster_data[c].count > 0) {
            centers[cTimesComponents + R_OFFSET] = cluster_data[c].sum_r / cluster_data[c].count;
            centers[cTimesComponents + G_OFFSET] = cluster_data[c].sum_g / cluster_data[c].count;
            centers[cTimesComponents + B_OFFSET] = cluster_data[c].sum_b / cluster_data[c].count;
        }
    }

    free(cluster_data);

    // Update image data with the cluster centers
    for (int i = 0; i < surface; ++i) {
        int cluster = assignments[i];
        // Copy the 3 pixels directly as there are contiguous !
        memcpy(image->data + i * image->components, centers + cluster * image->components, sizeOfComponents);
    }

    free(assignments);
    free(centers);
}
