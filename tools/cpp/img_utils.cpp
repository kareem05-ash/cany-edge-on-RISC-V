#include "img_io.h"
#include "tools.h"
#include <cstring>

// Wraps a raw uint8_t buffer into an Image and saves it.
void save_raw_u8(const char *path, const uint8_t *buf, int W, int H) {
    Image tmp(W, H);
    memcpy(tmp.data, buf, W * H);
    save_img(path, tmp); // tmp freed by ~Image()
}