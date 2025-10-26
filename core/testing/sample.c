#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../libs/stb_image_write.h"

int main(void) {
    const char *input_path = "static/snail.bmp";
    const char *output_path = "static/snail.png";

    int width, height, channels;
    unsigned char *image_data = stbi_load(input_path, &width, &height, &channels, 0);
    if (!image_data) {
        fprintf(stderr, "Error: failed to load %s\n", input_path);
        return 1;
    }

    printf("Loaded: %s (%dx%d, %d channels)\n", input_path, width, height, channels);

    // Time measurement setup
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Convert: write PNG using same data
    if (!stbi_write_png(output_path, width, height, channels, image_data, width * channels)) {
        fprintf(stderr, "Error: failed to write %s\n", output_path);
        stbi_image_free(image_data);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Elapsed time in seconds
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("BMP → PNG conversion complete.\n");
    printf("Time taken: %.6f seconds\n", elapsed);

    stbi_image_free(image_data);
    return 0;
}
