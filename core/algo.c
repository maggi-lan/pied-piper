#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#include "libs/arith.h"  // Minimal adaptive arithmetic coder

typedef struct {
    unsigned char r, g, b;
} Pixel;

static inline unsigned char clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

int loco_predict(int a, int b, int c) {
    int p = a + b - c;
    if (c >= (a > b ? a : b)) return (a < b) ? a : b;
    else if (c <= (a < b ? a : b)) return (a > b) ? a : b;
    else return p;
}

void compute_residuals(const uint8_t* src, int width, int height, uint8_t* residuals) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            int a = x > 0 ? src[y * width + (x - 1)] : 0;
            int b = y > 0 ? src[(y - 1) * width + x] : 0;
            int c = (x > 0 && y > 0) ? src[(y - 1) * width + (x - 1)] : 0;

            int pred = loco_predict(a, b, c);
            int res = (int)src[idx] - pred;
            residuals[idx] = (uint8_t)(res & 0xFF);
        }
    }
}

// RLE encode: [count][value]
unsigned char* rle_encode(const unsigned char* data, size_t len, size_t* out_len) {
    size_t capacity = len * 2;
    unsigned char* out = malloc(capacity);
    size_t pos = 0, i = 0;
    while (i < len) {
        size_t run = 1;
        while (i + run < len && data[i] == data[i + run] && run < 255) run++;
        if (pos + 2 > capacity) {
            capacity *= 2;
            out = realloc(out, capacity);
        }
        out[pos++] = (unsigned char)run;
        out[pos++] = data[i];
        i += run;
    }
    *out_len = pos;
    return out;
}

int main() {
    const char* inpath = "static/snail.bmp";
    const char* outcompressed = "static/compressed.pp";

    int width, height, channels;
    unsigned char* img = stbi_load(inpath, &width, &height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Error loading image: %s\n", stbi_failure_reason());
        return 1;
    }

    size_t px_count = (size_t)width * height;
    uint8_t* r_chan = malloc(px_count), *g_chan = malloc(px_count), *b_chan = malloc(px_count);
    for (size_t i = 0; i < px_count; i++) {
        r_chan[i] = img[3*i];
        g_chan[i] = img[3*i+1];
        b_chan[i] = img[3*i+2];
    }
    free(img);

    // Compute LOCO-I residuals
    uint8_t* res_r = malloc(px_count), *res_g = malloc(px_count), *res_b = malloc(px_count);
    compute_residuals(r_chan, width, height, res_r);
    compute_residuals(g_chan, width, height, res_g);
    compute_residuals(b_chan, width, height, res_b);
    free(r_chan); free(g_chan); free(b_chan);

    // Concatenate for compression
    size_t total_len = px_count * 3;
    unsigned char* combined = malloc(total_len);
    memcpy(combined, res_r, px_count);
    memcpy(combined + px_count, res_g, px_count);
    memcpy(combined + 2 * px_count, res_b, px_count);
    free(res_r); free(res_g); free(res_b);

    // RLE encode
    size_t rle_len;
    unsigned char* rle_data = rle_encode(combined, total_len, &rle_len);
    free(combined);

    // Arithmetic encode
    size_t arith_cap = rle_len * 2;
    unsigned char* arith_out = malloc(arith_cap);
    size_t arith_len = arithmetic_encode(rle_data, rle_len, arith_out, arith_cap);
    free(rle_data);

    FILE* fout = fopen(outcompressed, "wb");
    fwrite(&width, sizeof(int), 1, fout);
    fwrite(&height, sizeof(int), 1, fout);
    int nchannels = 3;
    fwrite(&nchannels, sizeof(int), 1, fout);
    fwrite(&arith_len, sizeof(size_t), 1, fout);
    fwrite(arith_out, 1, arith_len, fout);
    fclose(fout);
    free(arith_out);

    printf("Compression complete: %zu bytes compressed, output file: %s\n", arith_len, outcompressed);

    return 0;
}
