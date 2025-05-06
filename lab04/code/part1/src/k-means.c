#include "k-means.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

// This function will calculate the SQUARED euclidean distance between two pixels.
// Instead of using coordinates, we use the RGB value for evaluating distance.
unsigned distance(uint8_t *p1, uint8_t *p2) {
    // No SIMD here, because the cost of setup is too high...
    unsigned r_diff = p1[0] - p2[0];
    unsigned g_diff = p1[1] - p2[1];
    unsigned b_diff = p1[2] - p2[2];
    return r_diff * r_diff + g_diff * g_diff + b_diff * b_diff;
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

    unsigned *distances = (unsigned *) malloc(surface * sizeof(unsigned));

    // Calculate distances from each pixel to the first center
    for (int i = 0; i < surface; ++i) {
        distances[i] = distance(image->data + i * image->components, centers);
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
        unsigned min_dist = INFINITY;
        int best_cluster = -1;
        int assigned_to;

        for (int c = 0; c < num_clusters; c++) {
            unsigned dist = distance(image->data + i * image->components, centers + c * image->components);

            if (dist < min_dist) {
                min_dist = dist;
                best_cluster = c;
            }

            assigned_to = best_cluster;
        }
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
