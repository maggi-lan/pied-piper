#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../libs/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../libs/stb_image_write.h"

// Helper: get file size
long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0)
        return st.st_size;
    return -1;
}

// Helper: elapsed time in seconds (using CLOCK_MONOTONIC)
double elapsed(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) +
           (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main() {
    // Only change this line:
    const char *input_path = "static/venice.bmp";

    // Derived paths
    const char *compressed = "static/compressed.pp";
    const char *decoded = "static/decoded.bmp";
    const char *png_path = "static/converted.png";

    printf("=== BENCHMARK START ===\n");
    printf("Input: %s\nCompressed: %s\nDecoded: %s\nPNG: %s\n\n",
           input_path, compressed, decoded, png_path);

    struct timespec start, mid, end, png_start, png_end;

    // --- compression + decompression phase ---
    clock_gettime(CLOCK_MONOTONIC, &start);
    int comp_status = system("./algo1 static/venice.bmp static/compressed.pp static/decoded.bmp > /dev/null 2>&1");
    clock_gettime(CLOCK_MONOTONIC, &mid);

    if (comp_status != 0) {
        printf("❌ Compression/Decompression program failed.\n");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long input_size = get_file_size(input_path);
    long compressed_size = get_file_size(compressed);
    long output_size = get_file_size(decoded);

    if (input_size < 0 || compressed_size < 0 || output_size < 0) {
        printf("❌ Could not read one of the files for benchmarking.\n");
        return 1;
    }

    double t_compress = elapsed(start, mid);
    double t_total = elapsed(start, end);

    printf("✅ Custom Compression Complete\n");
    printf("Input size:       %ld bytes\n", input_size);
    printf("Compressed size:  %ld bytes\n", compressed_size);
    printf("Decoded size:     %ld bytes\n", output_size);
    printf("Compression ratio: %.2fx smaller\n",
           (double)input_size / (double)compressed_size);

    printf("\n⏱ Timing (Custom Algorithm):\n");
    printf("Compression time:   %.6f s\n", t_compress);
    printf("Total runtime:      %.6f s (includes decompression)\n", t_total);

    // --- PNG Conversion using stbi ---
    printf("\n--- PNG Conversion Benchmark (stbi) ---\n");

    int w, h, c;
    clock_gettime(CLOCK_MONOTONIC, &png_start);
    unsigned char *img = stbi_load(input_path, &w, &h, &c, 3); // force 3 channels
    if (!img) {
        printf("❌ Failed to load input for PNG conversion: %s\n", stbi_failure_reason());
        return 1;
    }

    if (!stbi_write_png(png_path, w, h, 3, img, w * 3)) {
        printf("❌ Failed to write PNG.\n");
        stbi_image_free(img);
        return 1;
    }
    stbi_image_free(img);
    clock_gettime(CLOCK_MONOTONIC, &png_end);

    long png_size = get_file_size(png_path);
    double t_png = elapsed(png_start, png_end);

    printf("✅ PNG conversion complete\n");
    printf("PNG size:          %ld bytes\n", png_size);
    printf("Compression ratio: %.2fx smaller\n", (double)input_size / (double)png_size);
    printf("Conversion time:   %.6f s\n", t_png);

    printf("\n=========================\n");
    printf("🏁 Summary:\n");
    printf("Custom algorithm: %.2fx smaller in %.6f s\n",
           (double)input_size / (double)compressed_size, t_total);
    printf("PNG conversion:   %.2fx smaller in %.6f s\n",
           (double)input_size / (double)png_size, t_png);
    printf("=========================\n");

    return 0;
}

