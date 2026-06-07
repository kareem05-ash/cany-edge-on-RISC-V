#ifndef IMG_IO_H
#define IMG_IO_H

#include <cstdint>
#include <cstdlib>

class Image {
    public:
        uint8_t* data;      // pixel value
        int width;          // file width
        int height;         // file height

        // Constructor
        Image(int width, int height): width(width), height(height) {
            data = static_cast<uint8_t*>(aligned_alloc(64, width * height));
        }

        // Destructor
        ~Image() {
            free(data);
        }

        // Avoid cost copies (Prevents Image b = a)
        Image(const Image&)             = delete;
        Image& operator=(const Image&)  = delete;

        // Move Constructor
        Image(Image&& other) noexcept
            : data(other.data), width(other.width), height(other.height) {
                other.data = nullptr;
            }

        // Move Assignment
        Image& operator=(Image&& other) noexcept {
            if (this != &other) {
                data        = other.data;
                width       = other.width;
                height      = other.height;
                other.data  = nullptr;
            }
            return *this;
        }

        // Pixel access - read and write: img(y, x) = 255;
        uint8_t& operator()(int y, int x) {
            return data[y * width + x];
        }

        // Pixel access - read only: uint8_t val = img(y, x);
        const uint8_t& operator()(int y, int x) const {
            return data[y * width + x];
        }

        // Size = Total number of bytes
        int size() const {
            return width * height;
        }
};

// Load raw grayscale image from file
Image load_img(const char* path, int width, int height);

// Store raw grayscale image to file
void save_img(const char* path, const Image& img);

#endif