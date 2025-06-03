#include <optional>
#include <span>

#include "chainparams.h"
#include "consensus/validation.h"
#include "descriptor.h"
#include "module.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/miniscript.h"
#include "script/script.h"
#include "protocol.h"
#include "streams.h"
#include "util/chaintype.h"
#include "validation.h"
#include "core_io.h"
#include "key_io.h"
#include "psbt.h"
#include "span.h"

namespace {
class FuzzedSignatureChecker : public BaseSignatureChecker
{
public:
    bool CheckLockTime(const CScriptNum& nLockTime) const override
    {
        return true;
    }

    bool CheckSequence(const CScriptNum& nSequence) const override
    {
        return true;
    }

    virtual ~FuzzedSignatureChecker() = default;
};
} // namespace

namespace bitcoinfuzz {
namespace module {
Bitcoin::Bitcoin(void) : BaseModule("Bitcoin") {}

using Fragment = miniscript::Fragment;
using NodeRef = miniscript::NodeRef<CPubKey>;
using Node = miniscript::Node<CPubKey>;
using Type = miniscript::Type;
using MsCtx = miniscript::MiniscriptContext;
using miniscript::operator"" _mst;

//! Some pre-computed data for more efficient string roundtrips and to simulate challenges.
struct TestData {
    typedef CPubKey Key;

    // Precomputed public keys, and a dummy signature for each of them.
    std::vector<Key> dummy_keys;
    std::map<Key, int> dummy_key_idx_map;
    std::map<CKeyID, Key> dummy_keys_map;
    std::map<Key, std::pair<std::vector<unsigned char>, bool>> dummy_sigs;
    std::map<XOnlyPubKey, std::pair<std::vector<unsigned char>, bool>> schnorr_sigs;

    // Precomputed hashes of each kind.
    std::vector<std::vector<unsigned char>> sha256;
    std::vector<std::vector<unsigned char>> ripemd160;
    std::vector<std::vector<unsigned char>> hash256;
    std::vector<std::vector<unsigned char>> hash160;
    std::map<std::vector<unsigned char>, std::vector<unsigned char>> sha256_preimages;
    std::map<std::vector<unsigned char>, std::vector<unsigned char>> ripemd160_preimages;
    std::map<std::vector<unsigned char>, std::vector<unsigned char>> hash256_preimages;
    std::map<std::vector<unsigned char>, std::vector<unsigned char>> hash160_preimages;

    //! Set the precomputed data.
    void Init() {
        unsigned char keydata[32] = {1};
        // All our signatures sign (and are required to sign) this constant message.
        constexpr uint256 MESSAGE_HASH{"0000000000000000f5cd94e18b6fe77dd7aca9e35c2b0c9cbd86356c80a71065"};
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
            assert(privkey.SignSchnorr(MESSAGE_HASH, schnorr_sig, nullptr, EMPTY_AUX));
            schnorr_sig.push_back(1); // Maximally-sized signature has sighash byte
            schnorr_sigs.emplace(XOnlyPubKey{pubkey}, std::make_pair(std::move(schnorr_sig), i & 1));

            std::vector<unsigned char> hash;
            hash.resize(32);
            CSHA256().Write(keydata, 32).Finalize(hash.data());
            sha256.push_back(hash);
            if (i & 1) sha256_preimages[hash] = std::vector<unsigned char>(keydata, keydata + 32);
            CHash256().Write(keydata).Finalize(hash);
            hash256.push_back(hash);
            if (i & 1) hash256_preimages[hash] = std::vector<unsigned char>(keydata, keydata + 32);
            hash.resize(20);
            CRIPEMD160().Write(keydata, 32).Finalize(hash.data());
            assert(hash.size() == 20);
            ripemd160.push_back(hash);
            if (i & 1) ripemd160_preimages[hash] = std::vector<unsigned char>(keydata, keydata + 32);
            CHash160().Write(keydata).Finalize(hash);
            hash160.push_back(hash);
            if (i & 1) hash160_preimages[hash] = std::vector<unsigned char>(keydata, keydata + 32);
        }
    }

    //! Get the (Schnorr or ECDSA, depending on context) signature for this pubkey.
    const std::pair<std::vector<unsigned char>, bool>* GetSig(const MsCtx script_ctx, const Key& key) const {
        if (!miniscript::IsTapscript(script_ctx)) {
            const auto it = dummy_sigs.find(key);
            if (it == dummy_sigs.end()) return nullptr;
            return &it->second;
        } else {
            const auto it = schnorr_sigs.find(XOnlyPubKey{key});
            if (it == schnorr_sigs.end()) return nullptr;
            return &it->second;
        }
    }
} TEST_DATA;

/**
 * Context to parse a Miniscript node to and from Script or text representation.
 * Uses an integer (an index in the dummy keys array from the test data) as keys in order
 * to focus on fuzzing the Miniscript nodes' test representation, not the key representation.
 */
struct ParserContext {
    typedef CPubKey Key;

    const MsCtx script_ctx;

    constexpr ParserContext(MsCtx ctx) noexcept : script_ctx(ctx) {}

    bool KeyCompare(const Key& a, const Key& b) const {
        return a < b;
    }

    std::optional<std::string> ToString(const Key& key) const
    {
        auto it = TEST_DATA.dummy_key_idx_map.find(key);
        if (it == TEST_DATA.dummy_key_idx_map.end()) return {};
        uint8_t idx = it->second;
        return HexStr(std::span{&idx, 1});
    }

    std::vector<unsigned char> ToPKBytes(const Key& key) const {
        if (!miniscript::IsTapscript(script_ctx)) {
            return {key.begin(), key.end()};
        }
        const XOnlyPubKey xonly_pubkey{key};
        return {xonly_pubkey.begin(), xonly_pubkey.end()};
    }

    std::vector<unsigned char> ToPKHBytes(const Key& key) const {
        if (!miniscript::IsTapscript(script_ctx)) {
            const auto h = Hash160(key);
            return {h.begin(), h.end()};
        }
        const auto h = Hash160(XOnlyPubKey{key});
        return {h.begin(), h.end()};
    }

    template<typename I>
    std::optional<Key> FromString(I first, I last) const {
        if (last - first != 2) return {};
        auto idx = ParseHex(std::string(first, last));
        if (idx.size() != 1) return {};
        return TEST_DATA.dummy_keys[idx[0]];
    }

    template<typename I>
    std::optional<Key> FromPKBytes(I first, I last) const {
        if (!miniscript::IsTapscript(script_ctx)) {
            Key key{first, last};
            if (key.IsValid()) return key;
            return {};
        }
        if (last - first != 32) return {};
        XOnlyPubKey xonly_pubkey;
        std::copy(first, last, xonly_pubkey.begin());
        return xonly_pubkey.GetEvenCorrespondingCPubKey();
    }

    template<typename I>
    std::optional<Key> FromPKHBytes(I first, I last) const {
        assert(last - first == 20);
        CKeyID keyid;
        std::copy(first, last, keyid.begin());
        const auto it = TEST_DATA.dummy_keys_map.find(keyid);
        if (it == TEST_DATA.dummy_keys_map.end()) return {};
        return it->second;
    }

    MsCtx MsContext() const {
        return script_ctx;
    }
};

std::optional<std::string> Bitcoin::script_parse(std::span<const uint8_t> buffer) const
{
    DataStream ds{buffer};
    CScript script;
    try {
        ds >> script;
    } catch (const std::ios_base::failure& e) {
        return "0";
    }
    if (script.IsUnspendable()) return "0";
    int version;
    std::vector<uint8_t> program;
    auto final_res{std::to_string(script.GetSigOpCount(false))};
    final_res += script.IsWitnessProgram(version, program) ? "1" : "0";
    final_res += script.IsPushOnly() ? "1" : "0";
    return final_res;
}

std::optional<bool> Bitcoin::script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const
{
    CScript script(input_data.begin(), input_data.end());
    if (script.empty()) return std::nullopt;

    // Skip sequence and locktime
    auto hex_str{HexStr(script)};
    if (hex_str.find("b1") != std::string::npos || hex_str.find("b2") != std::string::npos) return std::nullopt;

    std::vector<std::vector<unsigned char>> stack;
    SigVersion sig_version = (version == 0) ? SigVersion::BASE : SigVersion::WITNESS_V0;

    return EvalScript(stack, script, 0, FuzzedSignatureChecker(), sig_version, nullptr);
}

std::optional<bool> Bitcoin::descriptor_parse(std::string str) const
{
    // TODO: Move it to a constructor
    static ECC_Context ecc_context{};
    SelectParams(ChainType::MAIN);

    FlatSigningProvider signing_provider;
    std::string error;
    const auto desc = Parse(str, signing_provider, error, /*require_checksum=*/false);
    return !desc.empty();
}

std::optional<bool> Bitcoin::miniscript_parse(std::string str) const
{
    // TODO: Move it to a constructor
    static ECC_Context ecc_context{};
    static bool initialized = false;
    if (!initialized)
    {
        SelectParams(ChainType::MAIN);
        TEST_DATA.Init();
        initialized = true;
    }

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

std::optional<std::vector<bool>> Bitcoin::deserialize_block(std::span<const uint8_t> buffer) const
{
    DataStream ds{buffer};
    CBlock block;
    std::vector<bool> res{};
    try {
        ds >> TX_WITH_WITNESS(block);
    } catch (const std::ios_base::failure&) {
        return res;
    }
    if (block.vtx.empty()) {
        res.push_back(false);
    } else {
        res.push_back(!IsBlockMutated(block, true));
    }
    return res;
}

std::optional<std::string> Bitcoin::script_asm(std::span<const uint8_t> buffer) const
{
    CScript script(buffer.begin(), buffer.end());
    if (script.empty() || !script.HasValidOps()) return std::nullopt;
    auto asm_str = ScriptToAsmStr(script);
    if (asm_str.find("[error]") != std::string::npos) return std::nullopt;
    return asm_str;
}

std::optional<std::string> Bitcoin::address_parse(std::string str) const
{
    static bool initialized = false;
    if (!initialized)
    {
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
    } catch (const std::exception&) {
        return "INVALID";
    }
}

std::optional<std::string> Bitcoin::addrv2_parse(std::span<const uint8_t> buffer) const
{
    std::vector<CAddress> addrs;
    DataStream ds{buffer};
    uint64_t clearnet{0}, tor{0}, i2p{0}, cjdns{0};
    try {
        ds >> CAddress::V2_NETWORK(addrs);
        if (addrs.size() > 1000) return "clearnet=0tor=0cjdns=0i2p=0";
        for (auto& addr : addrs) {
            if (addr.IsIPv4() || addr.IsIPv6()) clearnet++;
            if (addr.IsTor()) tor++;
            if (addr.IsI2P()) i2p++;
            if (addr.IsCJDNS()) cjdns++;
            if (!addr.IsRoutable()) return "clearnet=0tor=0cjdns=0i2p=0";
        }
    } catch (const std::ios_base::failure& e) {
        // TODO: remove workaround to make it compatible with Core
        return std::nullopt;
        //return "clearnet=0tor=0cjdns=0i2p=0";
    }

    return "clearnet=" + std::to_string(clearnet) + "tor=" + std::to_string(tor) +
           "cjdns=" + std::to_string(cjdns) + "i2p=" + std::to_string(i2p);
}

std::optional<std::string> Bitcoin::psbt_parse(std::span<const uint8_t> buffer) const {

    if (buffer.empty()) {
        return std::nullopt;
    }

    PartiallySignedTransaction psbt;
    std::string error;

    // Attempt to decode raw psbt from buffer
    if (!DecodeRawPSBT(psbt, MakeByteSpan(buffer), error)) {
        return std::string{};
    }

    std::string result;

    try {
        // Check if it's a valid transaction
        if (!psbt.tx) {
            return std::string{};
        }

        const CMutableTransaction& tx = *psbt.tx;

        // Extract high-level transaction properties (matching rust-bitcoin format)
        //result += "v=" + std::to_string(tx.version) + ";";
        result += "lt=" + std::to_string(tx.nLockTime) + ";";
        result += "in=" + std::to_string(tx.vin.size()) + ";";
        result += "out=" + std::to_string(tx.vout.size()) + ";";

        // Extract input information (matching rust-bitcoin format exactly)
        for (size_t i = 0; i < tx.vin.size(); i++) {
            if (i < psbt.inputs.size()) {
                const CTxIn& txin = tx.vin[i];
                const PSBTInput& psbt_input = psbt.inputs[i];

                // Previous output reference in format "txid:vout"
                result += "in" + std::to_string(i) + "prev=" +
                         txin.prevout.hash.ToString() + ":" +
                         std::to_string(txin.prevout.n) + ";";

                // Sequence number
                result += "in" + std::to_string(i) + "seq=" +
                         std::to_string(txin.nSequence) + ";";

                // UTXO availability (check both witness and non-witness UTXO)
                bool has_utxo = false;
                if (!psbt_input.witness_utxo.IsNull() || psbt_input.non_witness_utxo) {
                    has_utxo = true;
                }
                if (has_utxo) {
                    result += "in" + std::to_string(i) + "utxo=1;";
                }

                // Partial signatures count
                result += "in" + std::to_string(i) + "sigs=" +
                         std::to_string(psbt_input.partial_sigs.size()) + ";";
            }
        }

        // Extract output information
        for (size_t i = 0; i < tx.vout.size(); i++) {
            if (i < psbt.outputs.size()) {
                const CTxOut& txout = tx.vout[i];

                // Output value (cast to int64_t to match rust-bitcoin's i64 cast)
                result += "out" + std::to_string(i) + "val=" +
                         std::to_string(static_cast<int64_t>(txout.nValue)) + ";";

                // Output script as hex string
                result += "out" + std::to_string(i) + "script=" +
                         HexStr(txout.scriptPubKey) + ";";
            }
        }

    } catch (const std::exception& e) {
        return std::string{};
    }

    return result;
}

} // namespace module
} // namespace bitcoinfuzz
