// Tests that the SpecLock boundary tables actually reach PSBT field values:
// PSBT_IN_WITNESS_UTXO amounts are seeded from OUTPUT_VALUES (CheckTransaction
// Mmax arm) and PSBT_*_LOCKTIME fields from LOCKTIMES (LocktimeType threshold).
//
// Runs the PSBTv0 custom mutator over its seed space, feeds each output back
// as the next input (like a real fuzz run), and requires that every boundary
// value appears in the corresponding parsed field at least once.

#define CUSTOM_MUTATOR_PSBT_V0
#include "../mutators/psbt.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <span>
#include <vector>

// Stub for the default mutator: pseudo-random bytes.
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize) {
  (void)Size;
  static uint64_t state = 0x452821e638d01377ULL;
  size_t n = MaxSize < 16 ? MaxSize : 16;
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

static uint64_t le64(const std::vector<uint8_t> &v) {
  uint64_t x = 0;
  for (int i = 0; i < 8; ++i)
    x |= static_cast<uint64_t>(v[i]) << (8 * i);
  return x;
}

static uint32_t le32(const std::vector<uint8_t> &v) {
  uint32_t x = 0;
  for (int i = 0; i < 4; ++i)
    x |= static_cast<uint32_t>(v[i]) << (8 * i);
  return x;
}

int main() {
  constexpr size_t MAX_SIZE = 4096;
  std::vector<uint8_t> buf(MAX_SIZE);
  size_t cur = 0;

  std::set<uint64_t> amounts_seen;
  std::set<uint32_t> locktimes_seen;
  bool all_wellformed = true;

  for (unsigned seed = 0; seed < 5000; ++seed) {
    size_t n = LLVMFuzzerCustomMutator(buf.data(), cur, MAX_SIZE, seed);
    cur = n;
    if (n < 5 || std::memcmp(buf.data(), PSBT_MAGIC, 5) != 0) {
      all_wellformed = false;
      break;
    }
    std::vector<Map> maps = parse_psbt(buf.data(), n);
    if (maps.empty()) {
      all_wellformed = false;
      break;
    }
    // maps[0] is the global map; the rest are input/output maps.
    for (size_t mi = 0; mi < maps.size(); ++mi) {
      for (const Record &rec : maps[mi].records) {
        if (rec.key.size() != 1)
          continue;
        if (mi == 0 && rec.key[0] == 0x03 && rec.value.size() == 4) {
          // PSBT_GLOBAL_FALLBACK_LOCKTIME
          locktimes_seen.insert(le32(rec.value));
        }
        if (mi > 0 && rec.key[0] == 0x01 && rec.value.size() >= 8) {
          // PSBT_IN_WITNESS_UTXO: <8-byte amount><compactsize script>...
          amounts_seen.insert(le64(rec.value));
        }
      }
    }
  }

  CHECK(all_wellformed, "every mutator output is a well-formed PSBT");

  // Each SpecLock boundary amount must have reached a WITNESS_UTXO field.
  for (uint64_t v : spec_lock::boundaries::OUTPUT_VALUES) {
    char msg[64];
    std::snprintf(msg, sizeof(msg), "amount boundary %llu reached a field",
                  static_cast<unsigned long long>(v));
    CHECK(amounts_seen.count(v) != 0, msg);
  }
  // Each SpecLock boundary locktime must have reached a LOCKTIME field.
  for (uint64_t lt : spec_lock::boundaries::LOCKTIMES) {
    char msg[64];
    std::snprintf(msg, sizeof(msg), "locktime boundary %llu reached a field",
                  static_cast<unsigned long long>(lt));
    CHECK(locktimes_seen.count(static_cast<uint32_t>(lt)) != 0, msg);
  }

  if (failures == 0) {
    std::printf("test_psbt_spec_lock: all checks passed (%zu amounts, %zu locktimes seen)\n",
                amounts_seen.size(), locktimes_seen.size());
    return 0;
  }
  std::fprintf(stderr, "test_psbt_spec_lock: %d failure(s)\n", failures);
  return 1;
}
