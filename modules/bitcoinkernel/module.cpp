#include "module.h"
#include <kernel/bitcoinkernel_wrapper.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>

namespace {
std::string bytes_to_hex(const uint8_t *bytes, int length) {
  std::stringstream string_stream;
  string_stream << std::hex;
  for (int i = length - 1; i >= 0; --i) {
    string_stream << std::setw(2) << std::setfill('0') << (int)bytes[i];
  }

  return string_stream.str();
}

char *libbitcoinkernel_transaction(std::span<const uint8_t> buffer) {
  try {
    std::span<const std::byte> raw_span{(const std::byte *)buffer.data(),
                                        buffer.size()};
    btck::Transaction transaction{raw_span};

    const auto txid_bytes = transaction.Txid().ToBytes();
    std::string result = "txid=";
    result.append(bytes_to_hex((const uint8_t *)txid_bytes.data(),
                               static_cast<int>(txid_bytes.size())));
    result.append(";");

    const auto txins = transaction.Inputs();
    for (const auto &txin : txins) {
      const auto outpoint = txin.OutPoint();
      uint32_t outpoint_index = outpoint.index();
      const auto outpoint_txid_bytes = outpoint.Txid().ToBytes();
      result.append("index=");
      result.append(std::to_string(outpoint_index));
      result.append("txid=");
      result.append(bytes_to_hex((const uint8_t *)outpoint_txid_bytes.data(),
                                 static_cast<int>(outpoint_txid_bytes.size())));
      result.append(";");
    }

    const auto txouts = transaction.Outputs();
    for (const auto &txout : txouts) {
      int64_t txout_amount = txout.Amount();
      const auto script_pubkey_bytes = txout.GetScriptPubkey().ToBytes();
      result.append("amount=");
      result.append(std::to_string(txout_amount));
      result.append("script_pubkey=");
      result.append(bytes_to_hex((const uint8_t *)script_pubkey_bytes.data(),
                                 static_cast<int>(script_pubkey_bytes.size())));
      result.append(";");
    }
    return strdup(result.c_str());
  } catch (...) {
    return strdup("0");
  }
}

char *libbitcoinkernel_block(std::span<const uint8_t> buffer) {
  try {
    std::span<const std::byte> raw_span{(const std::byte *)buffer.data(),
                                        buffer.size()};
    btck::Block block{raw_span};

    const auto block_hash_bytes = block.GetHash().ToBytes();
    std::string result =
        bytes_to_hex((const uint8_t *)block_hash_bytes.data(),
                     static_cast<int>(block_hash_bytes.size()));

    const auto txs = block.Transactions();
    for (const auto &tx : txs) {
      const auto txid_bytes = tx.Txid().ToBytes();
      result.append("txid=");
      result.append(bytes_to_hex((const uint8_t *)txid_bytes.data(),
                                 static_cast<int>(txid_bytes.size())));
      result.push_back(';');
    }
    return strdup(result.c_str());

  } catch (...) {
    return strdup("0");
  }
}
} // namespace

namespace bitcoinfuzz {
namespace module {
BitcoinKernel::BitcoinKernel(void) : BaseModule("BitcoinKernel") {}

std::optional<std::string>
BitcoinKernel::kernel_transaction(std::span<const uint8_t> buffer) const {
  auto result_ptr = libbitcoinkernel_transaction(buffer);
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}

std::optional<std::string>
BitcoinKernel::kernel_block(std::span<const uint8_t> buffer) const {
  auto result_ptr = libbitcoinkernel_block(buffer);
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}

} // namespace module
} // namespace bitcoinfuzz
