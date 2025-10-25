#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

#include "libs/arith.h"  // Minimal adaptive arithmetic coder interface

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

void inverse_predict_loco_i(const uint8_t* resid, uint8_t* out, int wid, int ht) {
    for (int y = 0; y < ht; y++) {
        for (int x = 0; x < wid; x++) {
            int idx = y * wid + x;
            int a = x > 0 ? out[y * wid + (x - 1)] : 0;
            int b = y > 0 ? out[(y - 1) * wid + x] : 0;
            int c = (x > 0 && y > 0) ? out[(y - 1) * wid + (x - 1)] : 0;
            int pred = loco_predict(a, b, c);
            int val = (int)resid[idx] + pred;
            out[idx] = clamp_u8(val);
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

// RLE decode function
unsigned char* rle_decode(const unsigned char* data, size_t len, size_t out_len) {
    unsigned char* out = malloc(out_len);
    size_t pos = 0, i = 0;
    while (i + 1 < len && pos < out_len) {
        unsigned char run = data[i++];
        unsigned char val = data[i++];
        for (int j = 0; j < run && pos < out_len; j++) {
            out[pos++] = val;
        }
    }
    return out;
}

int main() {
    // File paths
    const char* inpath = "static/snail.bmp";
    const char* outcompressed = "static/compressed.pp";
    const char* outdecoded = "static/decoded.bmp";

    // Load input image
    int width, height, channels;
    unsigned char* img = stbi_load(inpath, &width, &height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }

    size_t px_count = (size_t)width * height;

    // Separate into R G B channels
    uint8_t* r_chan = malloc(px_count);
    uint8_t* g_chan = malloc(px_count);
    uint8_t* b_chan = malloc(px_count);
    for (size_t i = 0; i < px_count; i++) {
        r_chan[i] = img[3*i];
        g_chan[i] = img[3*i + 1];
        b_chan[i] = img[3*i + 2];
    }
    free(img);

    // Compute prediction residuals using LOCO-I
    uint8_t* res_r = malloc(px_count);
    uint8_t* res_g = malloc(px_count);
    uint8_t* res_b = malloc(px_count);
    compute_residuals(r_chan, width, height, res_r);
    compute_residuals(g_chan, width, height, res_g);
    compute_residuals(b_chan, width, height, res_b);
    free(r_chan);
    free(g_chan);
    free(b_chan);

    // Concatenate residuals
    size_t total_len = px_count * 3;
    unsigned char* combined = malloc(total_len);
    memcpy(combined, res_r, px_count);
    memcpy(combined + px_count, res_g, px_count);
    memcpy(combined + 2 * px_count, res_b, px_count);
    free(res_r);
    free(res_g);
    free(res_b);

    // Apply RLE encoding
    size_t rle_len;
    unsigned char* rle_data = rle_encode(combined, total_len, &rle_len);
    free(combined);

    // Adaptive Arithmetic encode the RLE data
    size_t arith_capacity = rle_len * 2;
    unsigned char* arith_out = malloc(arith_capacity);
    size_t arith_len = arithmetic_encode(rle_data, rle_len, arith_out, arith_capacity);
    free(rle_data);

    // Write compressed output
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

    // ----------- Decompression -------------

    FILE* fin = fopen(outcompressed, "rb");
    int d_w, d_h, d_ch;
    size_t d_size;
    fread(&d_w, sizeof(int), 1, fin);
    fread(&d_h, sizeof(int), 1, fin);
    fread(&d_ch, sizeof(int), 1, fin);
    fread(&d_size, sizeof(size_t), 1, fin);

    unsigned char* enc_data = malloc(d_size);
    fread(enc_data, 1, d_size, fin);
    fclose(fin);

    // Decode arithmetic coded data to RLE stream
    size_t rle_bufcap = d_w * d_h * d_ch * 2;
    unsigned char* rle_decoded = malloc(rle_bufcap);
    size_t rle_dec_len = arithmetic_decode(enc_data, d_size, rle_decoded, rle_bufcap);
    free(enc_data);

    // Decode RLE stream to residuals
    size_t dec_total = d_w * d_h * d_ch;
    unsigned char* decoded_residuals = rle_decode(rle_decoded, rle_dec_len, dec_total);
    free(rle_decoded);

    // Separate residual channels
    size_t px_dec = d_w * d_h;
    uint8_t* res_r_dec = malloc(px_dec);
    uint8_t* res_g_dec = malloc(px_dec);
    uint8_t* res_b_dec = malloc(px_dec);
    memcpy(res_r_dec, decoded_residuals, px_dec);
    memcpy(res_g_dec, decoded_residuals + px_dec, px_dec);
    memcpy(res_b_dec, decoded_residuals + 2 * px_dec, px_dec);
    free(decoded_residuals);

    // Inverse LOCO-I prediction
    uint8_t* img_r = malloc(px_dec);
    uint8_t* img_g = malloc(px_dec);
    uint8_t* img_b = malloc(px_dec);
    inverse_predict_loco_i(res_r_dec, img_r, d_w, d_h);
    inverse_predict_loco_i(res_g_dec, img_g, d_w, d_h);
    inverse_predict_loco_i(res_b_dec, img_b, d_w, d_h);

    free(res_r_dec); free(res_g_dec); free(res_b_dec);

    // Interleave and write decoded image
    unsigned char* decoded_img = malloc(px_dec * 3);
    for (size_t i = 0; i < px_dec; i++) {
        decoded_img[3 * i] = img_r[i];
        decoded_img[3 * i + 1] = img_g[i];
        decoded_img[3 * i + 2] = img_b[i];
    }
    stbi_write_bmp(outdecoded, d_w, d_h, 3, decoded_img);

    free(img_r); free(img_g); free(img_b);
    free(decoded_img);

    printf("Decompression complete. Output saved to %s\n", outdecoded);

    return 0;
}
