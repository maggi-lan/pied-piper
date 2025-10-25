#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

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

// Helper to clamp a difference to signed char
int clamp_diff(int diff) {
    if (diff < 0) return 0;
    if (diff > 255) return 255;
    return diff;
}

// Compress pixels using chunk-based RLE + RAW + DIFF pixels
void compress_chunks(Pixel* pixels, int total_pixels, const char* out_file) {
    FILE* out = fopen(out_file, "wb");
    if (!out) {
        printf("Failed to open output file\n");
        return;
    }

    Pixel prev = pixels[0];
    int run = 1;

    for (int i = 1; i < total_pixels; i++) {
        Pixel curr = pixels[i];

        // Check for RLE continuation
        if (curr.r == prev.r && curr.g == prev.g && curr.b == prev.b && run < 255) {
            run++;
            continue;
        }

        // If run ended, flush RLE or single pixel
        if (run > 1) {
            fputc(0x00, out); // code for RLE
            fputc(run, out);
            fputc(prev.r, out);
            fputc(prev.g, out);
            fputc(prev.b, out);
            run = 1;
        } else {
            // Check if the current pixel is close enough for DIFF
            int dr = (int)curr.r - (int)prev.r;
            int dg = (int)curr.g - (int)prev.g;
            int db = (int)curr.b - (int)prev.b;

            if (dr >= -2 && dr <= 2 && dg >= -2 && dg <= 2 && db >= -2 && db <= 2) {
                // DIFF chunk
                fputc(0x02, out); // code for DIFF
                fputc((signed char)dr, out);
                fputc((signed char)dg, out);
                fputc((signed char)db, out);
            } else {
                // RAW chunk
                fputc(0x01, out); // code for RAW
                fputc(curr.r, out);
                fputc(curr.g, out);
                fputc(curr.b, out);
            }
        }

        prev = curr;
    }

    // Flush the last pixel
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

// Decode compressed file into pixel array
Pixel* decode_chunks(const char* in_file, int total_pixels) {
    FILE* in = fopen(in_file, "rb");
    if (!in) {
        printf("Failed to open input file for decoding!\n");
        return NULL;
    }

    Pixel* pixels = malloc(total_pixels * sizeof(Pixel));
    if (!pixels) {
        printf("Failed to allocate memory for decoding!\n");
        fclose(in);
        return NULL;
    }

    Pixel prev = {0, 0, 0};
    int count = 0;

    while (!feof(in) && count < total_pixels) {
        int code = fgetc(in);
        if (code == EOF) break;

        if (code == 0x00) { // RLE
            int run = fgetc(in);
            Pixel px;
            px.r = fgetc(in);
            px.g = fgetc(in);
            px.b = fgetc(in);

            for (int i = 0; i < run && count < total_pixels; i++) {
                pixels[count++] = px;
            }
            prev = px;
        } else if (code == 0x01) { // RAW
            Pixel px;
            px.r = fgetc(in);
            px.g = fgetc(in);
            px.b = fgetc(in);
            pixels[count++] = px;
            prev = px;
        } else if (code == 0x02) { // DIFF
            int dr = (signed char)fgetc(in);
            int dg = (signed char)fgetc(in);
            int db = (signed char)fgetc(in);
            Pixel px;
            px.r = clamp_diff(prev.r + dr);
            px.g = clamp_diff(prev.g + dg);
            px.b = clamp_diff(prev.b + db);
            pixels[count++] = px;
            prev = px;
        } else {
            printf("Unknown chunk code: 0x%02X\n", code);
            break;
        }
    }

    fclose(in);
    printf("Decoded %d pixels.\n", count);
    return pixels;
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

    const char* compressed_file = "static/compressed.pp";
    compress_chunks(pixels, width * height, compressed_file);

    // Decode back
    Pixel* decoded = decode_chunks(compressed_file, width * height);
    if (!decoded) {
        free(pixels);
        return 1;
    }

    // Write decoded image for verification
    unsigned char* decoded_bytes = malloc(width * height * 3);
    for (int i = 0; i < width * height; i++) {
        decoded_bytes[i * 3 + 0] = decoded[i].r;
        decoded_bytes[i * 3 + 1] = decoded[i].g;
        decoded_bytes[i * 3 + 2] = decoded[i].b;
    }

    stbi_write_bmp("static/decoded.bmp", width, height, 3, decoded_bytes);

    printf("Decoded image saved as decoded.bmp\n");

    free(pixels);
    free(decoded);
    free(decoded_bytes);
    return 0;
}
