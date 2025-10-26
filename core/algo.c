#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

#include "libs/arith.h"

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
            residuals[idx] = (uint8_t)res;
        }
    }
}

void inverse_predict_loco_i(const uint8_t* resid, uint8_t* out, int wid, int ht) {
    for (int y=0; y < ht; y++) {
        for (int x=0; x < wid; x++) {
            int idx = y * wid + x;
            int a = x > 0 ? out[y * wid + (x - 1)] : 0;
            int b = y > 0 ? out[(y - 1) * wid + x] : 0;
            int c = (x > 0 && y > 0) ? out[(y - 1) * wid + (x - 1)] : 0;
            int pred = loco_predict(a, b, c);

            int val = (pred + resid[idx]) & 0xFF;
            out[idx] = (uint8_t)val;
        }
    }
}

unsigned char* rle_encode(const unsigned char* data, size_t len, size_t* out_len) {
    size_t capacity = len * 2 + 1024;
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

unsigned char* rle_decode(const unsigned char* data, size_t len, size_t out_len) {
    unsigned char* out = malloc(out_len);
    if (!out) return NULL;

    size_t pos = 0, i = 0;
    while (i + 1 < len && pos < out_len) {
        unsigned char run = data[i++];
        unsigned char val = data[i++];
        for (int j = 0; j < run && pos < out_len; j++) {
            out[pos++] = val;
        }
    }

    while (pos < out_len) {
        out[pos++] = 0;
    }

    return out;
}

int main(int argc, char* argv[]) {
    // Check for correct number of arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input.bmp> <compressed.pp> <decoded.bmp>\n", argv[0]);
        fprintf(stderr, "Example: %s static/venice.bmp static/compressed.pp static/decoded.bmp\n", argv[0]);
        return 1;
    }

    const char* inpath = argv[1];
    const char* outcompressed = argv[2];
    const char* outdecoded = argv[3];

    // ============ COMPRESSION ============
    int width, height, channels;
    unsigned char* img = stbi_load(inpath, &width, &height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }

    printf("Compressing %dx%d image...\n", width, height);
    size_t px_count = (size_t)width * height;
    size_t total_len = px_count * 3;

    // Separate channels
    uint8_t* r_chan = malloc(px_count);
    uint8_t* g_chan = malloc(px_count);
    uint8_t* b_chan = malloc(px_count);
    for (size_t i = 0; i < px_count; i++) {
        r_chan[i] = img[3*i];
        g_chan[i] = img[3*i + 1];
        b_chan[i] = img[3*i + 2];
    }
    free(img);

    // Compute residuals
    uint8_t* res_r = malloc(px_count);
    uint8_t* res_g = malloc(px_count);
    uint8_t* res_b = malloc(px_count);
    compute_residuals(r_chan, width, height, res_r);
    compute_residuals(g_chan, width, height, res_g);
    compute_residuals(b_chan, width, height, res_b);
    free(r_chan);
    free(g_chan);
    free(b_chan);

    // Concatenate
    unsigned char* combined = malloc(total_len);
    memcpy(combined, res_r, px_count);
    memcpy(combined + px_count, res_g, px_count);
    memcpy(combined + 2 * px_count, res_b, px_count);
    free(res_r);
    free(res_g);
    free(res_b);

    // RLE encode
    size_t rle_len;
    unsigned char* rle_data = rle_encode(combined, total_len, &rle_len);
    free(combined);

    // Arithmetic encode
    size_t arith_capacity = rle_len + 4096;
    unsigned char* arith_out = malloc(arith_capacity);
    size_t arith_len = arithmetic_encode(rle_data, rle_len, arith_out, arith_capacity);
    free(rle_data);

    // Write file
    FILE* fout = fopen(outcompressed, "wb");
    if (!fout) {
        fprintf(stderr, "Cannot write output file\n");
        return 1;
    }
    fwrite(&width, sizeof(int), 1, fout);
    fwrite(&height, sizeof(int), 1, fout);
    int nchannels = 3;
    fwrite(&nchannels, sizeof(int), 1, fout);
    fwrite(&total_len, sizeof(size_t), 1, fout);
    fwrite(&rle_len, sizeof(size_t), 1, fout);
    fwrite(&arith_len, sizeof(size_t), 1, fout);
    fwrite(arith_out, 1, arith_len, fout);
    fclose(fout);
    free(arith_out);

    printf("Compressed: %zu -> %zu bytes (%.1f%%)\n",
           total_len, arith_len, 100.0 * arith_len / total_len);

    // ============ DECOMPRESSION ============
    FILE* fin = fopen(outcompressed, "rb");
    if (!fin) {
        fprintf(stderr, "Cannot read compressed file\n");
        return 1;
    }

    int d_w, d_h, d_ch;
    size_t d_total, d_rle, d_arith;
    fread(&d_w, sizeof(int), 1, fin);
    fread(&d_h, sizeof(int), 1, fin);
    fread(&d_ch, sizeof(int), 1, fin);
    fread(&d_total, sizeof(size_t), 1, fin);
    fread(&d_rle, sizeof(size_t), 1, fin);
    fread(&d_arith, sizeof(size_t), 1, fin);

    unsigned char* enc_data = malloc(d_arith);
    fread(enc_data, 1, d_arith, fin);
    fclose(fin);

    printf("Decompressing...\n");

    // Arithmetic decode
    unsigned char* rle_decoded = malloc(d_rle);
    arithmetic_decode(enc_data, d_arith, rle_decoded, d_rle);
    free(enc_data);

    // RLE decode
    unsigned char* decoded_residuals = rle_decode(rle_decoded, d_rle, d_total);
    free(rle_decoded);

    // Separate channels
    size_t px_dec = d_w * d_h;
    uint8_t* res_r_dec = malloc(px_dec);
    uint8_t* res_g_dec = malloc(px_dec);
    uint8_t* res_b_dec = malloc(px_dec);
    memcpy(res_r_dec, decoded_residuals, px_dec);
    memcpy(res_g_dec, decoded_residuals + px_dec, px_dec);
    memcpy(res_b_dec, decoded_residuals + 2 * px_dec, px_dec);
    free(decoded_residuals);

    // Inverse prediction
    uint8_t* img_r = calloc(px_dec, 1);
    uint8_t* img_g = calloc(px_dec, 1);
    uint8_t* img_b = calloc(px_dec, 1);

    inverse_predict_loco_i(res_r_dec, img_r, d_w, d_h);
    inverse_predict_loco_i(res_g_dec, img_g, d_w, d_h);
    inverse_predict_loco_i(res_b_dec, img_b, d_w, d_h);
    free(res_r_dec);
    free(res_g_dec);
    free(res_b_dec);

    // Interleave and write
    unsigned char* decoded_img = malloc(px_dec * 3);
    for (size_t i = 0; i < px_dec; i++) {
        decoded_img[3 * i] = img_r[i];
        decoded_img[3 * i + 1] = img_g[i];
        decoded_img[3 * i + 2] = img_b[i];
    }

    stbi_write_bmp(outdecoded, d_w, d_h, 3, decoded_img);
    free(img_r);
    free(img_g);
    free(img_b);
    free(decoded_img);

    printf("Done! Saved to %s\n", outdecoded);
    return 0;
}
