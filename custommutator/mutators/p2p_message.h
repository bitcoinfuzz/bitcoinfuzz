#ifndef BITCOINFUZZ_CUSTOMMUTATOR_P2P_MESSAGE_H
#define BITCOINFUZZ_CUSTOMMUTATOR_P2P_MESSAGE_H

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <custommutator/utils/crypto/sha256.h>

// Custom mutator for Bitcoin P2P messages that maintains valid checksums.
// This enables efficient fuzzing of Bitcoin message parsing logic without
// the fuzzer getting stuck on checksum validation failures.
//
// This custom mutator does the following:
//   1. Mutate the input using libFuzzer's default mutator `LLVMFuzzerMutate`
//   2. If the data is large enough to contain a header, recalculate and fix the
//   checksum
//   3. Return the mutated data with valid checksum

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

// Bitcoin message constants
static constexpr size_t BITCOIN_HEADER_SIZE = 24;
static constexpr size_t PAYLOAD_LENGTH_OFFSET = 16;
static constexpr size_t CHECKSUM_OFFSET = 20;

// Bitcoin double SHA256 using Bitcoin Core's CSHA256 class
static void bitcoin_double_sha256(const uint8_t *data, size_t len,
                                  uint8_t *hash) {
  CSHA256 hasher;
  hasher.Write(data, len);
  unsigned char first_hash[CSHA256::OUTPUT_SIZE];
  hasher.Finalize(first_hash);

  hasher.Reset();
  hasher.Write(first_hash, CSHA256::OUTPUT_SIZE);
  hasher.Finalize(hash);
}

// Fix the checksum in a Bitcoin P2P message
static void fix_bitcoin_checksum(uint8_t *data, size_t size) {
  if (size < BITCOIN_HEADER_SIZE) {
    return;
  }

  uint32_t payload_length;
  memcpy(&payload_length, data + PAYLOAD_LENGTH_OFFSET, 4);

  // Validate that we have the complete message
  if (size < BITCOIN_HEADER_SIZE + payload_length) {
    return;
  }

  // Calculate checksum for the payload
  const uint8_t *payload = data + BITCOIN_HEADER_SIZE;
  uint8_t calculated_checksum[32];
  bitcoin_double_sha256(payload, payload_length, calculated_checksum);

  // Update checksum in header (first 4 bytes of hash)
  memcpy(data + CHECKSUM_OFFSET, calculated_checksum, 4);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  // First, mutate the data using LibFuzzer's default mutator
  size_t new_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

  // Then fix the checksum if the data looks like it could be a Bitcoin message
  fix_bitcoin_checksum(fuzz_data, new_size);

  return new_size;
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_P2P_MESSAGE_H
