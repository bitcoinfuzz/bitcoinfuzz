#ifndef BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_INVOICE_H
#define BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_INVOICE_H

#define template c_template

extern "C" {
#include <external/secp256k1/include/secp256k1.h>
#include <external/secp256k1/include/secp256k1_extrakeys.h>
#include <external/secp256k1/include/secp256k1_schnorrsig.h>
}

#undef template

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <custommutator/utils/bech32.h>
#include <random>
#include <string>
#include <string_view>
#include <vector>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

// A custom mutator for BOLT12 invoices (lni). It decodes the bech32 string,
// mutates the TLV stream, and then, most of the time, re-signs the invoice:
// it derives a signing key from the TLV contents, sets invoice_node_id (and
// usually offer_issuer_id) to that key's public key, recomputes the BOLT12
// merkle root and signs it with BIP340. Roughly one input in eight keeps its
// old signature, so invalid signatures are still exercised.
namespace bolt12_invoice_mutator {

using Bytes = std::vector<uint8_t>;
using Hash = std::array<uint8_t, 32>;

constexpr std::string_view HRP = "lni";
constexpr std::string_view SIGNATURE_TAG = "lightninginvoicesignature";
constexpr uint64_t SIGNATURE_TYPE = 240;
constexpr uint64_t SIGNATURE_END = 1000;
constexpr uint64_t OFFER_ISSUER_ID = 22;
constexpr uint64_t INVOICE_NODE_ID = 176;

// A valid, signed invoice used as the starting point when the input is not a
// parseable lni string (for example with an empty corpus).
constexpr char SEED_INVOICE[] =
    "lni1qqsqzqsrqszsvpcgpy9qkrqdpc83qygjzv2p29shrqv35xcur50p7gq2pf6x2um5yphkve"
    "n9wgtzzqh9ws8x8wkjszq76l8k2nwkcxgznjsr8qhuqk447hw6s8ev2kuytdfqyqlgtqssyavj"
    "426agdscmksnlwn3uwvne4630fcj60dyjejvqmhph5737u905zrq9j2fe0y53kuyf5kdqjqsgv"
    "les2ngpd4ftc6xrlepppyjlyj8lmanqtt7cguv6pvgxl6yx90m3kepxncpmt4ptsgryry3w0na"
    "ta507tvaqqgz3khh752g4dls0a5d0rthgwcwy9lsytsxf22md4fznjpsrffk3z0sqgqpqgpsgp"
    "gxquyqjzstpsxsurcszyfpx9q4zct3sxg6rvwp68slyz3pcqqqq05qqqqqvsqrqqqqqqqqqqqq"
    "qyqqqqqq8wdv5qqqqzjqge2n7yq2sgqa9uverg284lv6u8gu6jd595wajdd0ffvh2ktn5l0d3w"
    "8cfhmttk4qyqlgkqss9et5pe3m455qs8kheaj5m4kpjq5u5qec9lq94d04mk5p7tz4hpzm7pq9"
    "3xlhmrup065gf00dt39k4c20sqxt60psa48jj2rn5t5930sat6a78ld6wlml9e9tw6r9ad25ag"
    "8z7t0lg2q4xxfk2z60nfy9mc5d5q";

struct Tlv {
  uint64_t type;
  Bytes value;
};

inline secp256k1_context *Context() {
  static secp256k1_context *ctx =
      secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  return ctx;
}

inline bool IsSignatureType(uint64_t type) {
  return type >= SIGNATURE_TYPE && type <= SIGNATURE_END;
}

inline void PutBigSize(Bytes &out, uint64_t n) {
  if (n < 0xfd) {
    out.push_back(static_cast<uint8_t>(n));
  } else if (n <= 0xffff) {
    out.push_back(0xfd);
    for (int i = 1; i >= 0; --i)
      out.push_back(static_cast<uint8_t>(n >> (8 * i)));
  } else if (n <= 0xffffffff) {
    out.push_back(0xfe);
    for (int i = 3; i >= 0; --i)
      out.push_back(static_cast<uint8_t>(n >> (8 * i)));
  } else {
    out.push_back(0xff);
    for (int i = 7; i >= 0; --i)
      out.push_back(static_cast<uint8_t>(n >> (8 * i)));
  }
}

inline bool ReadBigSize(const Bytes &in, size_t &pos, uint64_t &out) {
  if (pos >= in.size())
    return false;
  uint8_t first = in[pos++];
  size_t extra = first == 0xfd ? 2 : first == 0xfe ? 4 : first == 0xff ? 8 : 0;
  if (extra == 0) {
    out = first;
    return true;
  }
  if (in.size() - pos < extra)
    return false;
  uint64_t v = 0;
  for (size_t i = 0; i < extra; ++i)
    v = (v << 8) | in[pos++];
  uint64_t min = extra == 2 ? 0xfd : extra == 4 ? 0x10000 : 0x100000000ULL;
  if (v < min)
    return false;
  out = v;
  return true;
}

inline bool ParseTlvs(const Bytes &in, std::vector<Tlv> &out) {
  size_t pos = 0;
  while (pos < in.size()) {
    Tlv t;
    uint64_t len;
    if (!ReadBigSize(in, pos, t.type) || !ReadBigSize(in, pos, len))
      return false;
    if (len > in.size() - pos)
      return false;
    t.value.assign(in.begin() + pos, in.begin() + pos + len);
    pos += len;
    out.push_back(std::move(t));
  }
  return true;
}

inline Bytes EncodeTlv(const Tlv &t) {
  Bytes out;
  PutBigSize(out, t.type);
  PutBigSize(out, t.value.size());
  out.insert(out.end(), t.value.begin(), t.value.end());
  return out;
}

inline Bytes FiveToEight(const Bytes &in) {
  Bytes out;
  uint32_t acc = 0;
  int bits = 0;
  for (uint8_t v : in) {
    acc = (acc << 5) | (v & 31);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 0xff));
    }
    acc &= (1u << bits) - 1;
  }
  return out;
}

inline Bytes EightToFive(const Bytes &in) {
  Bytes out;
  uint32_t acc = 0;
  int bits = 0;
  for (uint8_t b : in) {
    acc = (acc << 8) | b;
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 31));
    }
    acc &= (1u << bits) - 1;
  }
  if (bits)
    out.push_back(static_cast<uint8_t>((acc << (5 - bits)) & 31));
  return out;
}

inline Hash Tagged(const uint8_t *tag, size_t taglen, const uint8_t *msg,
                   size_t msglen) {
  Hash h{};
  secp256k1_tagged_sha256(Context(), h.data(), tag, taglen, msg, msglen);
  return h;
}

inline Hash TaggedStr(std::string_view tag, const Bytes &msg) {
  return Tagged(reinterpret_cast<const uint8_t *>(tag.data()), tag.size(),
                msg.data(), msg.size());
}

inline Hash Branch(const Hash &a, const Hash &b) {
  bool a_first = std::memcmp(a.data(), b.data(), 32) <= 0;
  const Hash &lo = a_first ? a : b;
  const Hash &hi = a_first ? b : a;
  Bytes msg(lo.begin(), lo.end());
  msg.insert(msg.end(), hi.begin(), hi.end());
  return TaggedStr("LnBranch", msg);
}

// tlvs must be sorted by type, non-empty and contain no signature TLVs.
inline Hash MerkleRoot(const std::vector<Tlv> &tlvs) {
  Bytes first = EncodeTlv(tlvs[0]);
  Bytes nonce_tag{'L', 'n', 'N', 'o', 'n', 'c', 'e'};
  nonce_tag.insert(nonce_tag.end(), first.begin(), first.end());
  std::vector<Hash> nodes;
  for (const Tlv &t : tlvs) {
    Hash leaf = TaggedStr("LnLeaf", EncodeTlv(t));
    Bytes type_bytes;
    PutBigSize(type_bytes, t.type);
    Hash nonce = Tagged(nonce_tag.data(), nonce_tag.size(), type_bytes.data(),
                        type_bytes.size());
    nodes.push_back(Branch(leaf, nonce));
  }
  while (nodes.size() > 1) {
    std::vector<Hash> next;
    for (size_t i = 0; i + 1 < nodes.size(); i += 2)
      next.push_back(Branch(nodes[i], nodes[i + 1]));
    if (nodes.size() % 2)
      next.push_back(nodes.back());
    nodes = std::move(next);
  }
  return nodes[0];
}

inline void SortTlvs(std::vector<Tlv> &tlvs) {
  std::stable_sort(tlvs.begin(), tlvs.end(),
                   [](const Tlv &a, const Tlv &b) { return a.type < b.type; });
}

inline void SetTlv(std::vector<Tlv> &tlvs, uint64_t type, const Bytes &value) {
  for (Tlv &t : tlvs) {
    if (t.type == type) {
      t.value = value;
      return;
    }
  }
  tlvs.push_back({type, value});
}

inline bool HasTlv(const std::vector<Tlv> &tlvs, uint64_t type) {
  for (const Tlv &t : tlvs)
    if (t.type == type)
      return true;
  return false;
}

inline void Resign(std::vector<Tlv> &tlvs, std::mt19937 &rng) {
  tlvs.erase(
      std::remove_if(tlvs.begin(), tlvs.end(),
                     [](const Tlv &t) { return t.type == SIGNATURE_TYPE; }),
      tlvs.end());

  // The key depends on the contents, except for the fields we overwrite below.
  Bytes material;
  for (const Tlv &t : tlvs) {
    if (t.type == OFFER_ISSUER_ID || t.type == INVOICE_NODE_ID ||
        IsSignatureType(t.type))
      continue;
    Bytes raw = EncodeTlv(t);
    material.insert(material.end(), raw.begin(), raw.end());
  }
  Hash key = TaggedStr("bitcoinfuzz/bolt12-invoice-key", material);

  secp256k1_keypair keypair;
  if (!secp256k1_keypair_create(Context(), &keypair, key.data()))
    return;
  secp256k1_xonly_pubkey xonly;
  if (!secp256k1_keypair_xonly_pub(Context(), &xonly, nullptr, &keypair))
    return;
  uint8_t x[32];
  secp256k1_xonly_pubkey_serialize(Context(), x, &xonly);
  Bytes node_id{0x02};
  node_id.insert(node_id.end(), x, x + 32);

  SetTlv(tlvs, INVOICE_NODE_ID, node_id);
  if (HasTlv(tlvs, OFFER_ISSUER_ID) && rng() % 8 != 0)
    SetTlv(tlvs, OFFER_ISSUER_ID, node_id);
  SortTlvs(tlvs);

  std::vector<Tlv> signed_part;
  for (const Tlv &t : tlvs)
    if (!IsSignatureType(t.type))
      signed_part.push_back(t);
  if (signed_part.empty())
    return;

  Hash root = MerkleRoot(signed_part);
  Hash msg = TaggedStr(SIGNATURE_TAG, Bytes(root.begin(), root.end()));
  uint8_t sig[64];
  if (!secp256k1_schnorrsig_sign32(Context(), sig, msg.data(), &keypair,
                                   nullptr))
    return;
  tlvs.push_back({SIGNATURE_TYPE, Bytes(sig, sig + 64)});
}

inline void MutateOnce(std::vector<Tlv> &tlvs, std::mt19937 &rng) {
  // Types around the offer, invoice request and invoice ranges.
  // Includes types outside the ranges a writer may use (1001 to 999999999),
  // so that implementations disagreeing on them are found. Modules skip the
  // known discrepancies.
  static const uint64_t TYPES[] = {
      0,   80,  82,  84,   86,        88,         89,        90,
      160, 162, 164, 166,  168,       170,        172,       174,
      176, 240, 241, 1001, 999999999, 1000000001, 3999999999};
  auto random_type = [&]() {
    return TYPES[rng() % (sizeof(TYPES) / sizeof(TYPES[0]))];
  };
  auto random_bytes = [&](size_t n) {
    Bytes b(n);
    for (auto &v : b)
      v = static_cast<uint8_t>(rng());
    return b;
  };

  switch (rng() % 5) {
  case 0:
    if (!tlvs.empty()) {
      Tlv &t = tlvs[rng() % tlvs.size()];
      size_t cap = t.value.size() + 64;
      Bytes buf(cap);
      std::copy(t.value.begin(), t.value.end(), buf.begin());
      size_t n = LLVMFuzzerMutate(buf.data(), t.value.size(), cap);
      t.value.assign(buf.begin(), buf.begin() + n);
    }
    break;
  case 1:
    if (!tlvs.empty()) {
      size_t i = rng() % tlvs.size();
      tlvs.erase(tlvs.begin() + i);
    }
    break;
  case 2:
    if (!tlvs.empty()) {
      Tlv copy = tlvs[rng() % tlvs.size()];
      tlvs.push_back(std::move(copy));
    }
    break;
  case 3: {
    uint64_t type = random_type();
    tlvs.push_back({type, random_bytes(rng() % 40)});
    break;
  }
  default:
    if (!tlvs.empty()) {
      Tlv &t = tlvs[rng() % tlvs.size()];
      t.type = random_type();
    }
    break;
  }
}

inline bool DecodeToTlvs(std::string_view str, std::vector<Tlv> &out) {
  bech32::DecodeResult dec =
      bech32::DecodeNoChecksum(str, bech32::CharLimit::CUSTOM_MUTATOR);
  if (dec.encoding != bech32::Encoding::BECH32 || dec.hrp != HRP)
    return false;
  return ParseTlvs(FiveToEight(dec.data), out);
}

} // namespace bolt12_invoice_mutator

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  using namespace bolt12_invoice_mutator;
  std::mt19937 rng(seed);

  std::vector<Tlv> tlvs;
  std::string_view input(reinterpret_cast<char *>(fuzz_data), size);
  if (!DecodeToTlvs(input, tlvs)) {
    tlvs.clear();
    DecodeToTlvs(SEED_INVOICE, tlvs);
  }

  bool resign = rng() % 8 != 0;
  size_t rounds = 1 + rng() % 3;
  for (size_t i = 0; i < rounds; ++i)
    MutateOnce(tlvs, rng);
  if (resign)
    Resign(tlvs, rng);
  SortTlvs(tlvs);

  Bytes stream;
  for (const Tlv &t : tlvs) {
    Bytes raw = EncodeTlv(t);
    stream.insert(stream.end(), raw.begin(), raw.end());
  }
  std::string encoded = bech32::EncodeNoChecksum(HRP, EightToFive(stream));
  if (encoded.size() > max_size)
    return size;

  std::memcpy(fuzz_data, encoded.data(), encoded.size());
  return encoded.size();
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_INVOICE_H
