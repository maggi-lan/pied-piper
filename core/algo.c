#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

typedef struct {
    unsigned char r, g, b;
} Pixel;

/* Chunk opcodes (each chunk starts with one opcode byte) */
#define OP_RLE   0x00  // [OP_RLE][run:1][R][G][B]
#define OP_RAW   0x01  // [OP_RAW][R][G][B]
#define OP_DIFF  0x02  // [OP_DIFF][packed_byte]
#define OP_LUMA  0x03  // [OP_LUMA][dg+32][(dr-dg+8)<<4 | (db-dg+8)]

/* utility: clamp 0..255 */
static inline unsigned char clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

/* convert stbi buffer (row-major contiguous) to Pixel array (RGB) */
Pixel* convert_pixels(unsigned char* data, int width, int height, int channels) {
    Pixel* pixels = malloc((size_t)width * height * sizeof(Pixel));
    if (!pixels) return NULL;
    for (int i = 0; i < width * height; ++i) {
        pixels[i].r = data[i * channels + 0];
        pixels[i].g = data[i * channels + 1];
        pixels[i].b = data[i * channels + 2];
    }
    return pixels;
}

/* Compress with RLE + QOI-like DIFF (small) + LUMA (medium) + RAW fallback.
   Format is chunk-based and unambiguous (every chunk starts with one opcode byte). */
void compress_chunks(Pixel* pixels, int total_pixels, const char* out_file) {
    FILE* out = fopen(out_file, "wb");
    if (!out) { perror("open out"); return; }

    if (total_pixels <= 0) { fclose(out); return; }

    // Write the first pixel as RAW (so decoder knows the starting color)
    Pixel prev = pixels[0];
    fputc(OP_RAW, out);
    fputc(prev.r, out);
    fputc(prev.g, out);
    fputc(prev.b, out);

    int run = 0; // counts repeats *after* the first appearance
    for (int i = 1; i < total_pixels; ++i) {
        Pixel cur = pixels[i];

        if (cur.r == prev.r && cur.g == prev.g && cur.b == prev.b) {
            // repeating pixel
            run++;
            // flush if we reached max storable run (255)
            if (run == 255) {
                fputc(OP_RLE, out);
                fputc(run, out);
                fputc(prev.r, out);
                fputc(prev.g, out);
                fputc(prev.b, out);
                run = 0;
            }
            // don't update prev here (prev remains the repeated color)
            continue;
        }

        // if repeat sequence ended, flush RLE chunk
        if (run > 0) {
            fputc(OP_RLE, out);
            fputc(run, out);
            fputc(prev.r, out);
            fputc(prev.g, out);
            fputc(prev.b, out);
            run = 0;
        }

        // Now cur != prev, attempt DIFF or LUMA or RAW
        int dr = (int)cur.r - (int)prev.r;
        int dg = (int)cur.g - (int)prev.g;
        int db = (int)cur.b - (int)prev.b;

        // DIFF: small diffs in [-2..+1] (mirrors QOI small-diff range)
        if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
            unsigned char packed = (unsigned char)(((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2));
            fputc(OP_DIFF, out);
            fputc(packed, out);
        }
        // LUMA: medium diffs using green as base
        else if (dg >= -32 && dg <= 31) {
            int dr_dg = dr - dg;
            int db_dg = db - dg;
            if (dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                fputc(OP_LUMA, out);
                fputc((unsigned char)(dg + 32), out);
                unsigned char second = (unsigned char)(((dr_dg + 8) << 4) | ((db_dg + 8) & 0x0F));
                fputc(second, out);
            } else {
                // fallback to RAW
                fputc(OP_RAW, out);
                fputc(cur.r, out);
                fputc(cur.g, out);
                fputc(cur.b, out);
            }
        } else {
            // RAW fallback
            fputc(OP_RAW, out);
            fputc(cur.r, out);
            fputc(cur.g, out);
            fputc(cur.b, out);
        }

        prev = cur;
    }

    // If the image ended with a run, flush it
    if (run > 0) {
        fputc(OP_RLE, out);
        fputc(run, out);
        fputc(prev.r, out);
        fputc(prev.g, out);
        fputc(prev.b, out);
        run = 0;
    }

    fclose(out);
    printf("Compression complete. Output saved to %s\n", out_file);
}

/* Decode the chunk stream produced by compress_chunks */
Pixel* decode_chunks(const char* in_file, int total_pixels) {
    FILE* in = fopen(in_file, "rb");
    if (!in) { perror("open in"); return NULL; }
    Pixel* out_pixels = malloc((size_t)total_pixels * sizeof(Pixel));
    if (!out_pixels) { fclose(in); return NULL; }

    int count = 0;
    Pixel prev = {0,0,0};
    while (count < total_pixels) {
        int code = fgetc(in);
        if (code == EOF) break;

        if (code == OP_RLE) {
            int run = fgetc(in);
            int r = fgetc(in);
            int g = fgetc(in);
            int b = fgetc(in);
            if (run == EOF || r == EOF || g == EOF || b == EOF) break;
            Pixel px = {(unsigned char)r,(unsigned char)g,(unsigned char)b};
            for (int i = 0; i < run && count < total_pixels; ++i) {
                out_pixels[count++] = px;
            }
            prev = px;
        }
        else if (code == OP_RAW) {
            int r = fgetc(in);
            int g = fgetc(in);
            int b = fgetc(in);
            if (r == EOF || g == EOF || b == EOF) break;
            Pixel px = {(unsigned char)r,(unsigned char)g,(unsigned char)b};
            out_pixels[count++] = px;
            prev = px;
        }
        else if (code == OP_DIFF) {
            int packed = fgetc(in);
            if (packed == EOF) break;
            int dr = ((packed >> 4) & 0x03) - 2;
            int dg = ((packed >> 2) & 0x03) - 2;
            int db = (packed & 0x03) - 2;
            Pixel px = {
                clamp_u8((int)prev.r + dr),
                clamp_u8((int)prev.g + dg),
                clamp_u8((int)prev.b + db)
            };
            out_pixels[count++] = px;
            prev = px;
        }
        else if (code == OP_LUMA) {
            int dg_biased = fgetc(in);
            int second = fgetc(in);
            if (dg_biased == EOF || second == EOF) break;
            int dg = dg_biased - 32;
            int dr_dg = ((second >> 4) & 0x0F) - 8;
            int db_dg = (second & 0x0F) - 8;
            Pixel px = {
                clamp_u8((int)prev.r + dg + dr_dg),
                clamp_u8((int)prev.g + dg),
                clamp_u8((int)prev.b + dg + db_dg)
            };
            out_pixels[count++] = px;
            prev = px;
        }
        else {
            // unknown code -> fail safe: stop
            fprintf(stderr, "Unknown opcode 0x%02X at decoded count %d\n", code, count);
            break;
        }
    }

    fclose(in);
    if (count != total_pixels) {
        fprintf(stderr, "Warning: decoded pixel count %d != expected %d\n", count, total_pixels);
    } else {
        printf("Decoded %d pixels successfully\n", count);
    }
    return out_pixels;
}

/* ------------------- Main ------------------- */
int main(void) {
    const char *inpath = "static/snail.bmp";
    const char *out_compressed = "static/compressed.pp";
    const char *out_decoded_bmp = "static/decoded.bmp";

    int width, height, channels;
    unsigned char *img = stbi_load(inpath, &width, &height, &channels, 0);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }
    if (channels < 3) {
        fprintf(stderr, "Image has fewer than 3 channels (%d)\n", channels);
        stbi_image_free(img);
        return 1;
    }

    printf("Loaded image: %dx%d channels=%d\n", width, height, channels);

    int total_pixels = width * height;
    Pixel* pixels = convert_pixels(img, width, height, channels);
    stbi_image_free(img);
    if (!pixels) { fprintf(stderr, "OOM\n"); return 1; }

    compress_chunks(pixels, total_pixels, out_compressed);

    Pixel* decoded = decode_chunks(out_compressed, total_pixels);
    if (!decoded) { free(pixels); return 1; }

    // write decoded image for verification
    unsigned char *buf = malloc((size_t)total_pixels * 3);
    if (!buf) { fprintf(stderr,"OOM\n"); free(pixels); free(decoded); return 1; }
    for (int i = 0; i < total_pixels; ++i) {
        buf[i*3 + 0] = decoded[i].r;
        buf[i*3 + 1] = decoded[i].g;
        buf[i*3 + 2] = decoded[i].b;
    }
    stbi_write_bmp(out_decoded_bmp, width, height, 3, buf);
    printf("Decoded image written to %s\n", out_decoded_bmp);

    free(pixels);
    free(decoded);
    free(buf);
    return 0;
}

