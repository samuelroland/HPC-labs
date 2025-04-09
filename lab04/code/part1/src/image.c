#include <stdlib.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "image.h"

struct img_t *load_image(const char *path){
    struct img_t *img;

    /* Allocate struct */
    img = (struct img_t*)malloc(sizeof(struct img_t));
    if (!img) {
        fprintf(stderr, "[%s] struct allocation error\n", __func__);
        perror(__func__);
        exit(EXIT_FAILURE);
    }

    /* Load the image */ 
    img->data = stbi_load(path, &(img->width), &(img->height), &(img->components), 0);
    if (!(img->data)) {
        fprintf(stderr, "[%s] stbi load image failed\n", __func__);
        perror(__func__);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    fprintf(stdout, "[%s] image %s loaded (%d components, %dx%d)\n", __func__,
            path, img->components, img->width, img->height);
#endif

    return img;
}


int save_image(const char *dest_path, const struct img_t *img) {
    int ret;

    ret = stbi_write_png(dest_path, img->width, img->height, img->components, img->data, PNG_STRIDE_IN_BYTES);
    if(ret) {
        #ifdef DEBUG
        fprintf(stdout, "[%s] PNG file %s saved (%dx%d)\n", __func__, dest_path, img->width, img->height);
        #endif
    }
    else {
        fprintf(stderr, "[%s] save image failed\n", __func__);
    }

    return ret;
}

struct img_t *allocate_image(int width, int height, int components){
    struct img_t *img;

    if (components == 0 || components > COMPONENT_RGBA)
        return NULL;

    /* Allocate struct */
    img = (struct img_t*)malloc(sizeof(struct img_t));
    if (!img) {
        fprintf(stderr, "[%s] allocation error\n", __func__);
        perror(__func__);
        exit(EXIT_FAILURE);
    }

    img->width = width;
    img->height = height;
    img->components = components;

    /* Allocate space for image data */
    img->data = (uint8_t*)calloc(img->width * img->height *img->components, sizeof(uint8_t));
    if (!(img->data)) {
        fprintf(stderr, "[%s] image allocation error\n", __func__);
        perror(__func__);
        exit(EXIT_FAILURE);
    }

    return img;
}

void free_image(struct img_t *img) {
    stbi_image_free(img->data);
    free(img);
}