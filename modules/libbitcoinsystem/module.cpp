#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include <bitcoin/system.hpp>
#include <bitcoin/system/machine/interpreter.hpp>

#include "module.h"

using namespace libbitcoin::system;

namespace bitcoinfuzz {
namespace module {

LibbitcoinSystem::LibbitcoinSystem(void) : BaseModule("LibbitcoinSystem") {}

std::optional<std::string>
LibbitcoinSystem::script_parse(std::span<const uint8_t> buffer) const {
  try {
    data_chunk data(buffer.begin(), buffer.end());

    // Match bitcoin core's IsUnspendable(): OP_RETURN prefix or oversize
    // script. Without these, libbitcoin returns "0...0" sigops/witness/pushonly
    // for OP_RETURN scripts while bitcoin core returns "0", causing asserts.
    if (!data.empty() && data[0] == 0x6a) {
      return "0";
    }
    if (data.size() > 10000) {
      return "0";
    }

    chain::script script{data, false};

    if (!script.is_valid()) {
      return "0";
    }

    const auto &ops = script.ops();
    std::string result = std::to_string(script.signature_operations(false));

    bool is_witness = chain::script::is_pay_witness_key_hash_pattern(ops) ||
                      chain::script::is_pay_witness_script_hash_pattern(ops) ||
                      chain::script::is_pay_witness_taproot_pattern(ops);
    result += is_witness ? "1" : "0";
    result += chain::script::is_push_only_pattern(ops) ? "1" : "0";

    return result;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string>
LibbitcoinSystem::transaction_eval(std::span<const uint8_t> buffer) const {
  try {
    data_chunk data(buffer.begin(), buffer.end());
    chain::transaction tx{data, true};

    // is_valid() only signals deserialization succeeded — it accepts
    // 0-input/0-output and other consensus-invalid forms that bitcoin core's
    // CheckTransaction rejects. Use check() (the libbitcoin-system equivalent)
    // to align acceptance with the reference modules.
    if (!tx.is_valid() || tx.check()) {
      return "0";
    }

    const auto inputs_ptr = tx.inputs_ptr();
    if (!inputs_ptr) {
      return std::nullopt;
    }
    for (const auto &input_ptr : *inputs_ptr) {
      if (!input_ptr) {
        return std::nullopt;
      }
      if (!input_ptr->witness().stack().empty()) {
        return std::nullopt;
      }
    }

    const std::string hash = encode_hash(tx.hash(false));
    const size_t size = tx.serialized_size(false);

    return hash + std::to_string(size);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string>
LibbitcoinSystem::deserialize_block(std::span<const uint8_t> buffer) const {
  try {
    data_chunk data(buffer.begin(), buffer.end());
    chain::block blk{data, true};

    // Same rationale as transaction_eval: is_valid() only checks
    // deserialization. check() applies the consensus-level checks that match
    // bitcoin core's CheckBlock, so we don't return a hash for blocks the
    // reference rejects.
    if (!blk.is_valid() || blk.check()) {
      return std::nullopt;
    }

    return encode_hash(blk.hash());
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string>
LibbitcoinSystem::address_parse(std::string str) const {
  try {
    wallet::payment_address addr(str);

    if (!addr) {
      // libbitcoin-system's payment_address only handles legacy P2PKH/P2SH.
      // For bech32/segwit addresses (P2WPKH, P2WSH, P2TR) it returns an
      // invalid addr while other modules return "WPKH:", "WSH:", "TR:".
      // Abstain (nullopt) instead of falsely reporting "INVALID".
      return std::nullopt;
    }

    std::string prefix;
    if (addr.prefix() == wallet::payment_address::mainnet_p2kh) {
      prefix = "PKH:";
    } else if (addr.prefix() == wallet::payment_address::mainnet_p2sh) {
      prefix = "SH:";
    } else {
      prefix = "UNK:";
    }

    return prefix + addr.encoded();
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string>
LibbitcoinSystem::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  try {
    data_chunk seed(buffer.begin(), buffer.end());

    if (seed.size() < 16 || seed.size() > 64) {
      return std::nullopt;
    }

    wallet::hd_private master(seed, wallet::hd_private::mainnet);
    if (!master) {
      return std::nullopt;
    }

    return master.encoded();
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string> LibbitcoinSystem::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  try {
    if (buffer.empty()) {
      return "INVALID";
    }
    const std::string ext_str(reinterpret_cast<const char *>(buffer.data()),
                              buffer.size());

    const bool is_testnet = ext_str[0] == 't';

    auto format_key = [](const wallet::hd_lineage &lineage,
                         const wallet::hd_chain_code &chain,
                         const data_chunk &key_bytes) -> std::string {
      std::ostringstream ss;
      ss << std::hex << std::setfill('0');

      ss << "depth=" << std::setw(2) << static_cast<int>(lineage.depth) << ";";
      ss << "fp=";
      ss << std::setw(2) << ((lineage.parent_fingerprint >> 24) & 0xFFu)
         << std::setw(2) << ((lineage.parent_fingerprint >> 16) & 0xFFu)
         << std::setw(2) << ((lineage.parent_fingerprint >> 8) & 0xFFu)
         << std::setw(2) << (lineage.parent_fingerprint & 0xFFu) << ";";
      ss << "child=" << std::setw(8) << lineage.child_number << ";";
      ss << "chaincode=";
      for (const auto byte : chain) {
        ss << std::setw(2) << static_cast<int>(byte);
      }
      ss << ";";
      ss << "key=";
      for (const auto byte : key_bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
      }

      return ss.str();
    };

    const uint64_t priv_prefixes =
        is_testnet ? wallet::hd_private::testnet : wallet::hd_private::mainnet;
    wallet::hd_private priv(ext_str, priv_prefixes);
    if (priv) {
      const auto &secret = priv.secret();
      data_chunk key_bytes(secret.begin(), secret.end());
      return format_key(priv.lineage(), priv.chain_code(), key_bytes);
    }

    const uint32_t pub_prefix =
        is_testnet ? wallet::hd_public::testnet : wallet::hd_public::mainnet;
    wallet::hd_public pub(ext_str, pub_prefix);
    if (pub) {
      const auto &point = pub.point();
      data_chunk key_bytes(point.begin(), point.end());
      return format_key(pub.lineage(), pub.chain_code(), key_bytes);
    }

    return "INVALID";
  } catch (...) {
    return "INVALID";
  }
}

std::optional<bool>
LibbitcoinSystem::script_eval(const std::vector<uint8_t> &input_data,
                              unsigned int /*flags*/,
                              size_t /*version*/) const {
  try {
    if (input_data.empty()) {
      return std::nullopt;
    }
    data_chunk script_bytes(input_data.begin(), input_data.end());

    const chain::script in_script{script_bytes, false};
    if (!in_script.is_valid()) {
      return std::nullopt;
    }

    // Use an empty scriptPubKey so connect() checks whether scriptSig leaves
    // a truthy value on the stack — matching bitcoin core's EvalScript
    // semantics. An OP_1 scriptPubKey would always push truthy last, turning
    // the result into "did scriptSig execute without error?" instead of "did
    // it succeed?". Concrete divergence: scriptSig=OP_0 → bitcoin core false,
    // libbitcoin (with OP_1) returns true.
    const data_chunk empty_bytes{};
    const chain::script out_script{empty_bytes, false};

    const chain::transaction tx{
        1u,
        chain::inputs{{chain::point{}, in_script, chain::max_input_sequence}},
        chain::outputs{}, 0u};

    const auto inputs_ptr = tx.inputs_ptr();
    if (!inputs_ptr || inputs_ptr->empty() || !inputs_ptr->front()) {
      return std::nullopt;
    }
    inputs_ptr->front()->prevout = to_shared(chain::output{0u, out_script});

    const chain::context ctx{chain::flags::no_rules, 0u, 0u, 0u, 0u, 0u};

    using interp = machine::interpreter<machine::contiguous_stack>;
    const auto ec = interp::connect(ctx, tx, 0u);
    return !ec;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<bool> LibbitcoinSystem::verify_script(
    const std::vector<uint8_t> &script_sig,
    const std::vector<uint8_t> &script_pubkey) const {
  try {
    if (script_sig.empty() || script_pubkey.empty()) {
      return std::nullopt;
    }

    const chain::script in_script{
        data_chunk(script_sig.begin(), script_sig.end()), false};
    const chain::script out_script{
        data_chunk(script_pubkey.begin(), script_pubkey.end()), false};

    if (!in_script.is_valid() || !out_script.is_valid()) {
      return std::nullopt;
    }

    const chain::transaction tx{
        1u,
        chain::inputs{{chain::point{}, in_script, chain::max_input_sequence}},
        chain::outputs{}, 0u};

    const auto inputs_ptr = tx.inputs_ptr();
    if (!inputs_ptr || inputs_ptr->empty() || !inputs_ptr->front()) {
      return std::nullopt;
    }
    inputs_ptr->front()->prevout = to_shared(chain::output{0u, out_script});

    const chain::context ctx{chain::flags::no_rules, 0u, 0u, 0u, 0u, 0u};

    using interp = machine::interpreter<machine::contiguous_stack>;
    const auto ec = interp::connect(ctx, tx, 0u);
    return !ec;
  } catch (...) {
    return std::nullopt;
  }
}

} // namespace module
} // namespace bitcoinfuzz
