#include "module.h"
#include "bitcoinkernel_variant_symbol_prefix.h"

#include <bitcoinfuzz/result.h>
#include <kernel/bitcoinkernel_wrapper.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ranges>
#include <span>
#include <sstream>
#include <string>

namespace {
std::string bytes_to_hex(std::span<const std::byte> bytes) {
  std::stringstream string_stream;
  string_stream << std::hex;
  for (const auto byte : bytes) {
    string_stream << std::setw(2) << std::setfill('0')
                  << std::to_integer<int>(byte);
  }

  return string_stream.str();
}

std::string bytes_to_hex_reversed(std::span<const std::byte> bytes) {
  std::stringstream string_stream;
  string_stream << std::hex;
  for (const auto byte : bytes | std::views::reverse) {
    string_stream << std::setw(2) << std::setfill('0')
                  << std::to_integer<int>(byte);
  }

  return string_stream.str();
}

btck::ChainType decode_chain_type(uint8_t value) {
  constexpr std::array chain_types{
      btck::ChainType::MAINNET, btck::ChainType::TESTNET,
      btck::ChainType::TESTNET_4, btck::ChainType::SIGNET,
      btck::ChainType::REGTEST};
  return chain_types[value % chain_types.size()];
}

std::string chain_type_to_string(btck::ChainType chain_type) {
  switch (chain_type) {
  case btck::ChainType::MAINNET:
    return "mainnet";
  case btck::ChainType::TESTNET:
    return "testnet";
  case btck::ChainType::TESTNET_4:
    return "testnet4";
  case btck::ChainType::SIGNET:
    return "signet";
  case btck::ChainType::REGTEST:
    return "regtest";
  }

  assert(false);
}

btck::BlockCheckFlags decode_block_check_flags(uint8_t value) {
  auto flags = btck::BlockCheckFlags::BASE;
  if ((value & 0x01) != 0)
    flags |= btck::BlockCheckFlags::POW;
  if ((value & 0x02) != 0)
    flags |= btck::BlockCheckFlags::MERKLE;
  return flags;
}

} // namespace

namespace bitcoinfuzz {
namespace module {
BitcoinKernelVariant::BitcoinKernelVariant(void)
    : BaseModule("BitcoinKernelVariant") {}

std::optional<std::string> BitcoinKernelVariant::kernel_transaction(
    std::span<const uint8_t> buffer) const {
  try {
    const auto raw_span = std::as_bytes(buffer);
    btck::Transaction transaction{raw_span};

    const auto txid_bytes = transaction.Txid().ToBytes();
    std::string result = "txid=";
    result.append(bytes_to_hex_reversed(txid_bytes));
    result.append(";");

    const auto txins = transaction.Inputs();
    for (const auto &txin : txins) {
      const auto outpoint = txin.OutPoint();
      uint32_t outpoint_index = outpoint.index();
      const auto outpoint_txid_bytes = outpoint.Txid().ToBytes();
      result.append("index=");
      result.append(std::to_string(outpoint_index));
      result.append("txid=");
      result.append(bytes_to_hex_reversed(outpoint_txid_bytes));
      result.append(";");
    }

    const auto txouts = transaction.Outputs();
    for (const auto &txout : txouts) {
      int64_t txout_amount = txout.Amount();
      const auto script_pubkey_bytes = txout.GetScriptPubkey().ToBytes();
      result.append("amount=");
      result.append(std::to_string(txout_amount));
      result.append("script_pubkey=");
      result.append(bytes_to_hex(script_pubkey_bytes));
      result.append(";");
    }
    return Ok(result);
  } catch (...) {
    return Fail("0");
  }
}

std::optional<std::string>
BitcoinKernelVariant::kernel_block(std::span<const uint8_t> buffer) const {
  try {
    const auto raw_span = std::as_bytes(buffer);
    btck::Block block{raw_span};

    const auto block_hash_bytes = block.GetHash().ToBytes();
    std::string result = bytes_to_hex_reversed(block_hash_bytes);

    const auto txs = block.Transactions();
    for (const auto &tx : txs) {
      const auto txid_bytes = tx.Txid().ToBytes();
      result.append("txid=");
      result.append(bytes_to_hex_reversed(txid_bytes));
      result.push_back(';');
    }
    return Ok(result);
  } catch (...) {
    return Fail("0");
  }
}

std::optional<std::string> BitcoinKernelVariant::kernel_block_check(
    std::span<const uint8_t> buffer) const {
  const uint8_t chain_selector = buffer.size() > 0 ? buffer[0] : 0;
  const uint8_t flag_selector = buffer.size() > 1 ? buffer[1] : 0;
  const auto chain_type = decode_chain_type(chain_selector);
  const auto flags = decode_block_check_flags(flag_selector);
  const auto raw_block = buffer.subspan(
      buffer.size() > 2 ? static_cast<size_t>(2) : buffer.size());

  std::string result = "chain=";
  result.append(chain_type_to_string(chain_type));
  result.append(";flags=");
  result.append(std::to_string(flag_selector & 0x03));
  result.push_back(';');

  try {
    btck::ChainParams chain_params{chain_type};
    const auto raw_span = std::as_bytes(raw_block);
    btck::Block block{raw_span};
    btck::BlockValidationState state{};
    const bool ok =
        block.Check(chain_params.GetConsensusParams(), flags, state);

    result.append("ok=");
    result.append(ok ? "1" : "0");
    result.append(";mode=");
    result.append(std::to_string(static_cast<int>(state.GetValidationMode())));
    result.append(";result=");
    result.append(
        std::to_string(static_cast<int>(state.GetBlockValidationResult())));
    result.append(";hash=");
    const auto block_hash_bytes = block.GetHash().ToBytes();
    result.append(bytes_to_hex_reversed(block_hash_bytes));
    result.append(";txs=");
    result.append(std::to_string(block.CountTransactions()));
    result.push_back(';');
    return Ok(result);
  } catch (...) {
    result.append("err=exception;");
    return Fail(result);
  }
}

std::optional<std::string>
BitcoinKernelVariant::transaction_eval(std::span<const uint8_t> buffer) const {
  try {
    const auto raw_span = std::as_bytes(buffer);
    btck::Transaction transaction{raw_span};
    btck::TxValidationState state{};
    if (!btck::CheckTransaction(transaction, state))
      return Fail("0");

    const auto transaction_bytes = transaction.ToBytes();
    const auto wtxid_bytes = transaction.Wtxid().ToBytes();

    std::string result = bytes_to_hex_reversed(wtxid_bytes);
    result += std::to_string(transaction_bytes.size());
    return Ok(result);
  } catch (...) {
    return Fail("0");
  }
}

std::optional<bool> BitcoinKernelVariant::verify_witness_spend(
    const WitnessSpendInput &input) const {
  try {
    const auto raw_span = std::as_bytes(std::span{input.tx_bytes});
    btck::Transaction transaction{raw_span};

    const auto txins = transaction.Inputs();
    if (txins.empty())
      return Skip();

    const size_t n_in = txins.size();
    const uint32_t idx = input.input_index % static_cast<uint32_t>(n_in);

    std::vector<btck::ScriptPubkey> spks;
    spks.reserve(n_in);
    std::vector<btck::TransactionOutput> outputs;
    outputs.reserve(n_in);

    if (input.all_spent_spks.size() == n_in &&
        input.all_spent_amounts.size() == n_in) {
      for (size_t i = 0; i < n_in; ++i) {
        const auto spk_span = std::as_bytes(std::span{input.all_spent_spks[i]});
        spks.emplace_back(spk_span);
        outputs.emplace_back(spks.back(), input.all_spent_amounts[i]);
      }
    } else {
      const auto default_spk_span =
          std::as_bytes(std::span{input.spent_script_pubkey});
      spks.emplace_back(default_spk_span);
      for (size_t i = 0; i < n_in; ++i) {
        outputs.emplace_back(spks.front(), input.spent_amount);
      }
    }

    btck::PrecomputedTransactionData txdata{transaction, outputs};

    uint32_t kflags = 0;
    if (input.flags & (1U << 0))
      kflags |= btck_ScriptVerificationFlags_P2SH;
    if (input.flags & (1U << 2))
      kflags |= btck_ScriptVerificationFlags_DERSIG;
    if (input.flags & (1U << 4))
      kflags |= btck_ScriptVerificationFlags_NULLDUMMY;
    if (input.flags & (1U << 9))
      kflags |= btck_ScriptVerificationFlags_CHECKLOCKTIMEVERIFY;
    if (input.flags & (1U << 10))
      kflags |= btck_ScriptVerificationFlags_CHECKSEQUENCEVERIFY;
    if (input.flags & (1U << 11)) {
      kflags |= btck_ScriptVerificationFlags_WITNESS;
      kflags |= btck_ScriptVerificationFlags_P2SH;
    }
    if (input.flags & (1U << 17)) {
      kflags |= btck_ScriptVerificationFlags_TAPROOT;
      kflags |= btck_ScriptVerificationFlags_WITNESS;
      kflags |= btck_ScriptVerificationFlags_P2SH;
    }

    const auto &target_output = outputs[idx];
    const auto target_spk_view = target_output.GetScriptPubkey();
    btck::ScriptVerifyStatus status;

    bool ok = target_spk_view.Verify(
        target_output.Amount(), transaction, &txdata, idx,
        static_cast<btck::ScriptVerificationFlags>(kflags), status);

    if (!ok &&
        status == btck::ScriptVerifyStatus::ERROR_INVALID_FLAGS_COMBINATION) {
      return Skip();
    }

    return ok;
  } catch (...) {
    return Skip();
  }
}

} // namespace module
} // namespace bitcoinfuzz
