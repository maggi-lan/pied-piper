#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

#include "libs/arith.h"

// Helper function to get file size
long get_file_size(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}

// Helper to calculate elapsed time in milliseconds
double time_diff_ms(clock_t start, clock_t end) {
    return ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
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

int main() {
    const char* inpath = "static/venice.bmp";
    const char* outcompressed = "static/compressed.pp";
    const char* outdecoded = "static/decoded.bmp";
    const char* outpng = "static/venice_convert.png";

    clock_t start, end;
    double compression_time_ms, decompression_time_ms, png_time_ms;

    // Get original file size
    long original_file_size = get_file_size(inpath);

    printf("=== BENCHMARKING COMPRESSION ALGORITHMS ===\n\n");
    printf("Input image: %s\n", inpath);
    printf("Original file size: %ld bytes\n\n", original_file_size);

    // ============ YOUR ALGORITHM - COMPRESSION ============
    start = clock();
    
    int width, height, channels;
    unsigned char* img = stbi_load(inpath, &width, &height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }

    printf("Image dimensions: %dx%d\n", width, height);
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

    // Keep a copy for PNG conversion
    unsigned char* img_copy = malloc(px_count * 3);
    memcpy(img_copy, img, px_count * 3);
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

    end = clock();
    compression_time_ms = time_diff_ms(start, end);

    long compressed_file_size = get_file_size(outcompressed);
    double your_compression_ratio = (double)total_len / (double)arith_len;
    double your_compression_speed = (total_len / 1024.0 / 1024.0) / (compression_time_ms / 1000.0);

    // ============ YOUR ALGORITHM - DECOMPRESSION ============
    start = clock();
    
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

    end = clock();
    decompression_time_ms = time_diff_ms(start, end);
    double your_decompression_speed = (total_len / 1024.0 / 1024.0) / (decompression_time_ms / 1000.0);

    // ============ PNG COMPRESSION ============
    start = clock();
    
    // PNG compression level 9 (maximum)
    stbi_write_png_compression_level = 9;
    stbi_write_png(outpng, width, height, 3, img_copy, width * 3);
    
    end = clock();
    png_time_ms = time_diff_ms(start, end);
    
    long png_file_size = get_file_size(outpng);
    double png_compression_ratio = (double)total_len / (double)png_file_size;
    double png_compression_speed = (total_len / 1024.0 / 1024.0) / (png_time_ms / 1000.0);

    free(img_copy);

    // ============ RESULTS ============
    printf("\n=== BENCHMARK RESULTS ===\n\n");
    
    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│ YOUR ALGORITHM (LOCO-I + RLE + Arithmetic)             │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    printf("│ Uncompressed size:      %8zu bytes (%6.2f MB)    │\n", 
           total_len, total_len / 1024.0 / 1024.0);
    printf("│ Compressed size:        %8zu bytes (%6.2f MB)    │\n", 
           arith_len, arith_len / 1024.0 / 1024.0);
    printf("│ File size (w/ header):  %8ld bytes (%6.2f MB)    │\n", 
           compressed_file_size, compressed_file_size / 1024.0 / 1024.0);
    printf("│                                                         │\n");
    printf("│ Compression ratio:      %.2f:1                          │\n", 
           your_compression_ratio);
    printf("│ Space savings:          %.2f%%                          │\n", 
           (1.0 - (double)arith_len / total_len) * 100.0);
    printf("│                                                         │\n");
    printf("│ Compression time:       %.2f ms                         │\n", 
           compression_time_ms);
    printf("│ Compression speed:      %.2f MB/s                       │\n", 
           your_compression_speed);
    printf("│                                                         │\n");
    printf("│ Decompression time:     %.2f ms                         │\n", 
           decompression_time_ms);
    printf("│ Decompression speed:    %.2f MB/s                       │\n", 
           your_decompression_speed);
    printf("└─────────────────────────────────────────────────────────┘\n\n");

    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│ PNG (Level 9 - Maximum Compression)                    │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    printf("│ Uncompressed size:      %8zu bytes (%6.2f MB)    │\n", 
           total_len, total_len / 1024.0 / 1024.0);
    printf("│ PNG file size:          %8ld bytes (%6.2f MB)    │\n", 
           png_file_size, png_file_size / 1024.0 / 1024.0);
    printf("│                                                         │\n");
    printf("│ Compression ratio:      %.2f:1                          │\n", 
           png_compression_ratio);
    printf("│ Space savings:          %.2f%%                          │\n", 
           (1.0 - (double)png_file_size / total_len) * 100.0);
    printf("│                                                         │\n");
    printf("│ Conversion time:        %.2f ms                         │\n", 
           png_time_ms);
    printf("│ Conversion speed:       %.2f MB/s                       │\n", 
           png_compression_speed);
    printf("└─────────────────────────────────────────────────────────┘\n\n");

    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│ COMPARISON                                              │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    
    if (arith_len < png_file_size) {
        printf("│ ✓ Your algorithm is %.2f%% smaller than PNG           │\n",
               ((double)png_file_size / arith_len - 1.0) * 100.0);
    } else {
        printf("│ ✗ PNG is %.2f%% smaller than your algorithm            │\n",
               ((double)arith_len / png_file_size - 1.0) * 100.0);
    }
    
    if (compression_time_ms < png_time_ms) {
        printf("│ ✓ Your compression is %.2fx faster than PNG           │\n",
               png_time_ms / compression_time_ms);
    } else {
        printf("│ ✗ PNG is %.2fx faster than your compression            │\n",
               compression_time_ms / png_time_ms);
    }
    
    printf("│                                                         │\n");
    printf("│ Compression ratio comparison:                           │\n");
    printf("│   Your algorithm: %.2f:1                                │\n", 
           your_compression_ratio);
    printf("│   PNG:            %.2f:1                                │\n", 
           png_compression_ratio);
    printf("│   Difference:     %.2fx                                 │\n",
           your_compression_ratio / png_compression_ratio);
    printf("└─────────────────────────────────────────────────────────┘\n\n");

    printf("Output files:\n");
    printf("  - Compressed (your algorithm): %s\n", outcompressed);
    printf("  - Decompressed: %s\n", outdecoded);
    printf("  - PNG: %s\n", outpng);

    return 0;
}
