#include <cassert>
#include <fuzzer/FuzzedDataProvider.h>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>

#include "driver.h"
#include <bitcoinfuzz/basemodule.h>
#include <bitcoinfuzz/module_registry.h>

namespace {
template <typename T>
void VerifyMatchingResponse(std::optional<T> &last_response,
                            std::string &last_module_name,
                            const std::string &module_name, const T &response,
                            std::string_view failure_message) {
  if (last_response.has_value()) {
    if (response != *last_response) {
      std::cout << failure_message << std::endl;
      std::cout << "Module: " << module_name << std::endl;
      std::cout << "Result: " << response << std::endl;
      std::cout << "Module: " << last_module_name << std::endl;
      std::cout << "Result: " << *last_response << std::endl;
    }
    assert(response == *last_response);
  }

  last_response = response;
  last_module_name = module_name;
}
} // namespace

namespace bitcoinfuzz {
void Driver::LoadModule(std::shared_ptr<BaseModule> module) {
  modules[module->name] = module;
  module_logger.addModule(module->name);
}

void Driver::ScriptTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<std::string> res{module.second->script_parse(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Script parse failed");
  }
}

void Driver::BlockDeserializationTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<std::string> res{module.second->deserialize_block(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Block deserialization failed");
  }
}

void Driver::ScriptEvalTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::vector<uint8_t> input_data = provider.ConsumeBytes<uint8_t>(
      provider.ConsumeIntegralInRange<size_t>(0, 1024));

#ifdef BTCD
  if (ModuleRegistry::instance().isEnabled("BTCD")) {
    if (std::ranges::find(input_data, 0xAC) != input_data.end() ||
        std::ranges::find(input_data, 0xAE) != input_data.end() ||
        std::ranges::find(input_data, 0xAF) != input_data.end())
      return;
  }
#endif

#ifdef NBITCOIN
  if (std::ranges::find(input_data, 0xB2) != input_data.end())
    return;
#endif

  auto flags = provider.ConsumeIntegral<unsigned int>();

  std::optional<bool> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<bool> res{
        module.second->script_eval(input_data, flags, /*version=*/0)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Script evaluation failed");
  }
}

void Driver::VerifyScriptTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::vector<uint8_t> script_sig = provider.ConsumeBytes<uint8_t>(
      provider.ConsumeIntegralInRange<size_t>(0, 1024));

  std::vector<uint8_t> script_pubkey = provider.ConsumeBytes<uint8_t>(
      provider.ConsumeIntegralInRange<size_t>(0, 1024));

#if defined(BTCD) || defined(GOCOIN)
  // Skip these opcodes since there is a discrepancy between Core's
  // FindAndDelete and btcd's removeOpcodeByData.
  if (ModuleRegistry::instance().isEnabled("BTCD") ||
      ModuleRegistry::instance().isEnabled("GOCOIN")) {
    auto opcodes_to_skip = [](unsigned char op) {
      return op >= 0xAC && op <= 0xAF;
    };

    if (std::ranges::any_of(script_sig, opcodes_to_skip) ||
        std::ranges::any_of(script_pubkey, opcodes_to_skip))
      return;
  }
#endif

  std::optional<bool> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<bool> res{
        module.second->verify_script(script_sig, script_pubkey)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Script verification failed");
  }
}

void Driver::DescriptorParseTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::string desc{provider.ConsumeRemainingBytesAsString()};
  std::optional<bool> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<bool> res{module.second->descriptor_parse(desc)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Descriptor parse failed for " + desc);
  }
}

void Driver::MiniscriptParseTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::string miniscript{provider.ConsumeRemainingBytesAsString()};
  // Skip these cases
  if (strcmp(miniscript.c_str(), "1") == 0 ||
      strcmp(miniscript.c_str(), "0") == 0)
    return;
  std::optional<bool> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<bool> res{module.second->miniscript_parse(miniscript)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Miniscript parse failed for " + miniscript);
  }
}

void Driver::InvoiceDeserializationTarget(
    std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::string invoice{provider.ConsumeRemainingBytesAsString()};
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->deserialize_invoice(invoice)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Invoice deserialization failed for " + invoice);
  }
}

void Driver::AddressParseTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::string address{provider.ConsumeRemainingBytesAsString()};

  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->address_parse(address)};
    if (!res.has_value() || res->starts_with("UNK:"))
      continue;

    if (last_response.has_value()) {
      if (*res != *last_response) {
        std::cout << "Input address: " << address << "\n";
        std::cout << "MISMATCH DETECTED between " << last_module_name << " and "
                  << module.first << "!" << "\n";
        std::cout << "  " << last_module_name << ": " << *last_response << "\n";
        std::cout << "  " << module.first << ": " << *res << "\n";
        assert(*res == *last_response);
      }
    }
    last_response = *res;
    last_module_name = module.first;
  }
}

void Driver::PSBTParseTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->psbt_parse(buffer)};
    if (!res.has_value())
      continue;

#ifdef NBITCOIN
    if (ModuleRegistry::instance().isEnabled("NBITCOIN")) {
      if (res->find("in=0") != std::string::npos ||
          res->find("out=0") != std::string::npos)
        return;
    }
#endif

    if (last_response.has_value()) {
      if (*res != *last_response) {
        std::cout << "Input PSBT (truncated): ";
        for (size_t i = 0; i < std::min(size_t(32), buffer.size()); ++i)
          printf("%02x", buffer[i]);
        if (buffer.size() > 32)
          std::cout << "...";
        std::cout << " (" << buffer.size() << " bytes)\n";

        std::cout << "MISMATCH DETECTED between " << last_module_name << " and "
                  << module.first << "!" << "\n";

        // Find and highlight the differences
        std::string last = *last_response;
        std::string current = *res;

        // Print the full outputs only if they're reasonably sized
        if (last.size() < 1000 && current.size() < 1000) {
          std::cout << "  " << last_module_name << ": " << last << "\n";
          std::cout << "  " << module.first << ": " << current << "\n";
        } else {
          // Find first differing position
          size_t pos = 0;
          while (pos < last.size() && pos < current.size() &&
                 last[pos] == current[pos])
            pos++;

          // Print context around the difference
          size_t context = 20;
          size_t start = (pos > context) ? pos - context : 0;

          std::cout << "  Difference at position " << pos << "\n";
          std::cout << "  " << last_module_name << " (excerpt): ..."
                    << last.substr(start, context * 2) << "...\n";
          std::cout << "  " << module.first << " (excerpt): ..."
                    << current.substr(start, context * 2) << "...\n";
        }
      }

      assert(*res == *last_response);
    }
    last_response = *res;
    last_module_name = module.first;
  }
}

void Driver::AddrV2Target(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;
  for (auto &module : modules) {
    std::optional<std::string> res{module.second->addrv2_parse(buffer)};
    if (!res.has_value())
      continue;
    if (last_response.has_value()) {
      if (*res != *last_response) {
#ifdef BTCD
        // Skip mismatches involving btcd when i2p or cjdns addresses are
        // present (btcd always skips i2p and cjdns)
        bool involves_btcd =
            (module.first == "Btcd" || last_module_name == "Btcd");
        bool has_i2p_or_cjdns =
            (res->find("i2p") != std::string::npos ||
             res->find("cjdns") != std::string::npos ||
             last_response->find("i2p") != std::string::npos ||
             last_response->find("cjdns") != std::string::npos);
        if (involves_btcd && has_i2p_or_cjdns) {
          last_response = *res;
          last_module_name = module.first;
          continue;
        }
#endif
        std::cout << "Addrv2 parse failed" << std::endl;
        std::cout << "Module: " << module.first << std::endl;
        std::cout << "Result: " << *res << std::endl;
        std::cout << "Module: " << last_module_name << std::endl;
        std::cout << "Result: " << *last_response << std::endl;
      }
      assert(*res == *last_response);
    }
    last_response = *res;
    last_module_name = module.first;
  }
}

void Driver::OfferDeserializationTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  std::string offer{provider.ConsumeRemainingBytesAsString()};
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->deserialize_offer(offer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Offer deserialization failed for " + offer);
  }
}

void Driver::CompactBlocksTarget(std::span<const uint8_t> buffer) const {
  std::optional<int> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<int> res{module.second->cmpctblocks_parse(buffer)};
    if (!res.has_value() || *res == -2)
      continue;

    if (last_response.has_value()) {
      if (*res != *last_response) {
        if (!buffer.empty()) {
          for (size_t i = 0; i < std::min(size_t(32), buffer.size()); ++i)
            printf("%02x", buffer[i]);
          if (buffer.size() > 32)
            std::cout << "...";
        }

        std::cout << " (" << buffer.size() << "bytes)\n";
        std::cout << "MISMATCH DETECTED between " << last_module_name << " and "
                  << module.first << "!" << "\n";
        std::cout << "  " << last_module_name << ": " << *last_response << "\n";
        std::cout << "  " << module.first << ": " << *res << "\n";
      }

      assert(*res == *last_response);
    }
    last_response = *res;
    last_module_name = module.first;
  }
}

void Driver::ParseP2PMessageTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->parse_p2p_message(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "P2P message parsing failed");
  }
}

void Driver::TransactionEvalTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->transaction_eval(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Transaction evaluation failed");
  }
}

void Driver::KernelBlockTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->kernel_block(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(
        last_response, last_module_name, module.first, *res,
        "Block parsing from libbitcoinkernel binding failed:");
  }
}

void Driver::KernelTransactionTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->kernel_transaction(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(
        last_response, last_module_name, module.first, *res,
        "Transaction parsing from libbitcoinkernel binding failed:");
  }
}

void Driver::ParseLightningP2pMessageTarget(
    std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->parse_p2p_lightning_message(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Lightning P2P message parsing failed");
  }
}
void Driver::Bip32MasterKeygenTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->bip32_master_keygen(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "BIP32 master keygen failed");
  }
}

void Driver::PrivateToPublicKeyTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() < 32)
    return;

  std::vector<uint8_t> privkey_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->private_to_public_key(privkey_buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "PrivateToPublicKey Target failed");
  }
}

void Driver::SignCompactTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() < 64)
    return;

  std::vector<uint8_t> privkey_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> hash_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->sign_compact(privkey_buffer, hash_buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "SignCompact Target failed");
  }
}

void Driver::SignDerTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() < 64)
    return;

  std::vector<uint8_t> privkey_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> hash_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->sign_der(privkey_buffer, hash_buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "SignDer Target failed");
  }
}

void Driver::SignVerifyTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() < 72)
    return;

  std::vector<uint8_t> privkey_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> hash_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> sign_buffer = provider.ConsumeRemainingBytes<uint8_t>();
  std::optional<bool> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<bool> res{
        module.second->sign_verify(privkey_buffer, hash_buffer, sign_buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "SignVerify Target failed");
  }
}

void Driver::ECDHTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() < 65)
    return;

  std::vector<uint8_t> privkey_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> pubkey_buffer = provider.ConsumeBytes<uint8_t>(33);
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->ecdh(privkey_buffer, pubkey_buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "ECDH Target failed");
  }
}

void Driver::SignSchnorrTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() < 96)
    return;

  std::vector<uint8_t> privkey_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> hash_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> aux_buffer = provider.ConsumeBytes<uint8_t>(32);
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->sign_schnorr(privkey_buffer, hash_buffer, aux_buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "SignSchnorr Target failed");
  }
}

void Driver::Bip32DeserializeExtendedKeyTarget(
    std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->bip32_deserialize_extended_key(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "BIP32 deserialize extended key failed");
  }
}

void Driver::DecodeEllswiftTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() != 64)
    return;

  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->decode_ellswift(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "DecodeEllswiftTarget Target failed");
  }
}

void Driver::SchnorrVerifyTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());
  if (buffer.size() != 128)
    return;

  std::vector<uint8_t> privkey = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> hash = provider.ConsumeBytes<uint8_t>(32);
  std::vector<uint8_t> sign = provider.ConsumeBytes<uint8_t>(64);
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{
        module.second->schnorr_verify(privkey, hash, sign)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "SchnorrVerifyTarget failed");
  }
}

void Driver::DecodeOnionTarget(std::span<const uint8_t> buffer) const {
  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->decode_onion(buffer)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "Onion decoding failed");
  }
}

void Driver::StumpModifyAddTarget(std::span<const uint8_t> buffer) const {
  FuzzedDataProvider provider(buffer.data(), buffer.size());

  std::vector<std::vector<uint8_t>> add_hashes;
  while (true) {
    auto hash = provider.ConsumeBytes<uint8_t>(32);
    if (hash.size() < 32) {
      break;
    }
    add_hashes.push_back(std::move(hash));
  }

  std::optional<std::string> last_response{std::nullopt};
  std::string last_module_name;

  for (auto &module : modules) {
    std::optional<std::string> res{module.second->stump_modify_add(add_hashes)};
    if (!res.has_value())
      continue;

    VerifyMatchingResponse(last_response, last_module_name, module.first, *res,
                           "StumpModifyAddTarget failed");
  }
}

void Driver::Run(const uint8_t *data, const size_t size,
                 const std::string &target) const {
  std::span<const uint8_t> buffer{data, size};
  if (target == "script") {
    this->ScriptTarget(buffer);
  } else if (target == "deserialize_block") {
    this->BlockDeserializationTarget(buffer);
  } else if (target == "script_eval") {
    this->ScriptEvalTarget(buffer);
  } else if (target == "verify_script") {
    this->VerifyScriptTarget(buffer);
  } else if (target == "descriptor_parse") {
    this->DescriptorParseTarget(buffer);
  } else if (target == "miniscript_parse") {
    this->MiniscriptParseTarget(buffer);
  } else if (target == "deserialize_invoice") {
    this->InvoiceDeserializationTarget(buffer);
  } else if (target == "address_parse") {
    this->AddressParseTarget(buffer);
  } else if (target == "psbt_parse") {
    this->PSBTParseTarget(buffer);
  } else if (target == "addrv2") {
    this->AddrV2Target(buffer);
  } else if (target == "deserialize_offer") {
    this->OfferDeserializationTarget(buffer);
  } else if (target == "cmpctblocks_parse") {
    this->CompactBlocksTarget(buffer);
  } else if (target == "parse_p2p_message") {
    this->ParseP2PMessageTarget(buffer);
  } else if (target == "parse_p2p_lightning_message") {
    this->ParseLightningP2pMessageTarget(buffer);
  } else if (target == "transaction_eval") {
    this->TransactionEvalTarget(buffer);
  } else if (target == "bip32_master_keygen") {
    this->Bip32MasterKeygenTarget(buffer);
  } else if (target == "kernel_block") {
    this->KernelBlockTarget(buffer);
  } else if (target == "kernel_transaction") {
    this->KernelTransactionTarget(buffer);
  } else if (target == "private_to_public_key") {
    this->PrivateToPublicKeyTarget(buffer);
  } else if (target == "sign_compact") {
    this->SignCompactTarget(buffer);
  } else if (target == "sign_der") {
    this->SignDerTarget(buffer);
  } else if (target == "sign_verify") {
    this->SignVerifyTarget(buffer);
  } else if (target == "ecdh") {
    this->ECDHTarget(buffer);
  } else if (target == "sign_schnorr") {
    this->SignSchnorrTarget(buffer);
  } else if (target == "bip32_deserialize_extended_key") {
    this->Bip32DeserializeExtendedKeyTarget(buffer);
  } else if (target == "decode_ellswift") {
    this->DecodeEllswiftTarget(buffer);
  } else if (target == "schnorr_verify") {
    this->SchnorrVerifyTarget(buffer);
  } else if (target == "decode_onion") {
    this->DecodeOnionTarget(buffer);
  } else if (target == "stump_modify_add") {
    this->StumpModifyAddTarget(buffer);
  } else {
    std::cout << "Target not defined!" << std::endl;
    assert(false);
  }
};

} // namespace bitcoinfuzz
