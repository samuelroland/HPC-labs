#include "image.h"

typedef struct {
    int count;
    int sum_r;
    int sum_g;
    int sum_b;
} ClusterData;


unsigned distance(uint8_t *p1, uint8_t *p2);
void kmeans_pp(struct img_t *image, int num_clusters, uint8_t *centers);
void kmeans(struct img_t *image, int num_clusters);
