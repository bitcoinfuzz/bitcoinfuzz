#ifndef BITCOINFUZZ_CUSTOMMUTATOR_SPEC_LOCK_H
#define BITCOINFUZZ_CUSTOMMUTATOR_SPEC_LOCK_H

// Structured mutator for the SpecLock consensus invariants proved in
// blvm-spec/lean/SpecLock/*.lean. Instead of blind byte flips, every field is
// seeded from the boundary tables the Lean proofs case-split on, so the fuzzer
// systematically visits each arm of each proved definition.
//
// Buffer layout (all integers little-endian):
//   tag 0x01 TX_CHECK : u8 inputCount, u8 outputCount, outputCount * u64 value
//   tag 0x02 LOCKTIME : u32 locktime
//   tag 0x03 SUBSIDY  : u64 height
//   tag 0x04 SUPPLY   : u64 height
//   tag 0x05 CLAMP    : u64 raw, u64 expected
// Unknown tags fall back to plain LLVMFuzzerMutate.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "../invariants/spec_lock.h"

static constexpr size_t SPEC_LOCK_MAX_PAYLOAD =
    1 + 1 + 8 * 8;  // TX_CHECK with 8 outputs is the largest layout

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  if (max_size < SPEC_LOCK_MAX_PAYLOAD) {
    return LLVMFuzzerMutate(fuzz_data, size, max_size);
  }

  namespace B = spec_lock::boundaries;
  namespace W = spec_lock::wire;

  uint8_t buf[SPEC_LOCK_MAX_PAYLOAD];
  uint8_t *p = buf;

  // Pick the invariant family; the seed deterministically walks the table.
  W::Tag tag = static_cast<W::Tag>(seed % 5 + 1);
  switch (tag) {
  case W::TX_CHECK: {
    W::put_u8(p, W::TX_CHECK);
    W::put_u8(p, static_cast<uint8_t>(seed % 3));       // inputCount: 0,1,2
    uint8_t out_count = static_cast<uint8_t>((seed / 3) % 3);  // 0,1,2
    W::put_u8(p, out_count);
    for (uint8_t i = 0; i < out_count; ++i) {
      uint64_t v = B::OUTPUT_VALUES[(seed / 9 + i) %
                                    (sizeof(B::OUTPUT_VALUES) /
                                     sizeof(B::OUTPUT_VALUES[0]))];
      W::put_u64(p, v);
    }
    break;
  }
  case W::LOCKTIME: {
    W::put_u8(p, W::LOCKTIME);
    uint32_t lt = static_cast<uint32_t>(
        B::LOCKTIMES[seed % (sizeof(B::LOCKTIMES) / sizeof(B::LOCKTIMES[0]))]);
    W::put_u32(p, lt);
    break;
  }
  case W::SUBSIDY:
    W::put_u8(p, W::SUBSIDY);
    W::put_u64(p, B::HEIGHTS[seed % (sizeof(B::HEIGHTS) / sizeof(B::HEIGHTS[0]))]);
    break;
  case W::SUPPLY:
    W::put_u8(p, W::SUPPLY);
    W::put_u64(p, B::HEIGHTS[seed % (sizeof(B::HEIGHTS) / sizeof(B::HEIGHTS[0]))]);
    break;
  case W::CLAMP: {
    W::put_u8(p, W::CLAMP);
    auto pair = B::CLAMP_PAIRS[seed % (sizeof(B::CLAMP_PAIRS) /
                                       sizeof(B::CLAMP_PAIRS[0]))];
    W::put_u64(p, pair.first);
    W::put_u64(p, pair.second);
    break;
  }
  default:
    return LLVMFuzzerMutate(fuzz_data, size, max_size);
  }

  size_t payload_len = static_cast<size_t>(p - buf);
  memcpy(fuzz_data, buf, payload_len);
  // Any remaining budget is left to the default mutator so unstructured
  // exploration continues around the structured seed.
  if (payload_len < max_size) {
    size_t extra = LLVMFuzzerMutate(fuzz_data + payload_len, 0,
                                    max_size - payload_len);
    return payload_len + extra;
  }
  return payload_len;
}

#endif  // BITCOINFUZZ_CUSTOMMUTATOR_SPEC_LOCK_H
