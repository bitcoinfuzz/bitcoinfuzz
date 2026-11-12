#ifndef BITCOINFUZZ_CUSTOMMUTATOR_INVARIANTS_SPEC_LOCK_H
#define BITCOINFUZZ_CUSTOMMUTATOR_INVARIANTS_SPEC_LOCK_H

// Dependency-free C++ port of the SpecLock Lean 4 theorems in
// blvm-spec/lean/SpecLock/*.lean. Every checker below is a direct transcription
// of a declaration carrying @[spec_lock "<lock id>"]; the lock id is repeated on
// each function so a fuzz failure can be traced back to the proved statement.
//
// Lean models values as Nat; here they are uint64_t, so `outputs_nonnegative`
// holds by construction and the CheckTransaction port checks the upper bound.

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace spec_lock {

// --- Constants (SpecLock/Subsidy.lean, SpecLock/Tx.lean) ---------------------

inline constexpr uint64_t H = 210000;                    // PROTOCOL.md#const:H
inline constexpr uint64_t C = 100000000;                 // PROTOCOL.md#const:C
inline constexpr uint64_t INITIAL_SUBSIDY = 50 * C;      // PROTOCOL.md#const:initialSubsidy
inline constexpr uint64_t MMAX = 21000000 * C;           // SpecLock.Tx Mmax
inline constexpr uint64_t MAX_MONEY = 21 * 1000000 * C;  // SpecLock.Supply maxMoney
inline constexpr uint64_t EPOCH_ZERO_PAST = 33 * H;      // Theorem:6.1.2 boundary
inline constexpr uint64_t EPOCH_CLAMP_PAST = 64 * H;     // def:GetBlockSubsidy guard
inline constexpr uint64_t EXACT_SUPPLY_33H = 2099999997690000ULL;  // Theorem:6.2.3
inline constexpr uint32_t LOCKTIME_THRESHOLD = 500000000;          // def:LocktimeType

enum class Verdict { valid, invalid };  // SpecLock.Tx Verdict

// --- Definitions -------------------------------------------------------------

// @[spec_lock "PROTOCOL.md#def:CheckTransaction"]
inline Verdict checkTransaction(uint64_t inputCount, uint64_t outputCount,
                                std::span<const uint64_t> outputValues) {
  if (inputCount == 0 || outputCount == 0) return Verdict::invalid;
  for (uint64_t v : outputValues)
    if (MMAX < v) return Verdict::invalid;  // Nat >= 0 is implicit in uint64_t
  return Verdict::valid;
}

// @[spec_lock "PROTOCOL.md#def:LocktimeType"] — true means block-height type.
inline bool locktimeType(uint64_t lt) { return lt < LOCKTIME_THRESHOLD; }

// @[spec_lock "PROTOCOL.md#def:GetBlockSubsidy"]
inline uint64_t getBlockSubsidy(uint64_t h) {
  uint64_t k = h / H;
  if (k >= 64) return 0;
  return INITIAL_SUBSIDY / (uint64_t{1} << k);
}

// @[spec_lock "PROTOCOL.md#def:ClampTime"]
inline uint64_t clampTime(uint64_t raw, uint64_t expected) {
  uint64_t lo = expected / 4;
  uint64_t hi = expected * 4;
  uint64_t clamped = raw < hi ? raw : hi;
  return clamped < lo ? lo : clamped;
}

// @[spec_lock "PROTOCOL.md#def:TotalSupply"] — closed form of the Lean
// sumBelow/epochSum decomposition (sumBelow_epochs in SpecLock/Supply.lean):
// sum over full epochs 0..k-1 of H * (initial / 2^e), plus the (r+1) blocks
// already mined in epoch k. Epochs >= 33 contribute nothing (Theorem:6.1.2).
inline uint64_t totalSupply(uint64_t h) {
  uint64_t k = h / H;
  uint64_t r = h % H;
  if (k > 33) k = 33;  // subsidy is 0 from epoch 33 on; constant tail
  uint64_t sum = 0;
  for (uint64_t e = 0; e < k; ++e) sum += H * (INITIAL_SUBSIDY / (uint64_t{1} << e));
  sum += (r + 1) * (INITIAL_SUBSIDY / (uint64_t{1} << k));
  return sum;
}

// --- Theorem checkers (mirrors of the @[spec_lock] theorems) -----------------

// @[spec_lock "PROTOCOL.md#CheckTransaction#empty-inputs"]
inline bool reject_empty_inputs(uint64_t inputCount, uint64_t outputCount,
                                std::span<const uint64_t> values) {
  return inputCount != 0 ||
         checkTransaction(inputCount, outputCount, values) != Verdict::valid;
}

// @[spec_lock "PROTOCOL.md#CheckTransaction#empty-outputs"]
inline bool reject_empty_outputs(uint64_t inputCount, uint64_t outputCount,
                                 std::span<const uint64_t> values) {
  return outputCount != 0 ||
         checkTransaction(inputCount, outputCount, values) != Verdict::valid;
}

// @[spec_lock "PROTOCOL.md#Theorem:5.4.7.2"] (LocktimeType determination)
inline bool locktime_type_determination(uint64_t lt) {
  return (lt < LOCKTIME_THRESHOLD) == locktimeType(lt);
}

// @[spec_lock "PROTOCOL.md#Theorem:6.1.2"]
inline bool subsidy_zero_from_epoch_33(uint64_t h) {
  return EPOCH_ZERO_PAST <= h ? getBlockSubsidy(h) == 0 : true;
}

// @[spec_lock "PROTOCOL.md#Theorem:6.1.1"]
inline bool subsidy_epoch_edge(uint64_t k) {
  if (k >= 32) return true;
  return getBlockSubsidy(k * H + H) == getBlockSubsidy(k * H) / 2;
}

// @[spec_lock "PROTOCOL.md#Theorem:6.2.1"] (totalSupply_mono)
inline bool total_supply_mono(uint64_t h1, uint64_t h2) {
  return h1 > h2 || totalSupply(h1) <= totalSupply(h2);
}

// @[spec_lock "PROTOCOL.md#Theorem:6.2.2"]
inline bool total_supply_le_max(uint64_t h) { return totalSupply(h) <= MAX_MONEY; }

// @[spec_lock "PROTOCOL.md#Theorem:6.2.3"]
inline bool total_supply_exact_33h() {
  return totalSupply(EPOCH_ZERO_PAST) == EXACT_SUPPLY_33H;
}

// Constant tail: totalSupply h = totalSupply (33*H) once h >= 33*H
// (SpecLock/Supply.lean supply_constant).
inline bool supply_constant(uint64_t h) {
  return EPOCH_ZERO_PAST > h || totalSupply(h) == EXACT_SUPPLY_33H;
}

// subsidy_antitone (SpecLock/Subsidy.lean).
inline bool subsidy_antitone(uint64_t h1, uint64_t h2) {
  return h1 > h2 || getBlockSubsidy(h2) <= getBlockSubsidy(h1);
}

// @[spec_lock "PROTOCOL.md#Theorem:7.1"] (difficulty factor-of-four clamp)
inline bool difficulty_factor_of_four(uint64_t raw, uint64_t expected) {
  uint64_t c = clampTime(raw, expected);
  return expected / 4 <= c && c <= expected * 4;
}

// --- Boundary tables (the case splits the Lean proofs perform) ---------------
// A mutator seeded from these values exercises every arm of every definition
// above: each table entry sits on a branch boundary of the proved case split.

namespace boundaries {

inline constexpr uint64_t HEIGHTS[] = {
    0,
    1,
    H - 1,
    H,          // first block of epoch 1: subsidy halves
    H + 1,
    31 * H,
    32 * H - 1,
    32 * H,     // last halving covered by Theorem:6.1.1 (k < 32)
    32 * H + 1,
    33 * H - 1, // last block paying 1 sat
    33 * H,     // Theorem:6.1.2 / 6.2.3 boundary
    33 * H + 1,
    64 * H - 1, // def:GetBlockSubsidy guard arm
    64 * H,
    UINT64_MAX};

inline constexpr uint64_t OUTPUT_VALUES[] = {
    0,          // outputs_nonnegative boundary
    1,
    MMAX - 1,
    MMAX,       // largest value CheckTransaction accepts
    MMAX + 1,   // smallest value it must reject
    UINT64_MAX};

inline constexpr uint64_t LOCKTIMES[] = {0, 1, 499999999, 500000000, 500000001,
                                         UINT32_MAX};

// (raw, expected) pairs covering clampTime's min/max arms and Theorem:7.1.
inline constexpr std::pair<uint64_t, uint64_t> CLAMP_PAIRS[] = {
    {0, 1}, {1, 1}, {3, 12}, {4, 12}, {48, 12}, {49, 12}, {100, 100},
    {0, 100}, {24, 96}, {25, 100}, {399, 100}, {400, 100}, {401, 100},
    {UINT64_MAX / 8, UINT64_MAX / 32},
};

}  // namespace boundaries

// --- Wire format (shared by mutator and the differential fuzz target) --------

namespace wire {

enum Tag : uint8_t {
  TX_CHECK = 0x01,  // u8 inputCount, u8 outputCount, outputCount * u64 values
  LOCKTIME = 0x02,  // u32 locktime
  SUBSIDY = 0x03,   // u64 height
  SUPPLY = 0x04,    // u64 height
  CLAMP = 0x05,     // u64 raw, u64 expected
};

inline void put_u8(uint8_t *&p, uint8_t v) { *p++ = v; }
inline void put_u32(uint8_t *&p, uint32_t v) {
  for (int i = 0; i < 4; ++i) *p++ = static_cast<uint8_t>(v >> (8 * i));
}
inline void put_u64(uint8_t *&p, uint64_t v) {
  for (int i = 0; i < 8; ++i) *p++ = static_cast<uint8_t>(v >> (8 * i));
}
inline bool get_u8(std::span<const uint8_t> b, size_t &off, uint8_t &v) {
  if (off + 1 > b.size()) return false;
  v = b[off++];
  return true;
}
inline bool get_u32(std::span<const uint8_t> b, size_t &off, uint32_t &v) {
  if (off + 4 > b.size()) return false;
  v = 0;
  for (int i = 0; i < 4; ++i) v |= uint32_t(b[off + i]) << (8 * i);
  off += 4;
  return true;
}
inline bool get_u64(std::span<const uint8_t> b, size_t &off, uint64_t &v) {
  if (off + 8 > b.size()) return false;
  v = 0;
  for (int i = 0; i < 8; ++i) v |= uint64_t(b[off + i]) << (8 * i);
  off += 8;
  return true;
}

}  // namespace wire

}  // namespace spec_lock

#endif  // BITCOINFUZZ_CUSTOMMUTATOR_INVARIANTS_SPEC_LOCK_H
