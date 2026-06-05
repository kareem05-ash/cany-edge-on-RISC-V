#include "edge_refinement.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>

// ============================================
// Non-Maximum Suppression
// ============================================

void nms(const uint8_t *mag, const uint8_t *dir, uint8_t *out, int W, int H) {
    // Loop through each pixel (skipping borders to avoid out-of-bounds access)
    for (int y = 1; y < H - 1; y++) {
        for (int x = 1; x < W - 1; x++) {
            // Calculate the current pixel's position in the 1D array
            // Formula: y * width + x converts 2D coordinates to 1D index
            int idx = y * W + x;

            // Get the gradient magnitude at current pixel
            uint8_t mag_val = mag[idx];

            // Get the gradient direction (0, 1, 2, or 3)
            // Direction tells us which neighbors to compare with
            uint8_t direction = dir[idx];

            // Initialize a flag: assume this pixel IS a local maximum
            uint8_t is_max = 1;

            // Check neighbors based on gradient direction
            // Each direction checks 2 neighbors along that gradient line

            if (direction == 0) {
                // Direction 0: HORIZONTAL (left-right)
                // Gradient points left-right, so check left and right neighbors
                int left_idx = y * W + (x - 1);  // pixel to the left
                int right_idx = y * W + (x + 1); // pixel to the right

                // If magnitude is less than left OR right neighbor, it's NOT a max
                if (mag_val < mag[left_idx] || mag_val < mag[right_idx]) {
                    is_max = 0; // Mark as not a maximum
                }
            } else if (direction == 1) {
                // Direction 1: DIAGONAL (top-left to bottom-right)
                // Check diagonal neighbors
                int tl_idx = (y - 1) * W + (x - 1); // top-left
                int br_idx = (y + 1) * W + (x + 1); // bottom-right

                // If magnitude is less than either diagonal neighbor, it's NOT a max
                if (mag_val < mag[tl_idx] || mag_val < mag[br_idx]) {
                    is_max = 0;
                }
            } else if (direction == 2) {
                // Direction 2: VERTICAL (top-bottom)
                // Gradient points up-down, so check top and bottom neighbors
                int top_idx = (y - 1) * W + x; // pixel above
                int bot_idx = (y + 1) * W + x; // pixel below

                // If magnitude is less than top OR bottom neighbor, it's NOT a max
                if (mag_val < mag[top_idx] || mag_val < mag[bot_idx]) {
                    is_max = 0;
                }
            } else if (direction == 3) {
                // Direction 3: DIAGONAL (top-right to bottom-left)
                // Check opposite diagonal neighbors
                int tr_idx = (y - 1) * W + (x + 1); // top-right
                int bl_idx = (y + 1) * W + (x - 1); // bottom-left

                // If magnitude is less than either diagonal neighbor, it's NOT a max
                if (mag_val < mag[tr_idx] || mag_val < mag[bl_idx]) {
                    is_max = 0;
                }
            }

            // Store result: keep magnitude if local max, otherwise suppress to 0
            out[idx] = is_max ? mag_val : 0;
        }
    }

    // Handle border pixels: suppress them all (no enough neighbors to compare)
    // Top and bottom rows
    for (int x = 0; x < W; x++) {
        out[0 * W + x] = 0;       // Top row
        out[(H - 1) * W + x] = 0; // Bottom row
    }

    // Left and right columns
    for (int y = 0; y < H; y++) {
        out[y * W + 0] = 0;       // Left column
        out[y * W + (W - 1)] = 0; // Right column
    }
}

// ============================================
// Double Thresholding
// ============================================

void double_threshold(const uint8_t *in, uint8_t *out, int W, int H, uint8_t t_low,
                      uint8_t t_high) {
    // Loop through every pixel in the image
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // Calculate the 1D array index from 2D coordinates
            // This converts the 2D position (x,y) into a single array position
            int idx = y * W + x;

            // Get the gradient magnitude at this pixel from NMS output
            // This value is between 0 and 255
            uint8_t mag_val = in[idx];

            // First threshold check: is magnitude above the HIGH threshold?
            // HIGH threshold = definite strong edge (usually ~0.15 * max_magnitude)
            if (mag_val > t_high) {
                // YES: This is a STRONG edge, assign maximum value (255)
                out[idx] = 255;
            }
            // Second threshold check: is magnitude between LOW and HIGH?
            // LOW threshold = possible weak edge (usually ~0.05 * max_magnitude)
            else if (mag_val >= t_low) {
                // YES: This is a WEAK edge, assign middle value (128)
                // Will be processed by hysteresis later
                out[idx] = 128;
            }
            // Otherwise: magnitude is below LOW threshold
            else {
                // NO: This is NOT an edge, suppress to 0
                out[idx] = 0;
            }
        }
    }
}

// ============================================
// Hyseresis
// ============================================

void hysteresis(const uint8_t *in, uint8_t *out, int W, int H) {
    // Copy input to output (we'll modify output as we process)
    // memcpy copies the entire image from 'in' to 'out'
    memcpy(out, in, W * H * sizeof(uint8_t));

    // Create a queue to track pixels to process (BFS)
    // We'll use a simple queue implementation for BFS traversal
    std::queue<int> q;

    // Step 1: Find all STRONG pixels (255) and add them to the queue
    // These are the "seeds" for our BFS exploration
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // Calculate 1D array index from 2D coordinates
            int idx = y * W + x;

            // If this pixel is STRONG (definite edge), add to queue
            if (out[idx] == 255) {
                q.push(idx); // Add index to the queue
            }
        }
    }

    // Step 2: BFS from all STRONG pixels to promote connected WEAK pixels
    // Process queue while it's not empty
    while (!q.empty()) {
        // Get the first pixel from the queue
        int idx = q.front();
        q.pop();

        // Convert 1D index back to 2D coordinates
        // y = idx / W (integer division)
        // x = idx % W (remainder)
        int y = idx / W;
        int x = idx % W;

        // Check all 8 neighbors (4-connectivity: up,down,left,right + 4 diagonals)
        // We use offsets to check neighbors efficiently
        int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1}; // x-offsets for 8 neighbors
        int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1}; // y-offsets for 8 neighbors

        // Loop through all 8 neighbors
        for (int i = 0; i < 8; i++) {
            // Calculate neighbor's coordinates
            int ny = y + dy[i]; // neighbor's y-coordinate
            int nx = x + dx[i]; // neighbor's x-coordinate

            // Check if neighbor is within image bounds
            // Prevent out-of-bounds array access
            if (ny >= 0 && ny < H && nx >= 0 && nx < W) {
                // Calculate neighbor's 1D index
                int n_idx = ny * W + nx;

                // If neighbor is WEAK (128), we found a connected weak edge!
                if (out[n_idx] == 128) {
                    // Promote WEAK to STRONG (this is the key operation!)
                    out[n_idx] = 255;

                    // Add this newly promoted pixel to queue for further exploration
                    // This ensures we'll check ITS neighbors too (BFS continuation)
                    q.push(n_idx);
                }
            }
        }
    }

    // Step 3: Suppress all remaining WEAK pixels (not connected to STRONG)
    // Any WEAK pixel left at this point is isolated (not connected to edges)
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // Calculate 1D array index
            int idx = y * W + x;

            // If pixel is still WEAK (128), it wasn't connected to any STRONG pixel
            if (out[idx] == 128) {
                // Suppress it (convert to 0, remove it as noise)
                out[idx] = 0;
            }
        }
    }
}
