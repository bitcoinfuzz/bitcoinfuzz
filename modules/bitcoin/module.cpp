#include <optional>
#include <span>
#include <string>

#include "util/translation.h"
const TranslateFn G_TRANSLATION_FUN{nullptr};

#include "base58.h"
#include "blockencodings.h"
#include "chainparams.h"
#include "consensus/tx_check.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "descriptor.h"
#include "key.h"
#include "key_io.h"
#include "module.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "psbt.h"
#include "script/interpreter.h"
#include "script/miniscript.h"
#include "script/script.h"
#include "secp256k1.h"
#include "span.h"
#include "streams.h"
#include "util/bip32.h"
#include "util/chaintype.h"
#include "validation.h"

namespace {
class FuzzedSignatureChecker : public BaseSignatureChecker {
public:
  bool CheckECDSASignature(const std::vector<unsigned char> &scriptSig,
                           const std::vector<unsigned char> &vchPubKey,
                           const CScript &scriptCode,
                           SigVersion sigversion) const override {
    return true;
  }

  bool CheckSchnorrSignature(std::span<const unsigned char> sig,
                             std::span<const unsigned char> pubkey,
                             SigVersion sigversion,
                             ScriptExecutionData &execdata,
                             ScriptError *serror = nullptr) const override {
    return true;
  }

  bool CheckLockTime(const CScriptNum &nLockTime) const override {
    return true;
  }

  bool CheckSequence(const CScriptNum &nSequence) const override {
    return true;
  }

  virtual ~FuzzedSignatureChecker() = default;
};
} // namespace

namespace bitcoinfuzz {
namespace module {
Bitcoin::Bitcoin(void) : BaseModule("Bitcoin") {}

using Fragment = miniscript::Fragment;
using Node = miniscript::Node<CPubKey>;
using Type = miniscript::Type;
using MsCtx = miniscript::MiniscriptContext;
using miniscript::operator"" _mst;

//! Some pre-computed data for more efficient string roundtrips and to simulate
//! challenges.
struct TestData {
  typedef CPubKey Key;

  // Precomputed public keys, and a dummy signature for each of them.
  std::vector<Key> dummy_keys;
  std::map<Key, int> dummy_key_idx_map;
  std::map<CKeyID, Key> dummy_keys_map;
  std::map<Key, std::pair<std::vector<unsigned char>, bool>> dummy_sigs;
  std::map<XOnlyPubKey, std::pair<std::vector<unsigned char>, bool>>
      schnorr_sigs;

  // Precomputed hashes of each kind.
  std::vector<std::vector<unsigned char>> sha256;
  std::vector<std::vector<unsigned char>> ripemd160;
  std::vector<std::vector<unsigned char>> hash256;
  std::vector<std::vector<unsigned char>> hash160;
  std::map<std::vector<unsigned char>, std::vector<unsigned char>>
      sha256_preimages;
  std::map<std::vector<unsigned char>, std::vector<unsigned char>>
      ripemd160_preimages;
  std::map<std::vector<unsigned char>, std::vector<unsigned char>>
      hash256_preimages;
  std::map<std::vector<unsigned char>, std::vector<unsigned char>>
      hash160_preimages;

  //! Set the precomputed data.
  void Init() {
    unsigned char keydata[32] = {1};
    // All our signatures sign (and are required to sign) this constant message.
    constexpr uint256 MESSAGE_HASH{
        "0000000000000000f5cd94e18b6fe77dd7aca9e35c2b0c9cbd86356c80a71065"};
    // We don't pass additional randomness when creating a schnorr signature.
    const auto EMPTY_AUX{uint256::ZERO};

    for (size_t i = 0; i < 256; i++) {
      keydata[31] = i;
      CKey privkey;
      privkey.Set(keydata, keydata + 32, true);
      const Key pubkey = privkey.GetPubKey();

      dummy_keys.push_back(pubkey);
      dummy_key_idx_map.emplace(pubkey, i);
      dummy_keys_map.insert({pubkey.GetID(), pubkey});
      XOnlyPubKey xonly_pubkey{pubkey};
      dummy_key_idx_map.emplace(xonly_pubkey, i);
      uint160 xonly_hash{Hash160(xonly_pubkey)};
      dummy_keys_map.emplace(xonly_hash, pubkey);

      std::vector<unsigned char> sig, schnorr_sig(64);
      privkey.Sign(MESSAGE_HASH, sig);
      sig.push_back(1); // SIGHASH_ALL
      dummy_sigs.insert({pubkey, {sig, i & 1}});
      assert(
          privkey.SignSchnorr(MESSAGE_HASH, schnorr_sig, nullptr, EMPTY_AUX));
      schnorr_sig.push_back(1); // Maximally-sized signature has sighash byte
      schnorr_sigs.emplace(XOnlyPubKey{pubkey},
                           std::make_pair(std::move(schnorr_sig), i & 1));

      std::vector<unsigned char> hash;
      hash.resize(32);
      CSHA256().Write(keydata, 32).Finalize(hash.data());
      sha256.push_back(hash);
      if (i & 1)
        sha256_preimages[hash] =
            std::vector<unsigned char>(keydata, keydata + 32);
      CHash256().Write(keydata).Finalize(hash);
      hash256.push_back(hash);
      if (i & 1)
        hash256_preimages[hash] =
            std::vector<unsigned char>(keydata, keydata + 32);
      hash.resize(20);
      CRIPEMD160().Write(keydata, 32).Finalize(hash.data());
      assert(hash.size() == 20);
      ripemd160.push_back(hash);
      if (i & 1)
        ripemd160_preimages[hash] =
            std::vector<unsigned char>(keydata, keydata + 32);
      CHash160().Write(keydata).Finalize(hash);
      hash160.push_back(hash);
      if (i & 1)
        hash160_preimages[hash] =
            std::vector<unsigned char>(keydata, keydata + 32);
    }
  }

  //! Get the (Schnorr or ECDSA, depending on context) signature for this
  //! pubkey.
  const std::pair<std::vector<unsigned char>, bool> *
  GetSig(const MsCtx script_ctx, const Key &key) const {
    if (!miniscript::IsTapscript(script_ctx)) {
      const auto it = dummy_sigs.find(key);
      if (it == dummy_sigs.end())
        return nullptr;
      return &it->second;
    } else {
      const auto it = schnorr_sigs.find(XOnlyPubKey{key});
      if (it == schnorr_sigs.end())
        return nullptr;
      return &it->second;
    }
  }
} TEST_DATA;

/**
 * Context to parse a Miniscript node to and from Script or text representation.
 * Uses an integer (an index in the dummy keys array from the test data) as keys
 * in order to focus on fuzzing the Miniscript nodes' test representation, not
 * the key representation.
 */
struct ParserContext {
  typedef CPubKey Key;

  const MsCtx script_ctx;

  constexpr ParserContext(MsCtx ctx) noexcept : script_ctx(ctx) {}

  bool KeyCompare(const Key &a, const Key &b) const { return a < b; }

  std::optional<std::string> ToString(const Key &key) const {
    auto it = TEST_DATA.dummy_key_idx_map.find(key);
    if (it == TEST_DATA.dummy_key_idx_map.end())
      return {};
    uint8_t idx = it->second;
    return HexStr(std::span{&idx, 1});
  }

  std::vector<unsigned char> ToPKBytes(const Key &key) const {
    if (!miniscript::IsTapscript(script_ctx)) {
      return {key.begin(), key.end()};
    }
    const XOnlyPubKey xonly_pubkey{key};
    return {xonly_pubkey.begin(), xonly_pubkey.end()};
  }

  std::vector<unsigned char> ToPKHBytes(const Key &key) const {
    if (!miniscript::IsTapscript(script_ctx)) {
      const auto h = Hash160(key);
      return {h.begin(), h.end()};
    }
    const auto h = Hash160(XOnlyPubKey{key});
    return {h.begin(), h.end()};
  }

  std::optional<Key> FromString(std::span<const char> &in) const {
    if (in.size() != 1 && in.size() != 2)
      return {};
    std::string key_str(in.begin(), in.end());
    // Normalize symbolic one-digit key indexes (e.g. pk(8) -> pk(08))
    // so they match Core's pre-generated dummy key table representation.
    if (key_str.size() == 1) {
      key_str.insert(key_str.begin(), '0');
    }
    auto idx = ParseHex(key_str);
    if (idx.size() != 1)
      return {};
    return TEST_DATA.dummy_keys[idx[0]];
  }

  template <typename I> std::optional<Key> FromString(I first, I last) const {
    if (last - first != 1 && last - first != 2)
      return {};
    std::string key_str(first, last);
    if (key_str.size() == 1) {
      key_str.insert(key_str.begin(), '0');
    }
    auto idx = ParseHex(key_str);
    if (idx.size() != 1)
      return {};
    return TEST_DATA.dummy_keys[idx[0]];
  }

  template <typename I> std::optional<Key> FromPKBytes(I first, I last) const {
    if (!miniscript::IsTapscript(script_ctx)) {
      Key key{first, last};
      if (key.IsValid())
        return key;
      return {};
    }
    if (last - first != 32)
      return {};
    XOnlyPubKey xonly_pubkey;
    std::copy(first, last, xonly_pubkey.begin());
    return xonly_pubkey.GetEvenCorrespondingCPubKey();
  }

  template <typename I> std::optional<Key> FromPKHBytes(I first, I last) const {
    assert(last - first == 20);
    CKeyID keyid;
    std::copy(first, last, keyid.begin());
    const auto it = TEST_DATA.dummy_keys_map.find(keyid);
    if (it == TEST_DATA.dummy_keys_map.end())
      return {};
    return it->second;
  }

  MsCtx MsContext() const { return script_ctx; }
};

std::optional<std::string>
Bitcoin::script_parse(std::span<const uint8_t> buffer) const {
  DataStream ds{buffer};
  CScript script;
  try {
    ds >> script;
  } catch (const std::ios_base::failure &e) {
    return "0";
  }
  if (script.IsUnspendable())
    return "0";
  int version;
  std::vector<uint8_t> program;
  auto final_res{std::to_string(script.GetSigOpCount(false))};
  final_res += script.IsWitnessProgram(version, program) ? "1" : "0";
  final_res += script.IsPushOnly() ? "1" : "0";
  return final_res;
}

std::optional<bool> Bitcoin::script_eval(const std::vector<uint8_t> &input_data,
                                         unsigned int flags,
                                         size_t version) const {
  CScript script(input_data.begin(), input_data.end());
  if (script.empty())
    return std::nullopt;

  std::vector<std::vector<unsigned char>> stack;
  SigVersion sig_version =
      (version == 0) ? SigVersion::BASE : SigVersion::WITNESS_V0;

  return EvalScript(stack, script, 0, FuzzedSignatureChecker(), sig_version,
                    nullptr);
}

std::optional<bool>
Bitcoin::verify_script(const std::vector<uint8_t> &script_sig,
                       const std::vector<uint8_t> &script_pubkey) const {
  CScript ssig(script_sig.begin(), script_sig.end());
  if (ssig.empty())
    return std::nullopt;

  CScript spubkey(script_pubkey.begin(), script_pubkey.end());
  if (spubkey.empty())
    return std::nullopt;

  return VerifyScript(ssig, spubkey, nullptr, SCRIPT_VERIFY_NONE,
                      FuzzedSignatureChecker(), nullptr);
}

namespace {
void EnsureECCContextInitialized() {
  static bool ecc_context_initialized = false;
  static ECC_Context ecc_context{};
  if (!ecc_context_initialized) {
    SelectParams(ChainType::MAIN);
    ecc_context_initialized = true;
  }
}

void EnsureTestDataInitialized() {
  static bool test_data_initialized = false;
  if (!test_data_initialized) {
    TEST_DATA.Init();
    test_data_initialized = true;
  }
}
} // namespace

std::optional<bool> Bitcoin::descriptor_parse(std::string str) const {
  EnsureECCContextInitialized();

  FlatSigningProvider signing_provider;
  std::string error;
  const auto desc =
      Parse(str, signing_provider, error, /*require_checksum=*/false);
  return !desc.empty();
}

std::optional<bool> Bitcoin::miniscript_parse(std::string str) const {
  EnsureECCContextInitialized();

  EnsureTestDataInitialized();

  const ParserContext parser_ctx{miniscript::MiniscriptContext::P2WSH};
  auto ret{miniscript::FromString(str, parser_ctx)};
  if (ret && ret->IsSane()) {
    return true;
  }

  const ParserContext parser_ctx_tap{miniscript::MiniscriptContext::TAPSCRIPT};
  ret = miniscript::FromString(str, parser_ctx_tap);
  if (ret && ret->IsSane()) {
    return true;
  }

  return false;
}

std::optional<std::string>
Bitcoin::deserialize_block(std::span<const uint8_t> buffer) const {
  DataStream ds{buffer};
  CBlock block;
  try {
    ds >> TX_WITH_WITNESS(block);
  } catch (const std::ios_base::failure &) {
    return std::nullopt;
  }
  static bool initialized = false;
  if (!initialized) {
    SelectParams(ChainType::MAIN);
    initialized = true;
  }
  BlockValidationState state;
  if (!CheckBlock(block, state, Params().GetConsensus(), /*fCheckPOW=*/false)) {
    return "0";
  }
  if (IsBlockMutated(block, /*check_witness_root=*/true)) {
    return "0";
  }
  return block.GetHash().ToString();
}

std::optional<std::string>
Bitcoin::transaction_eval(std::span<const uint8_t> buffer) const {
  DataStream ds_mtx{buffer};
  CMutableTransaction mutable_tx;
  try {
    ds_mtx >> TX_WITH_WITNESS(mutable_tx);
  } catch (const std::ios_base::failure &e) {
    return "0";
  }

  CTransaction tx{mutable_tx};
  TxValidationState state;
  if (!CheckTransaction(tx, state))
    return "0";

  auto res{tx.GetWitnessHash().ToString()};
  res += std::to_string(tx.ComputeTotalSize());

  return res;
}

std::optional<std::string> Bitcoin::address_parse(std::string str) const {
  static bool initialized = false;
  if (!initialized) {
    SelectParams(ChainType::MAIN);
    initialized = true;
  }
  try {
    CTxDestination dest = DecodeDestination(str);
    if (IsValidDestination(dest)) {
      std::string result;

      if (std::holds_alternative<PKHash>(dest)) {
        result = "PKH:";
      } else if (std::holds_alternative<ScriptHash>(dest)) {
        result = "SH:";
      } else if (std::holds_alternative<WitnessV0KeyHash>(dest)) {
        result = "WPKH:";
      } else if (std::holds_alternative<WitnessV0ScriptHash>(dest)) {
        result = "WSH:";
      } else if (std::holds_alternative<WitnessV1Taproot>(dest)) {
        result = "TR:";
      } else {
        result = "UNK:";
      }

      return result + EncodeDestination(dest);
    }
    return "INVALID";
  } catch (const std::exception &) {
    return "INVALID";
  }
}

std::optional<std::string>
Bitcoin::addrv2_parse(std::span<const uint8_t> buffer) const {
  std::vector<CAddress> addrs;
  DataStream ds{buffer};
  try {
    ds >> CAddress::V2_NETWORK(addrs);
  } catch (const std::ios_base::failure &e) {
    return "[]";
  }

  std::string result = "[";
  bool first = true;
  for (const auto &addr : addrs) {
    if (!addr.IsRoutable()) {
      continue;
    }

    if (!first) {
      result += ",";
    }
    first = false;

    auto addr_bytes = addr.GetAddrBytes();
    std::string addr_hex;
    if (addr.IsIPv4() && addr_bytes.size() == 16) {
      // GetAddrBytes returns IPv4-mapped IPv6 format (16 bytes), extract last 4
      // bytes
      addr_hex = HexStr(std::span(addr_bytes).last(4));
    } else {
      addr_hex = HexStr(addr_bytes);
    }
    uint32_t time =
        static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                  addr.nTime.time_since_epoch())
                                  .count());
    uint64_t services = static_cast<uint64_t>(addr.nServices);
    uint16_t port = addr.GetPort();

    std::string addr_type;
    if (addr.IsIPv4()) {
      addr_type = "ipv4";
    } else if (addr.IsIPv6()) {
      addr_type = "ipv6";
    } else if (addr.IsTor()) {
      addr_type = "tor";
    } else if (addr.IsI2P()) {
      addr_type = "i2p";
    } else if (addr.IsCJDNS()) {
      addr_type = "cjdns";
    }

    result += "{\"addr\":\"" + addr_hex + "\",\"type\":\"" + addr_type +
              "\",\"time\":\"" + std::to_string(time) + "\",\"services\":\"" +
              std::to_string(services) + "\",\"port\":\"" +
              std::to_string(port) + "\"}";
  }
  result += "]";

  return result;
}

std::optional<std::string>
Bitcoin::psbt_parse(std::span<const uint8_t> buffer) const {
  if (buffer.empty()) {
    return std::nullopt;
  }

  PartiallySignedTransaction psbt;
  std::string error;

  // Attempt to decode raw psbt from buffer
  DataStream ds{buffer};
  try {
    ds >> psbt;
  } catch (const std::ios_base::failure &e) {
    return "";
  }

  std::string result;

  try {
    // Check if it's a valid transaction
    if (!psbt.tx) {
      return std::string{};
    }

    const CMutableTransaction &tx = *psbt.tx;

    // Extract high-level transaction properties (matching rust-bitcoin format)
    // result += "v=" + std::to_string(tx.version) + ";";
    result += "lt=" + std::to_string(tx.nLockTime) + ";";
    result += "in=" + std::to_string(tx.vin.size()) + ";";
    result += "out=" + std::to_string(tx.vout.size()) + ";";

    // Extract input information (matching rust-bitcoin format exactly)
    for (size_t i = 0; i < tx.vin.size(); i++) {
      if (i < psbt.inputs.size()) {
        const CTxIn &txin = tx.vin[i];
        const PSBTInput &psbt_input = psbt.inputs[i];

        // Previous output reference in format "txid:vout"
        result += "in" + std::to_string(i) +
                  "prev=" + txin.prevout.hash.ToString() + ":" +
                  std::to_string(txin.prevout.n) + ";";

        // Sequence number
        result += "in" + std::to_string(i) +
                  "seq=" + std::to_string(txin.nSequence) + ";";

        // UTXO availability (check both witness and non-witness UTXO)
        bool has_utxo = false;
        if (!psbt_input.witness_utxo.IsNull() || psbt_input.non_witness_utxo) {
          has_utxo = true;
        }
        if (has_utxo) {
          result += "in" + std::to_string(i) + "utxo=1;";
        }

        // Partial signatures count
        result += "in" + std::to_string(i) +
                  "sigs=" + std::to_string(psbt_input.partial_sigs.size()) +
                  ";";
      }
    }

    // Extract output information
    for (size_t i = 0; i < tx.vout.size(); i++) {
      if (i < psbt.outputs.size()) {
        const CTxOut &txout = tx.vout[i];

        // Output value (cast to int64_t to match rust-bitcoin's i64 cast)
        result += "out" + std::to_string(i) +
                  "val=" + std::to_string(static_cast<int64_t>(txout.nValue)) +
                  ";";

        // Output script as hex string
        result += "out" + std::to_string(i) +
                  "script=" + HexStr(txout.scriptPubKey) + ";";
      }
    }

  } catch (const std::exception &e) {
    return std::string{};
  }

  return result;
}

std::optional<int>
Bitcoin::cmpctblocks_parse(std::span<const uint8_t> buffer) const {
  DataStream ds{buffer};
  CBlockHeaderAndShortTxIDs block_header_and_short_txids;

  try {
    ds >> block_header_and_short_txids;
  } catch (const std::ios_base::failure &e) {
    if (std::string(e.what()).find("Superflous witness record") !=
        std::string::npos)
      return -2;
    return std::nullopt;
  }

  DataStream temp_ds{};
  try {
    temp_ds << block_header_and_short_txids;
    return static_cast<int>(temp_ds.size());
  } catch (const std::exception &e) {
    return std::nullopt;
  }
}

std::optional<std::string>
Bitcoin::bip32_master_keygen(std::span<const uint8_t> seed) const {
  SelectParams(ChainType::MAIN);
  CExtKey master;
  master.SetSeed(
      std::span{reinterpret_cast<const std::byte *>(seed.data()), seed.size()});
  return EncodeExtKey(master);
}

std::optional<std::string>
Bitcoin::bip32_deserialize_extended_key(std::span<const uint8_t> buffer) const {
  const std::string ext_str(reinterpret_cast<const char *>(buffer.data()),
                            buffer.size());

  if (ext_str[0] != 'x' && ext_str[0] != 't')
    return "INVALID";
  SelectParams(ext_str[0] == 't'
                   ? ChainType::TESTNET
                   : ChainType::MAIN); // needs to be done this way due to how
                                       // Params() works

  /* xprv / tprv */
  try {
    CExtKey ext_key = DecodeExtKey(ext_str);
    if (ext_key.key.size() ==
        0) { // if DecodeExtKey failed, it returns a CExtKey with empty key
      throw std::runtime_error("DecodeExtKey failed");
    }

    std::string result = strprintf(
        "depth=%02x;fp=%02x%02x%02x%02x;child=%08x;chaincode=%s;key=%s",
        ext_key.nDepth, ext_key.vchFingerprint[0], ext_key.vchFingerprint[1],
        ext_key.vchFingerprint[2], ext_key.vchFingerprint[3], ext_key.nChild,
        HexStr(ext_key.chaincode), HexStr(ext_key.key));
    return result;
  } catch (...) {
    /* fall through */
  }

  /* xpub / tpub */
  try {
    CExtPubKey ext_pubkey = DecodeExtPubKey(ext_str);
    if ((ext_pubkey.pubkey.size() == 0)) {
      throw std::runtime_error("DecodeExtPubKey failed");
    }

    std::string result = strprintf(
        "depth=%02x;fp=%02x%02x%02x%02x;child=%08x;chaincode=%s;key=%s",
        ext_pubkey.nDepth, ext_pubkey.vchFingerprint[0],
        ext_pubkey.vchFingerprint[1], ext_pubkey.vchFingerprint[2],
        ext_pubkey.vchFingerprint[3], ext_pubkey.nChild,
        HexStr(ext_pubkey.chaincode), HexStr(ext_pubkey.pubkey));
    return result;
  } catch (...) {
    return "INVALID";
  }
}

std::optional<std::string>
Bitcoin::bip32_derive_from_path(std::span<const uint8_t> buffer) const {
  std::string path_str(reinterpret_cast<const char *>(buffer.data()),
                       buffer.size());

  // Reject hardened notation ('h')
  if (path_str.find('h') != std::string::npos) {
    return std::nullopt;
  }

  // Parse derivation path
  std::vector<uint32_t> path;
  if (!ParseHDKeypath(path_str, path) || path.empty()) {
    return "INVALID";
  }

  static ECC_Context ecc_context;
  SelectParams(ChainType::MAIN);

  constexpr std::array<uint8_t, 32> seed_raw{
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

  CExtKey key;
  key.SetSeed(std::as_bytes(std::span(seed_raw)));

  for (uint32_t child : path) {
    CExtKey next;
    if (!key.Derive(next, child)) {
      return "INVALID";
    }
    key = next;
  }

  return EncodeExtKey(key);
}

} // namespace module
} // namespace bitcoinfuzz
