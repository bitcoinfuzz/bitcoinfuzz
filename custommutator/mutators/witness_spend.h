#ifndef BITCOINFUZZ_CUSTOMMUTATOR_WITNESS_SPEND_H
#define BITCOINFUZZ_CUSTOMMUTATOR_WITNESS_SPEND_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <custommutator/utils/crypto/ripemd160.h>
#include <custommutator/utils/crypto/sha256.h>

#define template c_template
extern "C" {
#if __has_include(<external/secp256k1/include/secp256k1.h>)
#include <external/secp256k1/include/secp256k1.h>
#include <external/secp256k1/include/secp256k1_extrakeys.h>
#elif __has_include(<secp256k1.h>)
#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#endif
}
#undef template

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

namespace {

// BIP-342 OP_SUCCESS opcodes that unconditionally pass in Tapscript
constexpr uint8_t OP_SUCCESS_OPCODES[] = {
    0x7e, 0x7f, 0x80, 0x83, 0x84, 0x85, 0x86, 0x8b, 0x8c, 0x8d, 0x8e,
    0x95, 0x96, 0x97, 0x98, 0x99, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf};

// Well-known internal pubkey (generator G x-only coordinate)
constexpr uint8_t INTERNAL_PUBKEY_G[32] = {
    0x79, 0xbe, 0x66, 0x7e, 0xf9, 0xdc, 0xbb, 0xac, 0x55, 0xa0, 0x62,
    0x95, 0xce, 0x87, 0x0b, 0x07, 0x02, 0x9b, 0xfc, 0xdb, 0x2d, 0xce,
    0x28, 0xd9, 0x59, 0xf2, 0x81, 0x5b, 0x16, 0xf8, 0x17, 0x98};

// Compressed pubkey G (33 bytes)
constexpr uint8_t COMPRESSED_PUBKEY_G[33] = {
    0x02, 0x79, 0xbe, 0x66, 0x7e, 0xf9, 0xdc, 0xbb, 0xac, 0x55, 0xa0,
    0x62, 0x95, 0xce, 0x87, 0x0b, 0x07, 0x02, 0x9b, 0xfc, 0xdb, 0x2d,
    0xce, 0x28, 0xd9, 0x59, 0xf2, 0x81, 0x5b, 0x16, 0xf8, 0x17, 0x98};

secp256k1_context *get_secp_ctx() {
  static secp256k1_context *ctx = nullptr;
  if (!ctx) {
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  }
  return ctx;
}

void bip340_tagged_hash(const char *tag, size_t tag_len, const uint8_t *data,
                        size_t data_len, uint8_t *out32) {
  uint8_t tag_hash[32];
  CSHA256()
      .Write(reinterpret_cast<const uint8_t *>(tag), tag_len)
      .Finalize(tag_hash);
  CSHA256 hasher;
  hasher.Write(tag_hash, 32);
  hasher.Write(tag_hash, 32);
  hasher.Write(data, data_len);
  hasher.Finalize(out32);
}

void write_compact_size(std::vector<uint8_t> &out, size_t n) {
  if (n < 253) {
    out.push_back(static_cast<uint8_t>(n));
  } else if (n <= 0xFFFF) {
    out.push_back(253);
    out.push_back(static_cast<uint8_t>(n & 0xFF));
    out.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
  } else {
    out.push_back(254);
    out.push_back(static_cast<uint8_t>(n & 0xFF));
    out.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((n >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((n >> 24) & 0xFF));
  }
}

void write_vector_item(std::vector<uint8_t> &out,
                       const std::vector<uint8_t> &item) {
  write_compact_size(out, item.size());
  out.insert(out.end(), item.begin(), item.end());
}

} // namespace

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  size_t mutated_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

  // In 50% of mutations, allow raw libFuzzer byte perturbations
  if (mutated_size >= 64 && (seed % 2 == 0)) {
    return mutated_size;
  }

  std::srand(seed);
  secp256k1_context *ctx = get_secp_ctx();
  if (!ctx) {
    return mutated_size;
  }

  // Choose Archetype:
  // 0: Taproot BIP-342 Script Path with OP_SUCCESS
  // 1: Taproot BIP-342 Script Path with arithmetic/logic
  // 2: Taproot BIP-341 Key Path Spend
  // 3: SegWit v0 BIP-141 P2WPKH
  // 4: SegWit v0 BIP-141 P2WSH
  int mode = std::rand() % 5;

  std::vector<uint8_t> spent_spk;
  std::vector<std::vector<uint8_t>> witness_items;

  if (mode == 0 || mode == 1) {
    // Taproot Script Path Spend
    std::vector<uint8_t> script;
    if (mode == 0) {
      // Inject OP_SUCCESS opcode
      uint8_t op = OP_SUCCESS_OPCODES[std::rand() % sizeof(OP_SUCCESS_OPCODES)];
      script.push_back(op);
      // Append random trailing script garbage to prove unconditional success
      size_t garbage_len = std::rand() % 16;
      for (size_t i = 0; i < garbage_len; ++i) {
        script.push_back(static_cast<uint8_t>(std::rand() & 0xFF));
      }
    } else {
      // OP_ADD OP_EQUAL
      script = {0x93, 0x87};
      witness_items.push_back({0x03}); // target sum
      witness_items.push_back({0x01}); // operand 2
      witness_items.push_back({0x02}); // operand 1
    }

    // Compute TapLeaf hash (leaf version 0xC0 for Tapscript)
    std::vector<uint8_t> leaf_data;
    leaf_data.push_back(0xc0);
    write_compact_size(leaf_data, script.size());
    leaf_data.insert(leaf_data.end(), script.begin(), script.end());

    uint8_t tapleaf_hash[32];
    bip340_tagged_hash("TapLeaf", 7, leaf_data.data(), leaf_data.size(),
                       tapleaf_hash);

    // Compute TapTweak: tagged_hash("TapTweak", internal_pubkey ||
    // tapleaf_hash)
    uint8_t tweak_data[64];
    std::memcpy(tweak_data, INTERNAL_PUBKEY_G, 32);
    std::memcpy(tweak_data + 32, tapleaf_hash, 32);

    uint8_t taptweak[32];
    bip340_tagged_hash("TapTweak", 8, tweak_data, 64, taptweak);

    secp256k1_xonly_pubkey internal_pk;
    if (!secp256k1_xonly_pubkey_parse(ctx, &internal_pk, INTERNAL_PUBKEY_G)) {
      return mutated_size;
    }

    secp256k1_pubkey out_pk;
    if (!secp256k1_xonly_pubkey_tweak_add(ctx, &out_pk, &internal_pk,
                                          taptweak)) {
      return mutated_size;
    }

    secp256k1_xonly_pubkey out_xonly;
    int parity = 0;
    secp256k1_xonly_pubkey_from_pubkey(ctx, &out_xonly, &parity, &out_pk);
    uint8_t out_pubkey_bytes[32];
    secp256k1_xonly_pubkey_serialize(ctx, out_pubkey_bytes, &out_xonly);

    // spent_spk: OP_1 <32-byte-output-pubkey>
    spent_spk = {0x51, 0x20};
    spent_spk.insert(spent_spk.end(), out_pubkey_bytes, out_pubkey_bytes + 32);

    // Control Block: (0xC0 | parity) || internal_pubkey
    std::vector<uint8_t> control_block;
    control_block.push_back(static_cast<uint8_t>(0xc0 | (parity & 1)));
    control_block.insert(control_block.end(), INTERNAL_PUBKEY_G,
                         INTERNAL_PUBKEY_G + 32);

    // Push script and control block to witness stack
    witness_items.push_back(script);
    witness_items.push_back(control_block);

  } else if (mode == 2) {
    // Taproot Key Path Spend (OP_1 <32-byte-pubkey>)
    spent_spk = {0x51, 0x20};
    spent_spk.insert(spent_spk.end(), INTERNAL_PUBKEY_G,
                     INTERNAL_PUBKEY_G + 32);

    // Dummy 64-byte Schnorr signature
    std::vector<uint8_t> sig(64, 0x01);
    witness_items.push_back(sig);

  } else if (mode == 3) {
    // SegWit v0 P2WPKH: OP_0 <20-byte-hash160>
    uint8_t sha[32];
    CSHA256().Write(COMPRESSED_PUBKEY_G, 33).Finalize(sha);
    uint8_t hash160[20];
    CRIPEMD160().Write(sha, 32).Finalize(hash160);

    spent_spk = {0x00, 0x14};
    spent_spk.insert(spent_spk.end(), hash160, hash160 + 20);

    std::vector<uint8_t> pubkey_item(COMPRESSED_PUBKEY_G,
                                     COMPRESSED_PUBKEY_G + 33);
    std::vector<uint8_t> dummy_sig = {0x30, 0x06, 0x02, 0x01, 0x01,
                                      0x02, 0x01, 0x01, 0x01};
    witness_items.push_back(dummy_sig);
    witness_items.push_back(pubkey_item);

  } else {
    // SegWit v0 P2WSH: OP_0 <32-byte-sha256>
    std::vector<uint8_t> redeem_script = {0x51}; // OP_TRUE
    uint8_t sha[32];
    CSHA256().Write(redeem_script.data(), redeem_script.size()).Finalize(sha);

    spent_spk = {0x00, 0x20};
    spent_spk.insert(spent_spk.end(), sha, sha + 32);

    witness_items.push_back(redeem_script);
  }

  // 20% chance to append Annex (0x50 prefix)
  if (std::rand() % 5 == 0) {
    std::vector<uint8_t> annex = {0x50, 0x01, 0x02, 0x03};
    witness_items.push_back(annex);
  }

  // Build Transaction Envelope:
  // nVersion (4 bytes) = 2
  // marker (1 byte) = 0x00
  // flag (1 byte) = 0x01
  // vin_count (1 byte) = 1
  // txin: prevout_hash (32 bytes 0x00), prevout_n (4 bytes 0x00), scriptSig
  // (0x00), nSequence (0xFFFFFFFF) vout_count (1 byte) = 1 txout: nValue (8
  // bytes 1000 sats), scriptPubKey (OP_TRUE: 0x01, 0x51) witness: item_count,
  // items... nLockTime (4 bytes) = 0
  std::vector<uint8_t> tx;
  // nVersion = 2
  tx.push_back(0x02);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);

  // marker & flag
  tx.push_back(0x00);
  tx.push_back(0x01);

  // vin count = 1
  tx.push_back(0x01);

  // txin prevout hash (32 bytes)
  for (int i = 0; i < 32; ++i) {
    tx.push_back(0x00);
  }
  // prevout index = 0
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);

  // scriptSig length = 0
  tx.push_back(0x00);

  // nSequence = 0xFFFFFFFF
  tx.push_back(0xff);
  tx.push_back(0xff);
  tx.push_back(0xff);
  tx.push_back(0xff);

  // vout count = 1
  tx.push_back(0x01);

  // txout nValue = 1000 satoshis (0x03E8 LE)
  tx.push_back(0xe8);
  tx.push_back(0x03);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);

  // txout scriptPubKey (OP_TRUE: 1 byte len, 0x51)
  tx.push_back(0x01);
  tx.push_back(0x51);

  // witness for input 0
  write_compact_size(tx, witness_items.size());
  for (const auto &item : witness_items) {
    write_vector_item(tx, item);
  }

  // nLockTime = 0
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);
  tx.push_back(0x00);

  // Assembly of Target Input Buffer:
  // [spent_spk] + [tx_bytes] + [has_multi_prevouts=0 (1B)] + [spk_len (1B)] +
  // [amount (8B)] + [input_idx (4B)] + [flags (4B)]
  const size_t tail_len = 1 + 1 + 8 + 4 + 4;
  const size_t total_len = spent_spk.size() + tx.size() + tail_len;

  if (total_len > max_size) {
    return mutated_size;
  }

  uint8_t *p = fuzz_data;
  std::memcpy(p, spent_spk.data(), spent_spk.size());
  p += spent_spk.size();

  std::memcpy(p, tx.data(), tx.size());
  p += tx.size();

  // has_multi_prevouts = false
  *p++ = 0x00;

  // spk_len
  *p++ = static_cast<uint8_t>(spent_spk.size());

  // spent_amount = 1,000,000 satoshis (0x000F4240 int64_t LE)
  int64_t amount = 1000000;
  std::memcpy(p, &amount, 8);
  p += 8;

  // input_index = 0
  uint32_t input_idx = 0;
  std::memcpy(p, &input_idx, 4);
  p += 4;

  // flags: BIP141 + BIP341 + P2SH (0x1U | 0x800U | 0x20000U)
  uint32_t flags = (1U << 0) | (1U << 11) | (1U << 17);
  std::memcpy(p, &flags, 4);

  return total_len;
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_WITNESS_SPEND_H
