#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>

#include <bitcoinfuzz/basemodule.h>
#include <bitcoinfuzz/modulelogger.h>

namespace bitcoinfuzz {
class Driver {
private:
  ModuleLogger &module_logger;
  std::map<std::string, std::shared_ptr<BaseModule>> modules;

public:
  Driver(ModuleLogger &logger) : module_logger(logger) {}
  void LoadModule(std::shared_ptr<BaseModule> module);
  void ScriptTarget(std::span<const uint8_t>) const;
  void ScriptEvalTarget(std::span<const uint8_t> buffer) const;
  void VerifyScriptTarget(std::span<const uint8_t> buffer) const;
  void BlockDeserializationTarget(std::span<const uint8_t> buffer) const;
  void DescriptorParseTarget(std::span<const uint8_t> buffer) const;
  void MiniscriptParseTarget(std::span<const uint8_t> buffer) const;
  void InvoiceDeserializationTarget(std::span<const uint8_t> buffer) const;
  void Run(const uint8_t *data, const size_t size,
           const std::string &target) const;
  void AddressParseTarget(std::span<const uint8_t> buffer) const;
  void PSBTParseTarget(std::span<const uint8_t> buffer) const;
  void AddrV2Target(std::span<const uint8_t> buffer) const;
  void OfferDeserializationTarget(std::span<const uint8_t> buffer) const;
  void CompactBlocksTarget(std::span<const uint8_t> buffer) const;
  void ParseP2PMessageTarget(std::span<const uint8_t> buffer) const;
  void ParseLightningP2pMessageTarget(std::span<const uint8_t> buffer) const;
  void TransactionEvalTarget(std::span<const uint8_t> buffer) const;
  void Bip32MasterKeygenTarget(std::span<const uint8_t> buffer) const;
  void KernelBlockTarget(std::span<const uint8_t> buffer) const;
  void KernelTransactionTarget(std::span<const uint8_t> buffer) const;
  void PrivateToPublicKeyTarget(std::span<const uint8_t> buffer) const;
  void SignCompactTarget(std::span<const uint8_t> buffer) const;
  void SignDerTarget(std::span<const uint8_t> buffer) const;
  void SignVerifyTarget(std::span<const uint8_t> buffer) const;
  void ECDHTarget(std::span<const uint8_t> buffer) const;
  void SignSchnorrTarget(std::span<const uint8_t> buffer) const;
  void Bip32DeserializeExtendedKeyTarget(std::span<const uint8_t> buffer) const;
  void DecodeEllswiftTarget(std::span<const uint8_t> buffer) const;
  void EllswiftRoundTripXTarget(std::span<const uint8_t> buffer) const;
  void SchnorrVerifyTarget(std::span<const uint8_t> buffer) const;
  void DecodeOnionTarget(std::span<const uint8_t> buffer) const;
  void StumpModifyAddTarget(std::span<const uint8_t> buffer) const;
};
} // namespace bitcoinfuzz
