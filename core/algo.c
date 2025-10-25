#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

// Define a simple Pixel struct
typedef struct {
    unsigned char r, g, b;
} Pixel;

// Convert the stbi pixel buffer into Pixel array
Pixel* convert_pixels(unsigned char* data, int width, int height, int channels) {
    Pixel* pixels = malloc(width * height * sizeof(Pixel));
    if (!pixels) {
        printf("Failed to allocate memory for pixels\n");
        return NULL;
    }

    for (int i = 0; i < width * height; i++) {
        pixels[i].r = data[i * channels + 0];
        pixels[i].g = data[i * channels + 1];
        pixels[i].b = data[i * channels + 2];
    }

    return pixels;
}

// Compress pixels using chunk-based RLE + RAW pixels
void compress_chunks(Pixel* pixels, int total_pixels, const char* out_file) {
    FILE* out = fopen(out_file, "wb");
    if (!out) {
        printf("Failed to open output file\n");
        return;
    }

    Pixel prev = pixels[0];
    int run = 1;

    for (int i = 1; i < total_pixels; i++) {
        if (pixels[i].r == prev.r &&
            pixels[i].g == prev.g &&
            pixels[i].b == prev.b &&
            run < 255) { // max run length for 1 byte
            run++;
        } else {
            if (run > 1) {
                // RLE chunk
                fputc(0x00, out);      // code for RLE
                fputc(run, out);       // run length
                fputc(prev.r, out);
                fputc(prev.g, out);
                fputc(prev.b, out);
            } else {
                // RAW chunk
                fputc(0x01, out);      // code for RAW
                fputc(prev.r, out);
                fputc(prev.g, out);
                fputc(prev.b, out);
            }
            prev = pixels[i];
            run = 1;
        }
    }

    // flush last pixel
    if (run > 1) {
        fputc(0x00, out);
        fputc(run, out);
        fputc(prev.r, out);
        fputc(prev.g, out);
        fputc(prev.b, out);
    } else {
        fputc(0x01, out);
        fputc(prev.r, out);
        fputc(prev.g, out);
        fputc(prev.b, out);
    }

    fclose(out);
    printf("Compression complete. Output saved to %s\n", out_file);
}

int main() {
    char *filename = "static/snail.bmp";

    int width, height, channels;
    unsigned char *pixels_data = stbi_load(filename, &width, &height, &channels, 0);

    if (!pixels_data) {
        printf("Failed to load image! Reason: %s\n", stbi_failure_reason());
        return 1;
    }

    printf("Loaded image: %dx%d, %d channels\n", width, height, channels);

    Pixel* pixels = convert_pixels(pixels_data, width, height, channels);
    if (!pixels) {
        stbi_image_free(pixels_data);
        return 1;
    }

    stbi_image_free(pixels_data);

    compress_chunks(pixels, width * height, "compressed.pp");

    free(pixels);
    return 0;
}

