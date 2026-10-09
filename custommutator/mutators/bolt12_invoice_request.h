#ifndef BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_INVOICE_REQUEST_H
#define BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_INVOICE_REQUEST_H

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

// A custom mutator for BOLT12 invoice requests (lnr). It decodes the bech32
// string, mutates the TLV stream, and then, most of the time, re-signs the
// request: it derives a payer key from the TLV contents, sets invreq_payer_id
// to that key, recomputes the BOLT12 merkle root and signs it with BIP340.
// Roughly one input in eight keeps its old signature, so invalid signatures
// are still exercised. Unknown TLV types and missing fields are generated
// deliberately; inputs that cause module mismatches are not filtered out.
namespace bolt12_invoice_request_mutator {

using Bytes = std::vector<uint8_t>;
using Hash = std::array<uint8_t, 32>;

constexpr std::string_view HRP = "lnr";
constexpr std::string_view SIGNATURE_TAG = "lightninginvoice_requestsignature";
constexpr uint64_t SIGNATURE_TYPE = 240;
constexpr uint64_t SIGNATURE_END = 1000;
constexpr uint64_t PAYER_ID = 88;

// A signed invoice request used as the starting point when the input is not a
// parseable lnr string (for example with an empty corpus).
constexpr char SEED_INVOICE_REQUEST[] =
    "lnr1qqyqqqqqqqqqqqqqqcp4256ypqqkgzshgysy6ct5dpjk6ct5d93kzmpq23ex2ct5d9ek"
    "293pqthvwfzadd7jejes8q9lhc4rvjxd022zv5l44g6qah82ru5rdpnpjkppqvjx204vgdz"
    "gsqpvcp4mldl3plscny0rt707gvpdh6ndydfacz43euzqhrurageg3n7kafgsek6gz3e9w5"
    "2parv8gs2hlxzk95tzeswywffxlkeyhml0hh46kndmwf4m6xma3tkq2lu04qz3slje2rfth"
    "c89vss";

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
  // secp256k1 aborts on a NULL msg even when msglen is 0, and data() of an
  // empty vector may be NULL (e.g. key material of an empty TLV stream).
  static constexpr uint8_t EMPTY[1] = {0};
  Hash h{};
  secp256k1_tagged_sha256(Context(), h.data(), tag, taglen,
                          msglen ? msg : EMPTY, msglen);
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

inline void Resign(std::vector<Tlv> &tlvs) {
  tlvs.erase(
      std::remove_if(tlvs.begin(), tlvs.end(),
                     [](const Tlv &t) { return t.type == SIGNATURE_TYPE; }),
      tlvs.end());

  // The key depends on the contents, except for the payer id we overwrite.
  Bytes material;
  for (const Tlv &t : tlvs) {
    if (t.type == PAYER_ID || IsSignatureType(t.type))
      continue;
    Bytes raw = EncodeTlv(t);
    material.insert(material.end(), raw.begin(), raw.end());
  }
  Hash key = TaggedStr("bitcoinfuzz/bolt12-invoice-request-key", material);

  secp256k1_keypair keypair;
  if (!secp256k1_keypair_create(Context(), &keypair, key.data()))
    return;
  secp256k1_xonly_pubkey xonly;
  if (!secp256k1_keypair_xonly_pub(Context(), &xonly, nullptr, &keypair))
    return;
  uint8_t x[32];
  secp256k1_xonly_pubkey_serialize(Context(), x, &xonly);
  Bytes payer_id{0x02};
  payer_id.insert(payer_id.end(), x, x + 32);

  SetTlv(tlvs, PAYER_ID, payer_id);
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
  // Offer and invoice-request fields, plus types outside the ranges a writer
  // may use, so that implementations disagreeing on them are found.
  static const uint64_t TYPES[] = {
      0,  2,   4,   6,    8,         10,         12,        16, 18,
      20, 22,  80,  82,   84,        86,         88,        89, 90,
      91, 240, 241, 1001, 999999999, 1000000001, 3999999999};
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

} // namespace bolt12_invoice_request_mutator

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  using namespace bolt12_invoice_request_mutator;
  std::mt19937 rng(seed);

  std::vector<Tlv> tlvs;
  std::string_view input(reinterpret_cast<char *>(fuzz_data), size);
  if (!DecodeToTlvs(input, tlvs)) {
    tlvs.clear();
    DecodeToTlvs(SEED_INVOICE_REQUEST, tlvs);
  }

  bool resign = rng() % 8 != 0;
  size_t rounds = 1 + rng() % 3;
  for (size_t i = 0; i < rounds; ++i)
    MutateOnce(tlvs, rng);
  if (resign)
    Resign(tlvs);
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

#endif // BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_INVOICE_REQUEST_H
