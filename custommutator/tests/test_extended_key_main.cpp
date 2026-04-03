#include "../mutators/extended_key.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Stub for LLVMFuzzerMutate so the unit test can link without libFuzzer.
extern "C" size_t LLVMFuzzerMutate(uint8_t *data, size_t size,
                                   size_t max_size) {
  return size > max_size ? max_size : size;
}

namespace {

constexpr size_t kFullOutputMaxSize = 256;

constexpr std::array<size_t, 9> kInputSizes = {0, 1, 2, 3, 7, 16, 32, 78, 128};
constexpr std::array<size_t, 6> kMaxSizes = {0, 1, 8, 16, 64, kFullOutputMaxSize};
constexpr std::array<unsigned int, 8> kSeeds = {0u, 1u, 7u, 13u, 42u, 99u,
                                                 123u, 2026u};

static void fill_deterministic_bytes(uint8_t *data, size_t size,
                                     unsigned int seed) {
  for (size_t index = 0; index < size; ++index) {
    data[index] = static_cast<uint8_t>((seed + (index * 37u) + 29u) & 0xFFu);
  }
}

static bool run_mutator_case(size_t input_size, size_t max_size,
                             unsigned int seed, bool expect_decodable) {
  const size_t buffer_size = std::max<size_t>({size_t{1}, input_size, max_size});
  std::vector<uint8_t> input(buffer_size, 0);
  if (input_size > 0) {
    fill_deterministic_bytes(input.data(), input_size, seed);
  }

  const size_t output_size =
      LLVMFuzzerCustomMutator(input.data(), input_size, max_size, seed);

  if (output_size > max_size) {
    std::printf("  [FAIL] output_size=%zu exceeded max_size=%zu\n", output_size,
                max_size);
    return false;
  }

  if (!expect_decodable) {
    return true;
  }

  const std::string encoded(reinterpret_cast<const char *>(input.data()),
                            output_size);
  std::vector<unsigned char> decoded;
  if (!DecodeBase58Check(encoded, decoded, BIP32_PAYLOAD_LEN)) {
    std::printf("  [FAIL] output did not base58check-decode cleanly\n");
    return false;
  }

  if (decoded.size() != BIP32_PAYLOAD_LEN) {
    std::printf("  [FAIL] decoded payload had %zu bytes, expected %zu\n",
                decoded.size(), BIP32_PAYLOAD_LEN);
    return false;
  }

  return true;
}

static bool test_output_never_exceeds_max_size() {
  std::printf("[TEST] Output stays within max_size...\n");

  for (unsigned int seed : kSeeds) {
    for (size_t input_size : kInputSizes) {
      for (size_t max_size : kMaxSizes) {
        if (!run_mutator_case(input_size, max_size, seed, false)) {
          std::printf("  [FAIL] seed=%u input_size=%zu max_size=%zu\n", seed,
                      input_size, max_size);
          return false;
        }
      }
    }
  }

  std::printf("  [PASS] All outputs respected their max_size limits\n");
  return true;
}

static bool test_output_round_trips_through_base58check() {
  std::printf("[TEST] Output decodes to a 78-byte payload...\n");

  for (unsigned int seed : kSeeds) {
    for (size_t input_size : kInputSizes) {
      if (!run_mutator_case(input_size, kFullOutputMaxSize, seed, true)) {
        std::printf("  [FAIL] seed=%u input_size=%zu\n", seed, input_size);
        return false;
      }
    }
  }

  std::printf("  [PASS] All outputs decoded to exactly 78 bytes\n");
  return true;
}

static bool test_zero_byte_input_is_safe() {
  std::printf("[TEST] Zero-byte input is safe...\n");

  if (!run_mutator_case(0, kFullOutputMaxSize, 2026u, true)) {
    std::printf("  [FAIL] zero-byte input triggered a failure\n");
    return false;
  }

  std::printf("  [PASS] Zero-byte input completed without crashing\n");
  return true;
}

} // namespace

int main() {
  std::printf("Extended Key Custom Mutator Test\n\n");

  int passed = 0;
  int total = 0;

  ++total;
  if (test_output_never_exceeds_max_size()) {
    ++passed;
  }

  ++total;
  if (test_output_round_trips_through_base58check()) {
    ++passed;
  }

  ++total;
  if (test_zero_byte_input_is_safe()) {
    ++passed;
  }

  std::printf("\nTest Results: %d/%d tests passed\n", passed, total);
  return passed == total ? 0 : 1;
}
