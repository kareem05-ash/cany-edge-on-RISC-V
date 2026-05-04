#include <cstdlib>
#include <cstdio>
#include "img_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "mag_dir.h"

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <input.raw> <output.raw> <width> <height>\n", argv[0]);
        return 1;
    }

    const char* input_path  = argv[1];
    const char* output_path = argv[2];
    int width  = atoi(argv[3]);
    int height = atoi(argv[4]);

    // Step 1 — load raw grayscale image
    Image src = load_img(input_path, width, height);

    // Step 2 — gaussian blur (smooth noise before edge detection)
    Image blurred(width, height);
    gaussian_blur(src, blurred);

    // Step 3 — sobel gradients
    int n = width * height;
    int16_t* Gx = new int16_t[n];
    int16_t* Gy = new int16_t[n];
    sobel(blurred, Gx, Gy);

    // Step 4 — gradient magnitude (saved as output)
    Image magnitude(width, height);
    compute_magnitude(Gx, Gy, magnitude.data, width, height, MagMethod::L2);

    // Step 4b — gradient direction (useful for later NMS step)
    uint8_t* direction = new uint8_t[n];
    compute_direction(Gx, Gy, direction, width, height);

    // Save the magnitude image
    save_img(output_path, magnitude);

    printf("Done. Output written to %s\n", output_path);

    delete[] Gx;
    delete[] Gy;
    delete[] direction;
    return 0;
}