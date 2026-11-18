// Tests for the BOLT12 invoice mutator: its merkle root against the BOLT12
// test vectors, and that its outputs decode and are mostly validly signed.
#include "../mutators/bolt12_invoice.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

// The mutator calls libFuzzer to mutate TLV values; leave them unchanged.
extern "C" size_t LLVMFuzzerMutate(uint8_t *, size_t size, size_t) {
  return size;
}

namespace {
using namespace bolt12_invoice_mutator;

Bytes FromHex(std::string_view hex) {
  assert(hex.size() % 2 == 0);
  Bytes out;
  for (size_t i = 0; i < hex.size(); i += 2)
    out.push_back(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16));
  return out;
}

Hash HashFromHex(std::string_view hex) {
  Bytes b = FromHex(hex);
  assert(b.size() == 32);
  Hash h;
  std::copy(b.begin(), b.end(), h.begin());
  return h;
}

std::vector<Tlv> TlvsFromHex(std::initializer_list<std::string_view> tlvs) {
  Bytes stream;
  for (std::string_view tlv : tlvs) {
    Bytes b = FromHex(tlv);
    stream.insert(stream.end(), b.begin(), b.end());
  }
  std::vector<Tlv> out;
  assert(ParseTlvs(stream, out));
  return out;
}

// bolts/bolt12/signature-test.json at bolts commit 0ce484d.
void TestMerkleVectors() {
  assert(MerkleRoot(TlvsFromHex({"010203e8"})) ==
         HashFromHex("b013756c8fee86503a0b4abdab4cddeb"
                     "1af5d344ca6fc2fa8b6c08938caa6f93"));
  assert(MerkleRoot(TlvsFromHex({"010203e8", "02080000010000020003"})) ==
         HashFromHex("c3774abbf4815aa54ccaa026bff6581f"
                     "01f3be5fe814c620a252534f434bc0d1"));
  assert(MerkleRoot(TlvsFromHex(
             {"010203e8", "02080000010000020003",
              "03310266e4598d1d3c415f572a8488830b60f7e744ed9235eb0b1ba93283b315"
              "c0351800000000000000010000000000000002"})) ==
         HashFromHex("ab2e79b1283b0b31e0b035258de23782"
                     "df6b89a38cfa7237bde69aed1a658c5d"));

  bech32::DecodeResult dec = bech32::DecodeNoChecksum(
      "lnr1qqyqqqqqqqqqqqqqqcp4256ypqqkgzshgysy6ct5dpjk6ct5d93kzmpq23ex2ct5d9ek"
      "293pqthvwfzadd7jejes8q9lhc4rvjxd022zv5l44g6qah82ru5rdpnpjkppqvjx204vgdz"
      "gsqpvcp4mldl3plscny0rt707gvpdh6ndydfacz43euzqhrurageg3n7kafgsek6gz3e9w5"
      "2parv8gs2hlxzk95tzeswywffxlkeyhml0hh46kndmwf4m6xma3tkq2lu04qz3slje2rfth"
      "c89vss",
      bech32::CharLimit::CUSTOM_MUTATOR);
  std::vector<Tlv> tlvs;
  assert(dec.hrp == "lnr" && ParseTlvs(FiveToEight(dec.data), tlvs));
  std::erase_if(tlvs, [](const Tlv &t) { return IsSignatureType(t.type); });
  Hash root = MerkleRoot(tlvs);
  assert(root == HashFromHex("608407c18ad9a94d9ea2bcdbe170b6c2"
                             "0c462a7833a197621c916f78cf18e624"));
  assert(TaggedStr("lightninginvoice_requestsignature",
                   Bytes(root.begin(), root.end())) ==
         HashFromHex("aefe3aa88a69772c246dcaef75ed3e75"
                     "66c08ecc4e9f995233526a5651fc34cd"));
}

const Tlv *Find(const std::vector<Tlv> &tlvs, uint64_t type) {
  for (const Tlv &t : tlvs)
    if (t.type == type)
      return &t;
  return nullptr;
}

bool SignatureVerifies(const std::vector<Tlv> &tlvs) {
  const Tlv *sig = Find(tlvs, SIGNATURE_TYPE);
  const Tlv *node_id = Find(tlvs, INVOICE_NODE_ID);
  if (!sig || sig->value.size() != 64 || !node_id ||
      node_id->value.size() != 33)
    return false;
  std::vector<Tlv> signed_part;
  for (const Tlv &t : tlvs)
    if (!IsSignatureType(t.type))
      signed_part.push_back(t);
  Hash root = MerkleRoot(signed_part);
  Hash msg = TaggedStr(SIGNATURE_TAG, Bytes(root.begin(), root.end()));
  secp256k1_xonly_pubkey key;
  return secp256k1_xonly_pubkey_parse(Context(), &key,
                                      node_id->value.data() + 1) &&
         secp256k1_schnorrsig_verify(Context(), sig->value.data(), msg.data(),
                                     msg.size(), &key);
}

// Outputs must decode as lni strings, and most must be re-signed, or the
// target only compares rejections. An empty TLV stream ("lni1") used to abort.
void TestMutatedOutputs() {
  const std::string inputs[] = {"", "lni1", "not bech32", SEED_INVOICE};
  size_t total = 0, verified = 0;
  for (const std::string &input : inputs) {
    for (unsigned seed = 0; seed < 256; ++seed) {
      std::vector<uint8_t> buf(8192);
      std::memcpy(buf.data(), input.data(), input.size());
      size_t n =
          LLVMFuzzerCustomMutator(buf.data(), input.size(), buf.size(), seed);
      std::vector<Tlv> tlvs;
      assert(DecodeToTlvs(
          std::string_view(reinterpret_cast<char *>(buf.data()), n), tlvs));
      ++total;
      verified += SignatureVerifies(tlvs);
    }
  }
  std::printf("%zu of %zu mutated invoices are validly signed\n", verified,
              total);
  assert(verified * 4 > total * 3);
}
} // namespace

int main() {
  TestMerkleVectors();
  TestMutatedOutputs();
  std::printf("bolt12_invoice mutator tests passed\n");
}
