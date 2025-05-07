#include <stdint.h>

#define PNG_STRIDE_IN_BYTES 0

#define COMPONENT_RGBA 4
#define COMPONENT_RGB 3
#define COMPONENT_GRAYSCALE 1

#define FACTOR_R 0.21
#define FACTOR_G 0.72
#define FACTOR_B 0.07

#define R_OFFSET 0
#define G_OFFSET 1
#define B_OFFSET 2
#define A_OFFSET 3

struct img_t {
    int width;
    int height;
    int components;

    uint8_t *data;
};

struct img_t *load_image(const char *path);

int save_image(const char *dest_path, const struct img_t *img);

struct img_t *allocate_image(int width, int height, int components);

void free_image(struct img_t *img);
