#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "module.h"
#include <bitcoin/system.hpp>
#include <bitcoin/system/chain/enums/magic_numbers.hpp>
#include <bitcoin/system/machine/interpreter.hpp>

using namespace libbitcoin::system;

namespace {
class mock_signature_checker
    : public libbitcoin::system::machine::script_checker {
public:
  bool override_ecdsa_checksig_verify() const NOEXCEPT override { return true; }

  bool verify_schnorr_signature(
      const data_chunk &x_point, const hash_digest &hash,
      const ec_signature &signature) const NOEXCEPT override {
    return true;
  }

  error::op_error_t
  verify_locktime(bool input_final, uint64_t stack_locktime,
                  uint32_t transaction_locktime) const NOEXCEPT override {
    return error::op_success;
  }

  error::op_error_t verify_sequence(uint32_t transaction_version,
                                    uint32_t stack_sequence,
                                    uint32_t input_sequence) const NOEXCEPT {
    return error::op_success;
  }

  ~mock_signature_checker() override = default;
};
} // namespace

namespace bitcoinfuzz {
namespace module {

LibbitcoinSystem::LibbitcoinSystem(void) : BaseModule("LibbitcoinSystem") {}

std::optional<std::string>
LibbitcoinSystem::script_parse(std::span<const uint8_t> buffer) const {
  data_chunk data(buffer.begin(), buffer.end());

  // Prefix set to true treats the first byte as lenght
  // to match core's CScript deserialization
  chain::script script{data, true};

  if (!script.is_valid())
    return "0";

  // Match bitcoin core's IsUnspendable(): OP_RETURN prefix or oversize
  // script. Libbitcoin's script::is_unspendable() is broader:
  // it treats parse-underflow/invalid leading operations as unspendable,
  // while Core only checks OP_RETURN prefix and MAX_SCRIPT_SIZE here.
  const auto &ops = script.ops();
  if ((!ops.empty() && ops.front() == chain::opcode::op_return) ||
      script.is_oversized())
    return "0";

  std::string result = std::to_string(script.signature_operations(false));

  bool is_witness = chain::script::is_witness_program_pattern(ops);
  result += is_witness ? "1" : "0";
  result += chain::script::is_relaxed_push_pattern(ops) ? "1" : "0";

  return result;
}

std::optional<std::string>
LibbitcoinSystem::transaction_eval(std::span<const uint8_t> buffer) const {
  data_chunk data(buffer.begin(), buffer.end());
  chain::transaction tx{data, true};
  static constexpr uint64_t max_money =
      21'000'000ULL * chain::satoshi_per_bitcoin;
  uint64_t total = 0;
  // is_valid() only signals deserialization succeeded — it accepts
  // 0-input/0-output and other consensus-invalid forms that bitcoin core's
  // CheckTransaction rejects. Use check() (the libbitcoin-system equivalent)
  // to align acceptance with the reference modules. However, check() only
  // verifies the transaction's internal consistency, not whether its inputs are
  // valid or unspent, so we also check for distinct inputs to match bitcoin
  // core's validation.
  if (!tx.is_valid() || tx.check()) {
    return "0";
  }

  chain::points points;
  const auto inputs_ptr = tx.inputs_ptr();
  for (const auto &input_ptr : *inputs_ptr) {
    points.push_back(input_ptr->point());
  }
  // Check for double spend, CVE-2018-17144
  if (!is_distinct(points))
    return "0";

  // Check for spend overflow, CVE-2010-5139.
  const auto outputs_ptr = tx.outputs_ptr();
  for (const auto &output : *outputs_ptr) {
    const uint64_t value = output->value();
    if (value > max_money)
      return "0";
    // Avoid overflow in the total sum
    if (total > max_money - value)
      return "0";
    total += value;
  }

  const std::string hash = encode_hash(tx.hash(true));
  const size_t size = tx.serialized_size(true);

  return hash + std::to_string(size);
}

std::optional<std::string>
LibbitcoinSystem::deserialize_block(std::span<const uint8_t> buffer) const {
  data_chunk data(buffer.begin(), buffer.end());
  chain::block blk{data, true};

  // Same rationale as transaction_eval: is_valid() only checks
  // deserialization. check() applies the consensus-level checks that match
  // bitcoin core's CheckBlock, so we don't return a hash for blocks the
  // reference rejects.
  if (!blk.is_valid())
    return std::nullopt;
  if (blk.check())
    return "0";
  return encode_hash(blk.hash());
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

  data_chunk seed(buffer.begin(), buffer.end());
  wallet::hd_private master(seed, wallet::hd_private::mainnet);
  if (!master) {
    return "INVALID";
  }

  return master.encoded();
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
      // Equivalent to Core's CExtKey::Decode master-key lineage check.
      const auto &lineage = priv.lineage();
      if (lineage.depth == 0 &&
          (lineage.parent_fingerprint != 0 || lineage.child_number != 0)) {
        return "INVALID";
      }
      const auto &secret = priv.secret();
      if (!verify(secret))
        return "INVALID";
      data_chunk key_bytes(secret.begin(), secret.end());
      return format_key(lineage, priv.chain_code(), key_bytes);
    }

    const uint32_t pub_prefix =
        is_testnet ? wallet::hd_public::testnet : wallet::hd_public::mainnet;
    wallet::hd_public pub(ext_str, pub_prefix);
    if (pub) {
      // Equivalent to Core's CExtPubKey::Decode master-key lineage check.
      const auto &lineage = pub.lineage();
      if (lineage.depth == 0 &&
          (lineage.parent_fingerprint != 0 || lineage.child_number != 0)) {
        return "INVALID";
      }
      const auto &point = pub.point();
      if (!is_compressed_key(point) || !verify(data_slice(point)))
        return "INVALID";
      data_chunk key_bytes(point.begin(), point.end());
      return format_key(lineage, pub.chain_code(), key_bytes);
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
  if (input_data.empty()) {
    return std::nullopt;
  }
  data_chunk script_bytes(input_data.begin(), input_data.end());

  const chain::script in_script{script_bytes, false};
  if (!in_script.is_valid()) {
    return false;
  }

  // Uses an 0P_1 scripPubkey to alway push truthy, so that script_eval tests
  // whether script executes without error, as oposed to whether it leaves
  // a truth value on stack, which is tested by ggipt target.
  // Concrete divergence: scriptSig=OP_0 → bitcoin core false,
  // libbitcoin (with OP_1) returns true.
  const data_chunk out_bytes{0x51}; // OP_1
  const chain::script out_script{out_bytes, false};

  const chain::transaction tx{
      1u, chain::inputs{{chain::point{}, in_script, chain::max_input_sequence}},
      chain::outputs{}, 0u};

  const auto inputs_ptr = tx.inputs_ptr();
  inputs_ptr->front()->prevout = to_shared(chain::output{0u, out_script});

  const chain::context ctx{chain::flags::no_rules, 0u, 0u, 0u, 0u, 0u};

  using interp = machine::interpreter<machine::contiguous_stack>;
  const auto ec = interp::connect(ctx, tx, 0u, mock_signature_checker());
  return !ec;
}

std::optional<bool> LibbitcoinSystem::verify_script(
    const std::vector<uint8_t> &script_sig,
    const std::vector<uint8_t> &script_pubkey) const {
  if (script_sig.empty() || script_pubkey.empty()) {
    return std::nullopt;
  }

  const chain::script in_script{
      data_chunk(script_sig.begin(), script_sig.end()), false};
  const chain::script out_script{
      data_chunk(script_pubkey.begin(), script_pubkey.end()), false};

  if (!in_script.is_valid() || !out_script.is_valid()) {
    return false;
  }

  const chain::transaction tx{
      1u, chain::inputs{{chain::point{}, in_script, chain::max_input_sequence}},
      chain::outputs{}, 0u};

  const auto inputs_ptr = tx.inputs_ptr();
  inputs_ptr->front()->prevout = to_shared(chain::output{0u, out_script});

  const chain::context ctx{chain::flags::no_rules, 0u, 0u, 0u, 0u, 0u};

  using interp = machine::interpreter<machine::contiguous_stack>;
  const auto ec = interp::connect(ctx, tx, 0u, mock_signature_checker());
  return !ec;
}

} // namespace module
} // namespace bitcoinfuzz
