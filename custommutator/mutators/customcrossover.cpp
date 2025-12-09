#include "customcrossover.h"

std::vector<uint8_t> insert_part(std::span<const uint8_t> in1,
                                 std::span<const uint8_t> in2,
                                 size_t max_out_size) {
  size_t in1_size = in1.size();
  size_t in2_size = in2.size();
  std::vector<uint8_t> out;

  if (in1_size >= max_out_size)
    return out;
  if (in1_size == 0 || in2_size == 0)
    return out;

  size_t max_insert_size = max_out_size - in1_size;
  if (max_insert_size > in2_size)
    max_insert_size = in2_size;
  size_t insert_begin = std::rand() % in1_size;
  size_t insert_size = (std::rand() % max_insert_size) + 1;

  size_t in2_begin = std::rand() % (in2_size - insert_size + 1);

  size_t total_size = in1_size + insert_size;
  out.reserve(total_size);

  out.insert(out.end(), in1.begin(), in1.begin() + insert_begin);
  out.insert(out.end(), in2.begin() + in2_begin,
             in2.begin() + in2_begin + insert_size);
  out.insert(out.end(), in1.begin() + insert_begin, in1.end());

  return out;
}

std::vector<uint8_t> overwrite_part(std::span<const uint8_t> in1,
                                    std::span<const uint8_t> in2,
                                    size_t max_out_size) {
  std::vector<uint8_t> out;
  out.reserve(max_out_size);

  if (in1.empty() || in2.empty()) {
    return out;
  }

  // Copy in1 to out first (limited by max_out_size)
  size_t in1_bytes = std::min(in1.size(), max_out_size);
  out.insert(out.end(), in1.begin(), in1.begin() + in1_bytes);

  if (out.empty())
    return out;

  size_t pos = std::rand() % out.size();
  size_t max_possible = std::min(in2.size(), out.size() - pos);
  size_t overwrite_length = (std::rand() % max_possible) + 1;

  size_t in2_start = 0;
  if (in2.size() > overwrite_length) {
    in2_start = std::rand() % (in2.size() - overwrite_length + 1);
  }

  // Overwrite portion of out with random substring from in2
  std::copy_n(in2.begin() + in2_start, overwrite_length, out.begin() + pos);

  return out;
}

std::vector<uint8_t> cross_over(std::span<const uint8_t> in1,
                                std::span<const uint8_t> in2,
                                size_t max_out_size, unsigned seed) {
  std::srand(seed);
  if (std::rand() % 2) {
    return insert_part(in1, in2, max_out_size);
  } else {
    return overwrite_part(in1, in2, max_out_size);
  }
}