// Tests for the SpecLock consensus-invariant mutator and its C++ port of the
// Lean 4 theorems in blvm-spec/lean/SpecLock/*.lean.
//
// Part 1: golden-value assertions — each @[spec_lock] theorem restated against
//         constants computed from the proved statements (cross-checked against
//         the Lean build; see SPEC_LOCK_FUZZ_PLAN.md).
// Part 2: mutator branch coverage — run the mutator over its seed space and
//         require that every arm of every invariant is exercised.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

#include "../invariants/spec_lock.h"
#include "../mutators/spec_lock.h"

// Stub for the default mutator: pseudo-random tail bytes.
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize) {
  (void)Size;
  static uint64_t state = 0x9e3779b97f4a7c15ULL;
  size_t n = MaxSize < 8 ? MaxSize : 8;
  for (size_t i = 0; i < n; ++i) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    Data[i] = static_cast<uint8_t>(state);
  }
  return n;
}

static int failures = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, msg);      \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

using namespace spec_lock;
namespace B = spec_lock::boundaries;
namespace W = spec_lock::wire;

// --- Part 1: golden values from the Lean theorems ----------------------------

static void test_check_transaction() {
  // Theorem def:GetBlockSubsidy constants sanity
  CHECK(H == 210000, "H = 210000");
  CHECK(INITIAL_SUBSIDY == 5000000000ULL, "initialSubsidy = 50e8");

  uint64_t one[] = {1};
  // PROTOCOL.md#CheckTransaction#empty-inputs
  CHECK(reject_empty_inputs(0, 1, one), "empty inputs rejected");
  // PROTOCOL.md#CheckTransaction#empty-outputs
  CHECK(reject_empty_outputs(1, 0, {}), "empty outputs rejected");
  // def:CheckTransaction boundaries
  uint64_t vmax[] = {MMAX};
  uint64_t vmax_p1[] = {MMAX + 1};
  uint64_t zero[] = {0};
  CHECK(checkTransaction(1, 1, vmax) == Verdict::valid, "v == Mmax accepted");
  CHECK(checkTransaction(1, 1, vmax_p1) == Verdict::invalid, "v == Mmax+1 rejected");
  CHECK(checkTransaction(1, 1, zero) == Verdict::valid, "v == 0 accepted");
  uint64_t two_max[] = {MMAX, MMAX};
  CHECK(checkTransaction(2, 2, two_max) == Verdict::valid,
        "multi-in multi-out accepted");
}

static void test_locktime() {
  CHECK(locktimeType(0), "locktime 0 is height");
  CHECK(locktimeType(499999999), "499999999 is height");
  CHECK(!locktimeType(500000000), "500000000 is timestamp");
  CHECK(!locktimeType(500000001), "500000001 is timestamp");
  for (uint64_t lt : B::LOCKTIMES)
    CHECK(locktime_type_determination(lt), "locktime determination theorem");
}

static void test_subsidy() {
  // def:GetBlockSubsidy epoch schedule, first and last epochs
  CHECK(getBlockSubsidy(0) == 5000000000ULL, "epoch 0 subsidy 50 BTC");
  CHECK(getBlockSubsidy(H) == 2500000000ULL, "epoch 1 subsidy 25 BTC");
  CHECK(getBlockSubsidy(2 * H) == 1250000000ULL, "epoch 2 subsidy 12.5 BTC");
  // Theorem:6.1.1 epoch edge (k < 32): subsidy(kH + H) = subsidy(kH) / 2
  for (uint64_t k = 0; k < 33; ++k) CHECK(subsidy_epoch_edge(k), "epoch edge");
  // Theorem:6.1.2: zero from epoch 33
  CHECK(getBlockSubsidy(33 * H - 1) == 1, "last 1-sat block");
  CHECK(getBlockSubsidy(33 * H) == 0, "zero at 33H");
  CHECK(getBlockSubsidy(64 * H) == 0, "zero at 64H");
  for (uint64_t h : B::HEIGHTS) CHECK(subsidy_zero_from_epoch_33(h), "zero past 33H");
  for (size_t i = 1; i < sizeof(B::HEIGHTS) / sizeof(B::HEIGHTS[0]); ++i)
    CHECK(subsidy_antitone(B::HEIGHTS[i - 1], B::HEIGHTS[i]), "subsidy antitone");
}

static void test_supply() {
  // Theorem:6.2.3
  CHECK(total_supply_exact_33h(), "totalSupply(33H) exact");
  CHECK(totalSupply(33 * H) == 2099999997690000ULL, "exact issued supply");
  CHECK(EXACT_SUPPLY_33H < MAX_MONEY, "issued < 21M cap");
  // Theorem:6.2.2
  for (uint64_t h : B::HEIGHTS) CHECK(total_supply_le_max(h), "supply <= max");
  // Theorem:6.2.1 + constant tail (supply_constant)
  for (uint64_t h : B::HEIGHTS) {
    CHECK(supply_constant(h), "supply constant past 33H");
    CHECK(total_supply_mono(0, h), "supply monotone");
  }
  CHECK(totalSupply(33 * H - 1) == EXACT_SUPPLY_33H, "supply(33H-1) == supply(33H)");
  CHECK(totalSupply(64 * H) == EXACT_SUPPLY_33H, "supply(64H) == supply(33H)");
  CHECK(totalSupply(UINT64_MAX / 4) == EXACT_SUPPLY_33H, "supply saturates");
}

static void test_clamp() {
  CHECK(clampTime(0, 100) == 25, "clamp low arm");
  CHECK(clampTime(25, 100) == 25, "clamp at low bound");
  CHECK(clampTime(400, 100) == 400, "clamp at high bound");
  CHECK(clampTime(401, 100) == 400, "clamp high arm");
  for (auto [raw, expected] : B::CLAMP_PAIRS)
    CHECK(difficulty_factor_of_four(raw, expected), "factor-of-four theorem");
}

// --- Part 2: mutator branch coverage ------------------------------------------

// Decode a mutator output and record which theorem arms it can trigger.
struct Coverage {
  bool empty_inputs = false, nonempty_inputs = false;
  bool empty_outputs = false, nonempty_outputs = false;
  bool value_ok = false, value_over = false, value_zero = false;
  bool lt_height = false, lt_timestamp = false;
  bool subsidy_pre33 = false, subsidy_post33 = false;
  bool supply_pre33 = false, supply_post33 = false;
  bool clamp_arm = false;
};

static Coverage decode(const uint8_t *data, size_t size) {
  Coverage c;
  std::span<const uint8_t> b(data, size);
  size_t off = 0;
  uint8_t tag;
  if (!W::get_u8(b, off, tag)) return c;
  switch (static_cast<W::Tag>(tag)) {
  case W::TX_CHECK: {
    uint8_t in_count, out_count;
    if (!W::get_u8(b, off, in_count) || !W::get_u8(b, off, out_count)) return c;
    uint64_t vals[8];
    size_t n = out_count < 8 ? out_count : 8;
    for (size_t i = 0; i < n; ++i)
      if (!W::get_u64(b, off, vals[i])) return c;
    (in_count == 0 ? c.empty_inputs : c.nonempty_inputs) = true;
    (out_count == 0 ? c.empty_outputs : c.nonempty_outputs) = true;
    for (size_t i = 0; i < n; ++i) {
      if (vals[i] == 0) c.value_zero = true;
      else if (vals[i] <= MMAX) c.value_ok = true;
      else c.value_over = true;
    }
    break;
  }
  case W::LOCKTIME: {
    uint32_t lt;
    if (!W::get_u32(b, off, lt)) return c;
    (locktimeType(lt) ? c.lt_height : c.lt_timestamp) = true;
    break;
  }
  case W::SUBSIDY: {
    uint64_t h;
    if (!W::get_u64(b, off, h)) return c;
    (h < 33 * H ? c.subsidy_pre33 : c.subsidy_post33) = true;
    break;
  }
  case W::SUPPLY: {
    uint64_t h;
    if (!W::get_u64(b, off, h)) return c;
    (h < 33 * H ? c.supply_pre33 : c.supply_post33) = true;
    break;
  }
  case W::CLAMP: {
    uint64_t raw, expected;
    if (!W::get_u64(b, off, raw) || !W::get_u64(b, off, expected)) return c;
    c.clamp_arm = clampTime(raw, expected) != raw;
    break;
  }
  default:
    break;
  }
  return c;
}

static void test_mutator_coverage() {
  Coverage cov;
  constexpr size_t MAX_SIZE = 256;
  uint8_t buf[MAX_SIZE];
  bool decode_ok = true;
  for (unsigned seed = 0; seed < 512; ++seed) {
    size_t n = LLVMFuzzerCustomMutator(buf, 0, MAX_SIZE, seed);
    if (n < 1 || n > MAX_SIZE) {
      decode_ok = false;
      break;
    }
    Coverage c = decode(buf, n);
    cov.empty_inputs |= c.empty_inputs;
    cov.nonempty_inputs |= c.nonempty_inputs;
    cov.empty_outputs |= c.empty_outputs;
    cov.nonempty_outputs |= c.nonempty_outputs;
    cov.value_ok |= c.value_ok;
    cov.value_over |= c.value_over;
    cov.value_zero |= c.value_zero;
    cov.lt_height |= c.lt_height;
    cov.lt_timestamp |= c.lt_timestamp;
    cov.subsidy_pre33 |= c.subsidy_pre33;
    cov.subsidy_post33 |= c.subsidy_post33;
    cov.supply_pre33 |= c.supply_pre33;
    cov.supply_post33 |= c.supply_post33;
    cov.clamp_arm |= c.clamp_arm;
  }
  CHECK(decode_ok, "mutator outputs in range");
  CHECK(cov.empty_inputs && cov.nonempty_inputs, "inputCount 0 and nonzero seen");
  CHECK(cov.empty_outputs && cov.nonempty_outputs, "outputCount 0 and nonzero seen");
  CHECK(cov.value_ok, "in-range value seen");
  CHECK(cov.value_over, "Mmax+1 value seen (invalid arm)");
  CHECK(cov.value_zero, "zero value seen");
  CHECK(cov.lt_height && cov.lt_timestamp, "both locktime types seen");
  CHECK(cov.subsidy_pre33 && cov.subsidy_post33, "subsidy both sides of 33H");
  CHECK(cov.supply_pre33 && cov.supply_post33, "supply both sides of 33H");
  CHECK(cov.clamp_arm, "clamp arm triggered");
}

int main() {
  test_check_transaction();
  test_locktime();
  test_subsidy();
  test_supply();
  test_clamp();
  test_mutator_coverage();
  if (failures == 0) {
    std::printf("test_spec_lock: all checks passed\n");
    return 0;
  }
  std::fprintf(stderr, "test_spec_lock: %d failure(s)\n", failures);
  return 1;
}
