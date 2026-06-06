#ifndef IMG_IO_H
#define IMG_IO_H

#include <cstdint>
#include <cstdlib>

/**
 * @file img_io.h
 * @brief Raw grayscale image container and I/O functions.
 *
 * Images are stored as flat arrays of `uint8_t` in **row-major order**:
 * pixel at column `x`, row `y` lives at `data[y * width + x]`.
 *
 * The file format is headerless raw binary: exactly `width * height` bytes,
 * one byte per pixel (0 = black, 255 = white). No compression, no metadata.
 * This eliminates library dependencies and makes boundary math trivial.
 *
 * All buffers are allocated with `aligned_alloc(64, ...)` so that:
 *   - RVV unit-stride loads (`vle8.v`) receive 64-byte aligned pointers.
 *   - The compiler can emit aligned SIMD instructions on the host.
 *   - The allocation size is rounded up to the next 64-byte multiple.
 */

/**
 * @brief Heap-allocated, 64-byte-aligned grayscale image.
 *
 * Non-copyable (copy constructor and copy-assignment are deleted) to
 * prevent accidental deep copies of large pixel buffers.
 * Move constructor and move-assignment transfer ownership of `data`.
 *
 * ### Memory layout
 * ```
 *   data[y * width + x]  →  pixel at column x, row y
 *   total bytes          =  width * height  (rounded up to 64 for alignment)
 * ```
 *
 * ### Usage example
 * ```cpp
 * Image img(640, 480);
 * img(0, 0) = 255;                  // write pixel at (row=0, col=0)
 * uint8_t v = img(100, 200);        // read  pixel at (row=100, col=200)
 * ```
 */
class Image {
  public:
    uint8_t *data; ///< Pixel buffer — row-major, 64-byte aligned.
    int width;     ///< Image width in pixels.
    int height;    ///< Image height in pixels.

    /**
     * @brief Allocate a blank image of the given dimensions.
     *
     * Buffer size is rounded up to the next 64-byte boundary so that
     * `aligned_alloc(64, ...)` pre-condition (size must be a multiple of
     * alignment) is always satisfied.
     *
     * @param width  Width in pixels (must be > 0).
     * @param height Height in pixels (must be > 0).
     */
    Image(int width, int height) : width(width), height(height) {
        data = static_cast<uint8_t *>(aligned_alloc(64, (width * height + 63) & ~63));
    }

    /**
     * @brief Free the pixel buffer.
     *
     * Handles the `nullptr` case produced by the move constructor.
     */
    ~Image() { free(data); }

    /// @brief Deleted — prevents accidental O(W*H) copies.
    Image(const Image &) = delete;
    /// @brief Deleted — prevents accidental O(W*H) copies.
    Image &operator=(const Image &) = delete;

    /**
     * @brief Move constructor — transfers buffer ownership in O(1).
     * @param other Source image; `other.data` is set to `nullptr` after move.
     */
    Image(Image &&other) noexcept : data(other.data), width(other.width), height(other.height) {
        other.data = nullptr;
    }

    /**
     * @brief Move-assignment — frees current buffer, then transfers ownership.
     * @param other Source image; `other.data` is set to `nullptr` after move.
     * @return Reference to `*this`.
     */
    Image &operator=(Image &&other) noexcept {
        if (this != &other) {
            free(data);
            data = other.data;
            width = other.width;
            height = other.height;
            other.data = nullptr;
        }
        return *this;
    }

    /**
     * @brief Read-write pixel access via (row, col) coordinates.
     *
     * Converts 2-D coordinates to the flat 1-D index `y * width + x`.
     * No bounds checking — caller is responsible for valid indices.
     *
     * @param y Row index in [0, height).
     * @param x Column index in [0, width).
     * @return Reference to the pixel byte, assignable.
     */
    uint8_t &operator()(int y, int x) { return data[y * width + x]; }

    /**
     * @brief Read-only pixel access via (row, col) coordinates.
     * @param y Row index in [0, height).
     * @param x Column index in [0, width).
     * @return `const` reference to the pixel byte.
     */
    const uint8_t &operator()(int y, int x) const { return data[y * width + x]; }

    /**
     * @brief Total number of pixels (= total buffer bytes before alignment padding).
     * @return `width * height`.
     */
    int size() const { return width * height; }
};

/**
 * @brief Load a raw grayscale image from disk into an aligned `Image`.
 *
 * The file must be exactly `width * height` bytes — no header, no footer.
 * If the file cannot be opened or is the wrong size, the function prints
 * an error to `stderr` and calls `exit(1)`.
 *
 * @param path   Path to the `.raw` file.
 * @param width  Expected image width in pixels.
 * @param height Expected image height in pixels.
 * @return       Newly allocated `Image` containing the file's pixel data.
 */
Image load_img(const char *path, int width, int height);

/**
 * @brief Save an `Image` to disk as raw grayscale bytes.
 *
 * Writes exactly `img.width * img.height` bytes in row-major order.
 * If the file cannot be created or the write is short, prints an error
 * to `stderr` and calls `exit(1)`.
 *
 * @param path Path to the output `.raw` file (created or overwritten).
 * @param img  Image to save.
 */
void save_img(const char *path, const Image &img);

#endif // IMG_IO_H