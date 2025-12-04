#ifndef CROSS_OVER_H
#define CROSS_OVER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Inserts a random slice of in2 into in1, respecting max_out_size.
std::vector<uint8_t> insert_part(std::span<const uint8_t> in1,
                                 std::span<const uint8_t> in2,
                                 size_t max_out_size);

// Overwrites a random part of in1 with a slice from in2, up to max_out_size.
std::vector<uint8_t> overwrite_part(std::span<const uint8_t> in1,
                                    std::span<const uint8_t> in2,
                                    size_t max_out_size);

// Randomly selects insert_part or overwrite_part using the provided seed.
std::vector<uint8_t> cross_over(std::span<const uint8_t> in1,
                                std::span<const uint8_t> in2,
                                size_t max_out_size, unsigned seed);

#endif // CROSS_OVER_H
