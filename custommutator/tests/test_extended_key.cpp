#include "../mutators/extended_key.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize) {
  return Size;
}

static constexpr size_t MAX_SIZE = 200;

static bool test_output_respects_max_size() {
  printf("[TEST] Output always <= max_size...\n");

  std::string_view xpub =
      "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJ"
      "oCu1Rupje8YtGqeseD2tcSTx7yQrESc7hYn56L59EA";

  std::array<uint8_t, MAX_SIZE> buffer{};
  std::copy(xpub.begin(), xpub.end(), buffer.begin());

  size_t result =
      LLVMFuzzerCustomMutator(buffer.data(), xpub.size(), MAX_SIZE, 42);

  if (result > MAX_SIZE) {
    printf("  [FAIL] returned %zu which exceeds max_size %zu\n", result,
           MAX_SIZE);
    return false;
  }

  printf("  [PASS] Output within bounds (%zu bytes)\n", result);
  return true;
}

static bool test_output_decodes_to_78_bytes() {
  printf("[TEST] Output base58check-decodes to exactly 78 bytes...\n");

  std::array<uint8_t, MAX_SIZE> buffer{};
  buffer.fill(0x41);

  size_t result = LLVMFuzzerCustomMutator(buffer.data(), 50, MAX_SIZE, 43);

  std::string encoded(reinterpret_cast<char *>(buffer.data()), result);
  std::vector<unsigned char> decoded;

  if (!DecodeBase58Check(encoded, decoded, static_cast<int>(MAX_SIZE))) {
    printf("  [FAIL] Output does not decode as valid Base58Check\n");
    return false;
  }

  if (decoded.size() != BIP32_PAYLOAD_LEN) {
    printf("  [FAIL] Decoded to %zu bytes, expected %zu\n", decoded.size(),
           BIP32_PAYLOAD_LEN);
    return false;
  }

  printf("  [PASS] Decodes to exactly %zu bytes\n", BIP32_PAYLOAD_LEN);
  return true;
}

static bool test_empty_input_does_not_crash() {
  printf("[TEST] Empty input does not crash...\n");

  std::array<uint8_t, MAX_SIZE> buffer{};

  size_t result = LLVMFuzzerCustomMutator(buffer.data(), 0, MAX_SIZE, 44);

  if (result == 0) {
    printf("  [FAIL] Returned 0 bytes on empty input with valid max_size\n");
    return false;
  }

  printf("  [PASS] Survived empty input, returned %zu bytes\n", result);
  return true;
}

int main() {
  printf("Extended Key Custom Mutator Test\n\n");

  int passed = 0;
  int total = 0;

  total++;
  if (test_output_respects_max_size())
    passed++;
  total++;
  if (test_output_decodes_to_78_bytes())
    passed++;
  total++;
  if (test_empty_input_does_not_crash())
    passed++;

  printf("\nResults: %d/%d passed\n", passed, total);

  return (passed == total) ? 0 : 1;
}
