#include "module.h"
#include "bitcoinkernel_variant_symbol_prefix.h"

#include <bitcoinfuzz/result.h>
#include <crypto/sha256.h>
#include <kernel/bitcoinkernel_wrapper.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <vector>

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

using Hash32 = std::array<uint8_t, 32>;

// SHA256d(left || right), the inner-node hash of Core's merkle trees.
Hash32 Sha256dPair(const Hash32 &left, const Hash32 &right) {
  uint8_t first[CSHA256::OUTPUT_SIZE];
  CSHA256()
      .Write(left.data(), left.size())
      .Write(right.data(), right.size())
      .Finalize(first);
  Hash32 out;
  CSHA256().Write(first, sizeof(first)).Finalize(out.data());
  return out;
}

// Mirrors ComputeMerkleRoot from consensus/merkle.cpp. The mutation flag is
// ignored, as in BlockWitnessMerkleRoot.
Hash32 ComputeMerkleRoot(std::vector<Hash32> level) {
  if (level.empty())
    return Hash32{};
  while (level.size() > 1) {
    if (level.size() & 1)
      level.push_back(level.back());
    std::vector<Hash32> next;
    next.reserve(level.size() / 2);
    for (size_t i = 0; i < level.size(); i += 2)
      next.push_back(Sha256dPair(level[i], level[i + 1]));
    level = std::move(next);
  }
  return level.front();
}

// Mirrors GetWitnessCommitmentIndex: the last output matching the BIP141
// pattern wins. Returns its 32-byte commitment (scriptPubKey[6..38)).
std::optional<Hash32> FindWitnessCommitment(const btck::TransactionView &cb) {
  constexpr size_t MINIMUM_WITNESS_COMMITMENT = 38;
  std::optional<Hash32> commitment;
  for (size_t o = 0; o < cb.CountOutputs(); ++o) {
    const auto spk = cb.GetOutput(o).GetScriptPubkey().ToBytes();
    if (spk.size() >= MINIMUM_WITNESS_COMMITMENT && spk[0] == std::byte{0x6a} &&
        spk[1] == std::byte{0x24} && spk[2] == std::byte{0xaa} &&
        spk[3] == std::byte{0x21} && spk[4] == std::byte{0xa9} &&
        spk[5] == std::byte{0xed}) {
      Hash32 c;
      std::memcpy(c.data(), spk.data() + 6, c.size());
      commitment = c;
    }
  }
  return commitment;
}

// Mirrors CheckWitnessMalleation(block, /*expect_witness_commitment=*/true).
// Returns true if the block is mutated. The merkle-root check and the
// "no coinbase => 64-byte tx" branch of IsBlockMutated are unreachable after
// a successful btck block check with MERKLE, so they are not modelled.
bool IsWitnessMutated(const btck::Block &block) {
  const size_t num_txs = block.CountTransactions();
  if (num_txs == 0)
    return true; // unreachable after Check()

  const auto coinbase = block.GetTransaction(0);
  const auto commitment = FindWitnessCommitment(coinbase);

  if (!commitment) {
    // No commitment: no transaction may carry witness data.
    for (size_t i = 0; i < num_txs; ++i) {
      if (block.GetTransaction(i).HasWitness())
        return true;
    }
    return false;
  }

  if (coinbase.CountInputs() == 0)
    return true; // unreachable: CheckTransaction rejects empty vin
  const auto cb_input = coinbase.GetInput(0);
  const auto stack = cb_input.GetWitnessStack();
  if (stack.CountItems() != 1)
    return true;
  const auto nonce = stack.GetItem(0);
  if (nonce.size() != 32)
    return true;

  // Witness merkle root: coinbase leaf is all zeros, others are wtxids in
  // internal byte order.
  std::vector<Hash32> leaves;
  leaves.reserve(num_txs);
  leaves.emplace_back(); // value-initialised: 32 zero bytes
  for (size_t i = 1; i < num_txs; ++i) {
    const auto wtxid = block.GetTransaction(i).Wtxid().ToBytes();
    Hash32 leaf;
    std::memcpy(leaf.data(), wtxid.data(), leaf.size());
    leaves.push_back(leaf);
  }
  const Hash32 root = ComputeMerkleRoot(std::move(leaves));

  Hash32 nonce32;
  std::memcpy(nonce32.data(), nonce.data(), nonce32.size());
  return Sha256dPair(root, nonce32) != *commitment;
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

std::optional<std::string>
BitcoinKernelVariant::deserialize_block(std::span<const uint8_t> buffer) const {
  try {
    const auto raw_span = std::as_bytes(buffer);
    btck::Block block{raw_span};

    // Core: CheckBlock(..., /*fCheckPOW=*/false), merkle check on (default).
    btck::ChainParams chain_params{btck::ChainType::MAINNET};
    btck::BlockValidationState state{};
    if (!block.Check(chain_params.GetConsensusParams(),
                     btck::BlockCheckFlags::MERKLE, state))
      return Fail("0");

    // Core: IsBlockMutated(block, /*check_witness_root=*/true).
    if (IsWitnessMutated(block))
      return Fail("0");

    return Ok(bytes_to_hex_reversed(block.GetHash().ToBytes()));
  } catch (...) {
    return Skip();
  }
}

} // namespace module
} // namespace bitcoinfuzz
