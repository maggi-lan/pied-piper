// benchmark.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

// Get file size in bytes
long get_file_size(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// Get high precision time in seconds
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Run your compression algorithm
int run_custom_compression(const char* input, const char* compressed, const char* decoded) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "./algo \"%s\" \"%s\" \"%s\"", input, compressed, decoded);
    return system(cmd);
}

// Convert image to PNG and return PNG size
long convert_to_png(const char* input, const char* png_output) {
    int width, height, channels;
    unsigned char* img = stbi_load(input, &width, &height, &channels, 3);
    
    if (!img) {
        fprintf(stderr, "Failed to load image for PNG conversion: %s\n", stbi_failure_reason());
        return -1;
    }
    
    // Write PNG
    if (!stbi_write_png(png_output, width, height, 3, img, width * 3)) {
        fprintf(stderr, "Failed to write PNG\n");
        stbi_image_free(img);
        return -1;
    }
    
    stbi_image_free(img);
    
    return get_file_size(png_output);
}

// Verify decoded image matches original
int verify_images_match(const char* original, const char* decoded) {
    int w1, h1, c1, w2, h2, c2;
    unsigned char* img1 = stbi_load(original, &w1, &h1, &c1, 3);
    unsigned char* img2 = stbi_load(decoded, &w2, &h2, &c2, 3);
    
    if (!img1 || !img2) {
        if (img1) stbi_image_free(img1);
        if (img2) stbi_image_free(img2);
        return 0;
    }
    
    if (w1 != w2 || h1 != h2) {
        stbi_image_free(img1);
        stbi_image_free(img2);
        return 0;
    }
    
    size_t size = w1 * h1 * 3;
    int match = (memcmp(img1, img2, size) == 0);
    
    stbi_image_free(img1);
    stbi_image_free(img2);
    
    return match;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_image> <output_csv>\n", argv[0]);
        fprintf(stderr, "Example: %s static/image.bmp results.csv\n", argv[0]);
        fprintf(stderr, "\nThis will:\n");
        fprintf(stderr, "  1. Compress the image using your algorithm\n");
        fprintf(stderr, "  2. Convert the original to PNG\n");
        fprintf(stderr, "  3. Compare compression ratios\n");
        fprintf(stderr, "  4. Append results to output CSV\n");
        return 1;
    }
    
    const char* input = argv[1];
    const char* csv_output = argv[2];
    
    // Create temporary filenames
    char compressed_file[512];
    char decoded_file[512];
    char png_file[512];
    
    snprintf(compressed_file, sizeof(compressed_file), "/tmp/benchmark_compressed_%ld.pp", (long)time(NULL));
    snprintf(decoded_file, sizeof(decoded_file), "/tmp/benchmark_decoded_%ld.bmp", (long)time(NULL));
    snprintf(png_file, sizeof(png_file), "/tmp/benchmark_png_%ld.png", (long)time(NULL));
    
    // Get original file size
    long original_size = get_file_size(input);
    if (original_size < 0) {
        fprintf(stderr, "Error: Cannot access input file: %s\n", input);
        return 1;
    }
    
    // Load image to get dimensions
    int width, height, channels;
    unsigned char* img = stbi_load(input, &width, &height, &channels, 0);
    if (!img) {
        fprintf(stderr, "Error: Cannot load image: %s\n", stbi_failure_reason());
        return 1;
    }
    stbi_image_free(img);
    
    printf("Benchmarking: %s\n", input);
    printf("Image dimensions: %dx%d, channels: %d\n", width, height, channels);
    printf("Original file size: %ld bytes\n\n", original_size);
    
    // Run custom compression
    printf("Running custom compression algorithm...\n");
    double start = get_time();
    int ret = run_custom_compression(input, compressed_file, decoded_file);
    double end = get_time();
    double custom_time = end - start;
    
    if (ret != 0) {
        fprintf(stderr, "Error: Custom compression failed\n");
        return 1;
    }
    
    long custom_size = get_file_size(compressed_file);
    if (custom_size < 0) {
        fprintf(stderr, "Error: Cannot access compressed file\n");
        return 1;
    }
    
    // Verify decompression
    printf("Verifying decompression...\n");
    int verification = verify_images_match(input, decoded_file);
    if (!verification) {
        fprintf(stderr, "Warning: Decoded image does not match original!\n");
    }
    
    // Convert to PNG
    printf("Converting to PNG for comparison...\n");
    start = get_time();
    long png_size = convert_to_png(input, png_file);
    end = get_time();
    double png_time = end - start;
    
    if (png_size < 0) {
        fprintf(stderr, "Error: PNG conversion failed\n");
        return 1;
    }
    
    // Calculate metrics (as ratios, not percentages)
    double custom_ratio = (double)custom_size / original_size;
    double png_ratio = (double)png_size / original_size;
    double relative_performance = (double)custom_size / png_size;
    
    // Print results
    printf("\n=== RESULTS ===\n");
    printf("Custom Algorithm:\n");
    printf("  Compressed size: %ld bytes (%.4fx of original)\n", custom_size, custom_ratio);
    printf("  Time: %.6f seconds\n", custom_time);
    printf("  Verification: %s\n", verification ? "PASS" : "FAIL");
    printf("\nPNG Compression:\n");
    printf("  Compressed size: %ld bytes (%.4fx of original)\n", png_size, png_ratio);
    printf("  Time: %.6f seconds\n", png_time);
    printf("\nComparison:\n");
    printf("  Custom vs PNG: %.4fx (%s than PNG)\n", 
           relative_performance,
           custom_size < png_size ? "better" : "worse");
    printf("  Space difference: %ld bytes\n", png_size - custom_size);
    
    // Write to CSV
    FILE* csv = fopen(csv_output, "a");
    if (!csv) {
        fprintf(stderr, "Error: Cannot open output CSV file\n");
        return 1;
    }
    
    // Check if file is empty (write header)
    fseek(csv, 0, SEEK_END);
    if (ftell(csv) == 0) {
        fprintf(csv, "filename,width,height,channels,original_bytes,custom_bytes,custom_ratio,custom_time,png_bytes,png_ratio,png_time,relative_performance,verified\n");
    }
    
    // Write data
    fprintf(csv, "%s,%d,%d,%d,%ld,%ld,%.6f,%.6f,%ld,%.6f,%.6f,%.6f,%s\n",
            input, width, height, channels, original_size,
            custom_size, custom_ratio, custom_time,
            png_size, png_ratio, png_time,
            relative_performance, verification ? "yes" : "no");
    
    fclose(csv);
    printf("\nResults appended to: %s\n", csv_output);
    
    // Cleanup temporary files
    remove(compressed_file);
    remove(decoded_file);
    remove(png_file);
    
    return 0;
}
