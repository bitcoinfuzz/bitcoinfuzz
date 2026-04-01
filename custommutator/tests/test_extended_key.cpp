#include "../mutators/extended_key.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize) {
  return Size;
}


static bool test_max_size_property() {
  printf("[TEST] Output always <= max_size...\n");
  const size_t max_size = 200;
  uint8_t fuzz_data[max_size];
  const char *xpub = "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqeseD2tcSTx7yQrESc7hYn56L59EA";
  size_t input_size = strlen(xpub);
  memcpy(fuzz_data, xpub, input_size);

  size_t mutated_size = LLVMFuzzerCustomMutator(fuzz_data, input_size, max_size, 42);

  if (mutated_size > max_size) {
    printf("  [FAIL] mutated_size (%zu) > max_size (%zu)\n", mutated_size, max_size);
    return false;
  }

  printf("  [PASS] Output within bounds\n");
  return true;
}

static bool test_decode_property() {
  printf("[TEST] Output decodes to 78 bytes...\n");
  const size_t max_size = 200;
  uint8_t fuzz_data[max_size];
  memset(fuzz_data, 0x41, 50);

  size_t mutated_size = LLVMFuzzerCustomMutator(fuzz_data, 50, max_size, 43);

  std::string result(reinterpret_cast<char *>(fuzz_data), mutated_size);
  std::vector<unsigned char> decoded;
  if (!DecodeBase58Check(result, decoded, 100)) {
    printf("  [FAIL] Failed to decode mutated output as Base58Check\n");
    return false;
  }

  if (decoded.size() != 78) {
    printf("  [FAIL] Decoded size (%zu) != 78\n", decoded.size());
    return false;
  }

  printf("  [PASS] Output correctly decodes to BIP32 payload size\n");
  return true;
}

static bool test_zero_byte_input() {
  printf("[TEST] No crash on 0-byte input...\n");
  const size_t max_size = 200;
  uint8_t fuzz_data[max_size];

  size_t mutated_size = LLVMFuzzerCustomMutator(fuzz_data, 0, max_size, 44);

  if (mutated_size == 0) {
    printf("  [FAIL] Mutator returned 0 bytes for valid max_size\n");
    return false;
  }

  printf("  [PASS] Successfully handled empty input\n");
  return true;
}

int main() {
  printf("Extended Key Custom Mutator Test\n\n");

  srand(42);

  int passed = 0;
  int total = 0;

  total++;
  if (test_max_size_property())
    passed++;
  total++;
  if (test_decode_property())
    passed++;
  total++;
  if (test_zero_byte_input())
    passed++;

  printf("\nTest Results: %d/%d tests passed\n", passed, total);

  return (passed == total) ? 0 : 1;
}
