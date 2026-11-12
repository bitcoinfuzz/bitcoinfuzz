#ifndef BITCOINFUZZ_CUSTOMMUTATOR_PSBT_H
#define BITCOINFUZZ_CUSTOMMUTATOR_PSBT_H

/**
 * Custom mutator for PSBT (Partially Signed Bitcoin Transaction) inputs,
 * covering both BIP-174 (v0) and BIP-370 (v2) wire formats.
 *
 * WIRE FORMAT (shared by both versions):
 *   <magic:5 bytes = "psbt" 0xff>
 *   <global map>
 *   <input map> x N
 *   <output map> x M
 *
 * Each map is a sequence of key-value records:
 *   <keylen:compactsize><keytype+keydata:keylen
 * bytes><vallen:compactsize><value:vallen bytes>
 *   ...
 *   <0x00>  // zero-length key marks the end of the map (the "separator")
 *
 * This record/map shape is identical for the global, input, and output
 * maps, and identical between PSBTv0 and PSBTv2 -- only the set of key
 * types that are legal within a given map differs by version. That means
 * a single generic parser/serializer can mutate any PSBT without needing
 * to track how many inputs/outputs are declared: just keep splitting the
 * buffer into maps until it runs out.
 *
 * Random byte-level mutation rarely produces a well-formed map (let alone
 * one containing the fields required for a psbt to be accepted at all), so
 * this mutator instead:
 *   1. Parses the input into maps of records.
 *   2. Applies structure-aware mutations: mutate a record's key or value,
 *      remove/duplicate a record, insert a well-known field of the targeted
 *      version, or add/remove a whole map.
 *   3. Reserializes back to the wire format.
 * When the input doesn't parse as a well-formed PSBT container at all
 * (e.g. too short, bad magic), it falls back to a valid template of the
 * targeted version so libFuzzer always has a well-formed structure to mutate
 * from.
 *
 * The targeted version is picked at build time: CUSTOM_MUTATOR_PSBT_V0 for
 * the psbt_v0_parse target, CUSTOM_MUTATOR_PSBT_V2 for psbt_v2_parse.
 */

#if defined(CUSTOM_MUTATOR_PSBT_V0) && defined(CUSTOM_MUTATOR_PSBT_V2)
#error "CUSTOM_MUTATOR_PSBT_V0 and _V2 are mutually exclusive"
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

// SpecLock boundary tables (blvm-spec/lean/SpecLock): amounts seeded from
// OUTPUT_VALUES exercise CheckTransaction's Mmax arm; locktimes seeded from
// LOCKTIMES exercise LocktimeType's 500000000 threshold.
#include "../invariants/spec_lock.h"

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

namespace {

constexpr uint8_t PSBT_MAGIC[5] = {'p', 's', 'b', 't', 0xff};

struct CompactSizeResult {
  bool valid = false;
  uint64_t value = 0;
  size_t bytes_read = 0;
};

// Minimal, self-contained CompactSize codec (Bitcoin's little-endian varint
// format), independent of Bitcoin Core's stream-based (de)serialization so
// this mutator can operate directly on raw fuzzer buffers.
CompactSizeResult decode_compact_size(const uint8_t *data, size_t available) {
  CompactSizeResult r;
  if (available < 1) {
    return r;
  }
  const uint8_t first = data[0];
  if (first < 253) {
    r.valid = true;
    r.value = first;
    r.bytes_read = 1;
  } else if (first == 253) {
    if (available < 3) {
      return r;
    }
    const uint16_t v =
        static_cast<uint16_t>(data[1]) | (static_cast<uint16_t>(data[2]) << 8);
    if (v < 253) {
      return r; // non-canonical
    }
    r.valid = true;
    r.value = v;
    r.bytes_read = 3;
  } else if (first == 254) {
    if (available < 5) {
      return r;
    }
    const uint32_t v = static_cast<uint32_t>(data[1]) |
                       (static_cast<uint32_t>(data[2]) << 8) |
                       (static_cast<uint32_t>(data[3]) << 16) |
                       (static_cast<uint32_t>(data[4]) << 24);
    if (v < 0x10000u) {
      return r; // non-canonical
    }
    r.valid = true;
    r.value = v;
    r.bytes_read = 5;
  } else {
    if (available < 9) {
      return r;
    }
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
      v |= static_cast<uint64_t>(data[1 + i]) << (8 * i);
    }
    if (v < 0x100000000ULL) {
      return r; // non-canonical
    }
    r.valid = true;
    r.value = v;
    r.bytes_read = 9;
  }
  return r;
}

void encode_compact_size(std::vector<uint8_t> &out, uint64_t value) {
  if (value < 253) {
    out.push_back(static_cast<uint8_t>(value));
  } else if (value <= 0xffff) {
    out.push_back(253);
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
  } else if (value <= 0xffffffffu) {
    out.push_back(254);
    for (int i = 0; i < 4; i++) {
      out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
  } else {
    out.push_back(255);
    for (int i = 0; i < 8; i++) {
      out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
  }
}

struct Record {
  // Raw "<keytype><keydata>" bytes, i.e. the key excluding its length
  // prefix.
  std::vector<uint8_t> key;
  std::vector<uint8_t> value;
};

struct Map {
  std::vector<Record> records;
};

// Parses one map (a run of records terminated by a zero-length key) from
// `data[offset..)`. On success, advances `offset` past the separator and
// returns true. Returns false if the buffer runs out before a separator is
// reached, leaving `offset` unspecified.
bool parse_map(const uint8_t *data, size_t size, size_t &offset, Map &out) {
  while (true) {
    const CompactSizeResult keylen =
        decode_compact_size(data + offset, size - offset);
    if (!keylen.valid) {
      return false;
    }
    offset += keylen.bytes_read;
    if (keylen.value == 0) {
      return true; // separator
    }
    if (keylen.value > size - offset) {
      return false;
    }
    Record rec;
    rec.key.assign(data + offset, data + offset + keylen.value);
    offset += keylen.value;

    const CompactSizeResult vallen =
        decode_compact_size(data + offset, size - offset);
    if (!vallen.valid) {
      return false;
    }
    offset += vallen.bytes_read;
    if (vallen.value > size - offset) {
      return false;
    }
    rec.value.assign(data + offset, data + offset + vallen.value);
    offset += vallen.value;

    out.records.push_back(std::move(rec));
  }
}

std::vector<Map> parse_psbt(const uint8_t *data, size_t size) {
  std::vector<Map> maps;
  if (size < sizeof(PSBT_MAGIC) ||
      memcmp(data, PSBT_MAGIC, sizeof(PSBT_MAGIC)) != 0) {
    return maps;
  }
  size_t offset = sizeof(PSBT_MAGIC);
  while (offset < size) {
    Map m;
    if (!parse_map(data, size, offset, m)) {
      break;
    }
    maps.push_back(std::move(m));
  }
  return maps;
}

std::vector<uint8_t> serialize_psbt(const std::vector<Map> &maps) {
  std::vector<uint8_t> out(PSBT_MAGIC, PSBT_MAGIC + sizeof(PSBT_MAGIC));
  for (const Map &m : maps) {
    for (const Record &rec : m.records) {
      encode_compact_size(out, rec.key.size());
      out.insert(out.end(), rec.key.begin(), rec.key.end());
      encode_compact_size(out, rec.value.size());
      out.insert(out.end(), rec.value.begin(), rec.value.end());
    }
    encode_compact_size(out, 0); // separator
  }
  return out;
}

void push_le32(std::vector<uint8_t> &out, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    out.push_back(static_cast<uint8_t>(value >> (8 * i)));
  }
}

void push_le64(std::vector<uint8_t> &out, uint64_t value) {
  for (int i = 0; i < 8; i++) {
    out.push_back(static_cast<uint8_t>(value >> (8 * i)));
  }
}

void push_record(Map &m, uint8_t key_type, std::vector<uint8_t> value) {
  Record rec;
  rec.key = {key_type};
  rec.value = std::move(value);
  m.records.push_back(std::move(rec));
}

// A minimal, valid PSBTv0 template: a global unsigned tx with 1 input and 1
// output, followed by empty input/output maps. Used to seed/reset the corpus
// whenever the fuzzer-provided input doesn't parse as a well-formed PSBT
// container at all.
[[maybe_unused]] std::vector<uint8_t> build_psbtv0_template() {
  std::vector<uint8_t> tx;
  push_le32(tx, 2);           // tx version
  tx.push_back(1);            // vin count
  tx.insert(tx.end(), 32, 0); // prevout txid
  push_le32(tx, 0);           // prevout index
  tx.push_back(0);            // empty scriptSig
  push_le32(tx, 0xffffffff);  // sequence
  tx.push_back(1);            // vout count
  tx.insert(tx.end(), 8, 0);  // amount
  tx.push_back(0);            // empty scriptPubKey
  push_le32(tx, 0);           // locktime

  std::vector<Map> maps(3);
  push_record(maps[0], 0x00, std::move(tx)); // PSBT_GLOBAL_UNSIGNED_TX
  return serialize_psbt(maps);
}

// A minimal, valid PSBTv2 template (1 input, 1 output, required fields
// only). Used to seed/reset the corpus whenever the fuzzer-provided input
// doesn't parse as a well-formed PSBT container at all.
[[maybe_unused]] std::vector<uint8_t> build_psbtv2_template() {
  std::vector<Map> maps(3);

  push_record(maps[0], 0xFB, {2, 0, 0, 0}); // PSBT_GLOBAL_VERSION = 2
  push_record(maps[0], 0x02, {2, 0, 0, 0}); // PSBT_GLOBAL_TX_VERSION = 2
  push_record(maps[0], 0x04, {1});          // PSBT_GLOBAL_INPUT_COUNT = 1
  push_record(maps[0], 0x05, {1});          // PSBT_GLOBAL_OUTPUT_COUNT = 1

  push_record(maps[1], 0x0e,
              std::vector<uint8_t>(32, 0)); // PSBT_IN_PREVIOUS_TXID
  push_record(maps[1], 0x0f, {0, 0, 0, 0}); // PSBT_IN_OUTPUT_INDEX

  {  // PSBT_OUT_AMOUNT: seed the template from the boundary table too
    std::vector<uint8_t> amount;
    push_le64(amount, spec_lock::boundaries::OUTPUT_VALUES[1]); // 1 sat
    push_record(maps[2], 0x03, std::move(amount));
  }
  push_record(maps[2], 0x04, {}); // PSBT_OUT_SCRIPT

  return serialize_psbt(maps);
}

enum class KeyData {
  NONE,   // single-byte key
  PUBKEY, // <keytype><33-byte compressed pubkey>
};

enum class ValueKind {
  RANDOM,   // `value_len` random bytes
  TXOUT,    // <8-byte amount><compactsize script len><script>
  AMOUNT8,  // 8-byte LE amount from the SpecLock OUTPUT_VALUES boundary table
  LOCKTIME4 // 4-byte LE locktime from the SpecLock LOCKTIMES boundary table
};

struct KnownKey {
  uint8_t type;
  size_t value_len; // 0 => pick a random length
  KeyData key_data = KeyData::NONE;
  ValueKind value_kind = ValueKind::RANDOM;
};

// The secp256k1 generator point, compressed. Parsers validate pubkeys in
// keydata, so random bytes would almost never get past that check.
constexpr uint8_t VALID_PUBKEY[33] = {
    0x02, 0x79, 0xbe, 0x66, 0x7e, 0xf9, 0xdc, 0xbb, 0xac, 0x55, 0xa0,
    0x62, 0x95, 0xce, 0x87, 0x0b, 0x07, 0x02, 0x9b, 0xfc, 0xdb, 0x2d,
    0xce, 0x28, 0xd9, 0x59, 0xf2, 0x81, 0x5b, 0x16, 0xf8, 0x17, 0x98};

#ifdef CUSTOM_MUTATOR_PSBT_V0
// BIP-174 key types for a PSBTv0 input/output map. The global pool instead
// holds PSBTv2-only fields, which a v0 parser must reject.
constexpr KnownKey GLOBAL_KEYS[] = {
    {0xFB, 4},                              // PSBT_GLOBAL_VERSION
    {0x02, 4},                              // PSBT_GLOBAL_TX_VERSION
    {0x03, 4, KeyData::NONE, ValueKind::LOCKTIME4}, // PSBT_GLOBAL_FALLBACK_LOCKTIME
};

constexpr KnownKey INPUT_KEYS[] = {
    {0x01, 0, KeyData::NONE, ValueKind::TXOUT}, // PSBT_IN_WITNESS_UTXO
    {0x02, 72, KeyData::PUBKEY},                // PSBT_IN_PARTIAL_SIG
    {0x03, 4},                                  // PSBT_IN_SIGHASH_TYPE
    {0x04, 0},                                  // PSBT_IN_REDEEM_SCRIPT
    {0x05, 0},                                  // PSBT_IN_WITNESS_SCRIPT
    {0x06, 8, KeyData::PUBKEY},                 // PSBT_IN_BIP32_DERIVATION
    {0x07, 0},                                  // PSBT_IN_FINAL_SCRIPTSIG
    {0x08, 0},                                  // PSBT_IN_FINAL_SCRIPTWITNESS
};

constexpr KnownKey OUTPUT_KEYS[] = {
    {0x00, 0},                  // PSBT_OUT_REDEEM_SCRIPT
    {0x01, 0},                  // PSBT_OUT_WITNESS_SCRIPT
    {0x02, 8, KeyData::PUBKEY}, // PSBT_OUT_BIP32_DERIVATION
};

std::vector<uint8_t> build_template() { return build_psbtv0_template(); }
#else
// Key types legal in a PSBTv2 global/input/output map. Weighted towards
// v2-only fields (version, counts, per-input previous-txid/output-index/
// locktimes) since those are exactly what's needed to get past the
// version/count checks that gate the v2 parsing path in every
// implementation under test.
constexpr KnownKey GLOBAL_KEYS[] = {
    {0xFB, 4}, // PSBT_GLOBAL_VERSION
    {0x02, 4}, // PSBT_GLOBAL_TX_VERSION
    {0x03, 4, KeyData::NONE, ValueKind::LOCKTIME4}, // PSBT_GLOBAL_FALLBACK_LOCKTIME
    {0x04, 1}, // PSBT_GLOBAL_INPUT_COUNT
    {0x05, 1}, // PSBT_GLOBAL_OUTPUT_COUNT
    {0x06, 1}, // PSBT_GLOBAL_TX_MODIFIABLE
};

constexpr KnownKey INPUT_KEYS[] = {
    {0x0e, 32}, // PSBT_IN_PREVIOUS_TXID
    {0x0f, 4},  // PSBT_IN_OUTPUT_INDEX
    {0x10, 4},  // PSBT_IN_SEQUENCE
    {0x11, 4, KeyData::NONE, ValueKind::LOCKTIME4}, // PSBT_IN_REQUIRED_TIME_LOCKTIME
    {0x12, 4, KeyData::NONE, ValueKind::LOCKTIME4}, // PSBT_IN_REQUIRED_HEIGHT_LOCKTIME
};

constexpr KnownKey OUTPUT_KEYS[] = {
    {0x03, 8, KeyData::NONE, ValueKind::AMOUNT8}, // PSBT_OUT_AMOUNT
    {0x04, 0},                                    // PSBT_OUT_SCRIPT
};

std::vector<uint8_t> build_template() { return build_psbtv2_template(); }
#endif

std::vector<uint8_t> random_bytes(size_t n) {
  std::vector<uint8_t> v(n);
  for (uint8_t &b : v) {
    b = static_cast<uint8_t>(rand() & 0xFF);
  }
  return v;
}

// Picks one entry of a SpecLock boundary table (see invariants/spec_lock.h).
// These are the exact branch boundaries the Lean proofs case-split on, so
// biasing mutations toward them drives every consensus arm deterministically.
uint64_t pick_boundary(const uint64_t *table, size_t n) {
  return table[static_cast<size_t>(rand()) % n];
}

Record make_known_record(const KnownKey &k) {
  Record rec;
  rec.key = {k.type};
  if (k.key_data == KeyData::PUBKEY) {
    rec.key.insert(rec.key.end(), std::begin(VALID_PUBKEY),
                   std::end(VALID_PUBKEY));
  }
  const size_t len =
      k.value_len ? k.value_len : static_cast<size_t>(rand() % 64);
  if (k.value_kind == ValueKind::TXOUT) {
    // Witness UTXO: 8-byte amount from the CheckTransaction boundary table
    // (0, 1, Mmax-1, Mmax, Mmax+1, UINT64_MAX), then script.
    rec.value.clear();
    push_le64(rec.value,
              pick_boundary(spec_lock::boundaries::OUTPUT_VALUES,
                            std::size(spec_lock::boundaries::OUTPUT_VALUES)));
    encode_compact_size(rec.value, len);
    const std::vector<uint8_t> script = random_bytes(len);
    rec.value.insert(rec.value.end(), script.begin(), script.end());
  } else if (k.value_kind == ValueKind::AMOUNT8) {
    rec.value.clear();
    push_le64(rec.value,
              pick_boundary(spec_lock::boundaries::OUTPUT_VALUES,
                            std::size(spec_lock::boundaries::OUTPUT_VALUES)));
  } else if (k.value_kind == ValueKind::LOCKTIME4) {
    rec.value.clear();
    const uint32_t lt = static_cast<uint32_t>(pick_boundary(
        spec_lock::boundaries::LOCKTIMES,
        std::size(spec_lock::boundaries::LOCKTIMES)));
    push_le32(rec.value, lt);
  } else {
    rec.value = random_bytes(len);
  }
  return rec;
}

// Applies one structure-aware mutation to `maps` in place.
void mutate_maps(std::vector<Map> &maps) {
  if (maps.empty()) {
    return;
  }
  const size_t map_idx = static_cast<size_t>(rand()) % maps.size();

  switch (rand() % 6) {
  case 0: { // mutate an existing record's value bytes
    Map &m = maps[map_idx];
    if (m.records.empty()) {
      break;
    }
    Record &rec = m.records[static_cast<size_t>(rand()) % m.records.size()];
    const size_t old_size = rec.value.size();
    rec.value.resize(old_size + 16);
    const size_t new_size =
        LLVMFuzzerMutate(rec.value.data(), old_size, rec.value.size());
    rec.value.resize(new_size);
    break;
  }
  case 1: { // mutate an existing record's key bytes
    Map &m = maps[map_idx];
    if (m.records.empty()) {
      break;
    }
    Record &rec = m.records[static_cast<size_t>(rand()) % m.records.size()];
    if (rec.key.empty()) {
      break;
    }
    LLVMFuzzerMutate(rec.key.data(), rec.key.size(), rec.key.size());
    break;
  }
  case 2: { // remove a record
    Map &m = maps[map_idx];
    if (m.records.empty()) {
      break;
    }
    m.records.erase(
        m.records.begin() +
        static_cast<long>(static_cast<size_t>(rand()) % m.records.size()));
    break;
  }
  case 3: { // duplicate a record (exercises duplicate-key rejection paths)
    Map &m = maps[map_idx];
    if (m.records.empty()) {
      break;
    }
    m.records.push_back(
        m.records[static_cast<size_t>(rand()) % m.records.size()]);
    break;
  }
  case 4: { // insert a well-known field of the targeted version
    const int which = rand() % 3;
    const KnownKey *pool = GLOBAL_KEYS;
    size_t pool_size = std::size(GLOBAL_KEYS);
    if (which == 1) {
      pool = INPUT_KEYS;
      pool_size = std::size(INPUT_KEYS);
    } else if (which == 2) {
      pool = OUTPUT_KEYS;
      pool_size = std::size(OUTPUT_KEYS);
    }
    maps[map_idx].records.push_back(
        make_known_record(pool[static_cast<size_t>(rand()) % pool_size]));
    break;
  }
  case 5: { // add or remove a whole map (grows/shrinks the apparent
            // input/output count)
    if (maps.size() <= 1 || rand() % 2 == 0) {
      maps.emplace_back();
    } else {
      maps.erase(maps.begin() + static_cast<long>(map_idx));
    }
    break;
  }
  }
}

} // namespace

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  srand(seed);

  std::vector<Map> maps = parse_psbt(fuzz_data, size);
  if (maps.empty()) {
    const std::vector<uint8_t> tmpl = build_template();
    const size_t n = std::min(tmpl.size(), max_size);
    memcpy(fuzz_data, tmpl.data(), n);
    return n;
  }

  const int num_mutations = 1 + (rand() % 3);
  for (int i = 0; i < num_mutations; i++) {
    mutate_maps(maps);
  }

  std::vector<uint8_t> out = serialize_psbt(maps);
  if (out.size() > max_size) {
    out.resize(max_size);
  }
  memcpy(fuzz_data, out.data(), out.size());
  return out.size();
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_PSBT_H
