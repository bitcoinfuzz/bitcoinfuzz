#ifndef BITCOINFUZZ_CUSTOMMUTATOR_SHACHAIN_H
#define BITCOINFUZZ_CUSTOMMUTATOR_SHACHAIN_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <custommutator/utils/crypto/sha256.h>
#include <random>
#include <vector>

// Structure-aware mutator for the shachain target (input format: see
// BaseModule::shachain). It parses the input into ops, applies op-level
// mutations and serializes them back. Random secrets fail every store's check
// from the second insert on, so inserts stay a consistent BOLT3 chain except
// where a mutation corrupts one on purpose. Some calls use libFuzzer's byte
// mutations instead, unchanged.

namespace shachain_mutator {
constexpr uint8_t INSERT = 0x00;
constexpr uint8_t LOOKUP = 0x01;
constexpr uint8_t RESTORE = 0x02;
constexpr uint8_t GENERATE = 0x03;
constexpr size_t SECRET_SIZE = 32;
constexpr size_t INDEX_SIZE = 6;
constexpr uint64_t MAX_INDEX = (1ULL << 48) - 1;

using Secret = std::array<uint8_t, SECRET_SIZE>;

struct Op {
  uint8_t tag;
  Secret secret{};   // an insert's secret or a generate's seed
  uint64_t index{0}; // a lookup's or a generate's index
};

// Whether the secret at index `to` can be derived from the one at `from`.
inline bool Derivable(uint64_t from, uint64_t to) {
  const uint64_t prefix_mask = from == 0 ? 0 : ~((from & -from) - 1);
  return (to & prefix_mask) == from;
}

// BOLT3 derive_secret from the secret at `from` to the one at `to`; from index
// 0 (the seed) it is generate_from_seed.
inline Secret Derive(Secret secret, uint64_t from, uint64_t to) {
  for (int b = 47; b >= 0; --b) {
    if (((from >> b) & 1) == 0 && ((to >> b) & 1) == 1) {
      secret[b / 8] ^= 1 << (b % 8);
      CSHA256().Write(secret.data(), secret.size()).Finalize(secret.data());
    }
  }
  return secret;
}

inline size_t OpSize(uint8_t tag) {
  switch (tag) {
  case INSERT:
    return 1 + SECRET_SIZE;
  case LOOKUP:
    return 1 + INDEX_SIZE;
  case RESTORE:
    return 1;
  default:
    return 1 + SECRET_SIZE + INDEX_SIZE;
  }
}

// Reads whole ops as the driver does, stopping at an unknown tag or a
// truncated payload.
inline std::vector<Op> Parse(const uint8_t *data, size_t size) {
  std::vector<Op> ops;
  for (size_t pos = 0; pos < size && data[pos] <= GENERATE;) {
    Op op{data[pos]};
    if (size - pos < OpSize(op.tag))
      break;
    const uint8_t *p = data + pos + 1;
    if (op.tag == INSERT || op.tag == GENERATE) {
      std::memcpy(op.secret.data(), p, SECRET_SIZE);
      p += SECRET_SIZE;
    }
    if (op.tag == LOOKUP || op.tag == GENERATE)
      for (size_t i = 0; i < INDEX_SIZE; ++i)
        op.index = (op.index << 8) | p[i];
    ops.push_back(op);
    pos += OpSize(op.tag);
  }
  return ops;
}

// Writes as many whole ops as fit in max_size and returns the size written.
inline size_t Serialize(const std::vector<Op> &ops, uint8_t *data,
                        size_t max_size) {
  size_t size = 0;
  for (const Op &op : ops) {
    if (size + OpSize(op.tag) > max_size)
      break;
    data[size++] = op.tag;
    if (op.tag == INSERT || op.tag == GENERATE) {
      std::memcpy(data + size, op.secret.data(), SECRET_SIZE);
      size += SECRET_SIZE;
    }
    if (op.tag == LOOKUP || op.tag == GENERATE)
      for (size_t i = 0; i < INDEX_SIZE; ++i)
        data[size++] =
            static_cast<uint8_t>(op.index >> (8 * (INDEX_SIZE - 1 - i)));
  }
  return size;
}

inline Secret RandomSecret(std::mt19937_64 &rng) {
  Secret s;
  for (uint8_t &b : s)
    b = static_cast<uint8_t>(rng());
  return s;
}

// The number of inserts before `pos`. The insert at `pos` gets index
// MAX_INDEX minus that if every insert before it is accepted.
inline uint64_t InsertsBefore(const std::vector<Op> &ops, size_t pos) {
  uint64_t n = 0;
  for (size_t i = 0; i < pos; ++i)
    n += ops[i].tag == INSERT;
  return n;
}

// Gives the insert at `pos` a fresh secret and re-derives from it the earlier
// inserts it can derive. Nothing else received earlier depends on them, so the
// chain stays consistent until a later insert that derives this one, which
// then gets rejected.
inline void Reroot(std::vector<Op> &ops, size_t pos, std::mt19937_64 &rng) {
  const uint64_t root = MAX_INDEX - InsertsBefore(ops, pos);
  ops[pos].secret = RandomSecret(rng);
  uint64_t index = MAX_INDEX;
  for (size_t i = 0; i < pos; ++i) {
    if (ops[i].tag != INSERT)
      continue;
    if (Derivable(root, index))
      ops[i].secret = Derive(ops[pos].secret, root, index);
    --index;
  }
}

// A random insert's position; ops must hold one.
inline size_t RandomInsert(const std::vector<Op> &ops, std::mt19937_64 &rng) {
  uint64_t target = rng() % InsertsBefore(ops, ops.size());
  for (size_t i = 0;; ++i)
    if (ops[i].tag == INSERT && target-- == 0)
      return i;
}

enum Mutation {
  APPEND_INSERTS,
  REROOT_INSERT,
  CORRUPT_INSERT,
  ADD_LOOKUP,
  ADD_RESTORE,
  ADD_GENERATE,
  DELETE_OP,
  MUTATION_COUNT,
};

inline void Mutate(std::vector<Op> &ops, Mutation mutation,
                   std::mt19937_64 &rng) {
  const bool has_insert = InsertsBefore(ops, ops.size()) > 0;
  if (((mutation == REROOT_INSERT || mutation == CORRUPT_INSERT) &&
       !has_insert) ||
      (mutation == DELETE_OP && ops.empty()))
    mutation = APPEND_INSERTS;

  const size_t pos = rng() % (ops.size() + 1);
  switch (mutation) {
  case APPEND_INSERTS:
    for (uint64_t n = 1 + rng() % 8; n > 0; --n) {
      ops.push_back({INSERT});
      Reroot(ops, ops.size() - 1, rng);
    }
    break;
  case REROOT_INSERT:
    Reroot(ops, RandomInsert(ops, rng), rng);
    break;
  case CORRUPT_INSERT: {
    const size_t insert = RandomInsert(ops, rng);
    const uint8_t bit = rng() % (8 * SECRET_SIZE);
    ops[insert].secret[bit / 8] ^= 1 << (bit % 8);
    break;
  }
  case ADD_LOOKUP: {
    // Mostly a received index, the next one or the one after it.
    const uint64_t received = InsertsBefore(ops, pos);
    const uint64_t index =
        rng() % 4 != 0 ? MAX_INDEX - rng() % (received + 2) : rng() & MAX_INDEX;
    ops.insert(ops.begin() + pos, Op{LOOKUP, {}, index});
    break;
  }
  case ADD_RESTORE:
    ops.insert(ops.begin() + pos, Op{RESTORE});
    break;
  case ADD_GENERATE:
    ops.insert(ops.begin() + pos,
               Op{GENERATE, RandomSecret(rng), rng() & MAX_INDEX});
    break;
  case DELETE_OP:
    ops.erase(ops.begin() + rng() % ops.size());
    break;
  case MUTATION_COUNT:
    break;
  }
}
} // namespace shachain_mutator

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t max_size, unsigned int seed) {
  using namespace shachain_mutator;
  std::mt19937_64 rng(seed);
  // Byte-level changes the op mutations never make, such as shifted op
  // boundaries and secrets or indexes taken from the libraries' comparisons.
  if (rng() % 4 == 0)
    return LLVMFuzzerMutate(data, size, max_size);
  std::vector<Op> ops = Parse(data, size);
  for (uint64_t n = 1 + rng() % 4; n > 0; --n)
    Mutate(ops, static_cast<Mutation>(rng() % MUTATION_COUNT), rng);
  // lightning-kmp skips any input with a restore, and restores stay in an
  // input's descendants once added, so half the outputs leave them out.
  if (rng() % 2 == 0)
    std::erase_if(ops, [](const Op &op) { return op.tag == RESTORE; });
  return Serialize(ops, data, max_size);
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_SHACHAIN_H
