#include "weird.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    struct img_t *img;
    if (argc < 4) {
        fprintf(stderr, "Usage : %s <img_src.png> <brighness factor> <img_dest.png>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int brightness_factor = atoi(argv[2]);
    if (brightness_factor < -10 || brightness_factor > 10) {
        printf("The brightness_factor should be between -10 and 10 included\n");
        return 1;
    }

    img = load_image(argv[1]);

    printf("Image loaded!\n");

    printf("Nb of components in this image %d\n", img->components);

    printf("Inverting colors and applying a luminosity factor of %d on image %s\n", brightness_factor, argv[1]);
    weirdimg_transform(img, brightness_factor);

    save_image(argv[3], img);

    free_image(img);

    printf("Programm ended successfully\n\n");

    return 0;
}
