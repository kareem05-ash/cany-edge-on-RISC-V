#include "img_io.h"
#include "tools.h"
#include <cstdio>
#include <cstring>

// Wraps a raw uint8_t buffer into an Image and saves it.
void save_raw_u8(const char *path, const uint8_t *buf, int W, int H) {
    Image tmp(W, H);
    memcpy(tmp.data, buf, W * H);
    save_img(path, tmp); // tmp freed by ~Image()
}
void save_raw_i16(const char *path, const int16_t *buf, int W, int H) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("Failed to open file for writing");
        return;
    }
    fwrite(buf, sizeof(int16_t), W * H, f);
    fclose(f);
}