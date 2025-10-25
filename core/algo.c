#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

int main() {
	char *filename = "static/snail.bmp";

    int width, height, channels;
    unsigned char *pixels = stbi_load(filename, &width, &height, &channels, 0);

    if (!pixels) {
        printf("Failed to load image!\n");
        return 1;
    }

    printf("Loaded image: %dx%d, %d channels\n", width, height, channels);

    // iterate through pixels
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * channels;
            unsigned char r = pixels[idx + 0];
            unsigned char g = pixels[idx + 1];
            unsigned char b = pixels[idx + 2];
            unsigned char a = (channels == 4) ? pixels[idx + 3] : 255;

            // process pixel here
        }
    }

    stbi_image_free(pixels);
    return 0;
}
