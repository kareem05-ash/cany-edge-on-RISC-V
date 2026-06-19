#include "img_io.h"
#include <cstdio>
#include <cstdlib>

Image load_img(const char *path, int width, int height) {
    // Open file in binary read mode
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: can't open file %s\n", path);
        exit(1);
    }

    // Create Image Object
    Image img(width, height);

    // Read exactly width*height bytees from file into buffer
    size_t bytes_read = fread(img.data, 1, img.size(), f);

    // Invalid width/height
    if ((int)bytes_read != img.size()) {
        fprintf(stderr, "Error: expected %d bytes, got %zu from %s\n", img.size(), bytes_read,
                path);
        fclose(f);
        exit(1);
    }

    fclose(f);
    return img;
}

void save_img(const char *path, const Image &img) {
    // Open file in binary write mode
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: can't create file %s\n", path);
        exit(1);
    }

    // Write width*height bytes from buffer
    size_t bytes_written = fwrite(img.data, 1, img.size(), f);

    // Invalid width/hegith
    if ((int)bytes_written != img.size()) {
        fprintf(stderr, "Error: expected to write %d bytes, wrote %zu to %s\n", img.size(),
                bytes_written, path);
        fclose(f);
        exit(1);
    }

    fclose(f);
}

void save_raw_i16(const char *path, const int16_t *buf, int W, int H) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: can't create file %s\n", path);
        exit(1);
    }
    size_t n = (size_t)(W * H);
    size_t written = fwrite(buf, sizeof(int16_t), n, f);
    if (written != n) {
        fprintf(stderr, "Error: expected to write %zu elements, wrote %zu to %s\n",
                n, written, path);
        fclose(f);
        exit(1);
    }
    fclose(f);
}