#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bitcoinfuzz {
class BaseModule {
public:
  const std::string name;

  BaseModule(const std::string &name) : name(name) {}

  virtual std::optional<std::string>
  script_parse(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  deserialize_block(std::span<const uint8_t> buffer) const;
  virtual std::optional<bool>
  script_eval(const std::vector<uint8_t> &input_data, unsigned int flags,
              size_t version) const;
  virtual std::optional<bool> descriptor_parse(std::string str) const;
  virtual std::optional<bool> miniscript_parse(std::string str) const;
  virtual std::optional<std::string> deserialize_invoice(std::string str) const;
  virtual std::optional<std::string> address_parse(std::string str) const;
  virtual std::optional<std::string>
  psbt_parse(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  addrv2_parse(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string> deserialize_offer(std::string str) const;
  virtual std::optional<int>
  cmpctblocks_parse(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  parse_p2p_message(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  parse_p2p_lightning_message(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  transaction_eval(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  bip32_master_keygen(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  kernel_block(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  private_to_public_key(std::span<const uint8_t> buffer) const;
  virtual std::optional<std::string>
  sign_compact(std::span<const uint8_t> buffer,
               std::span<const uint8_t> hash) const;
  virtual std::optional<std::string>
  sign_der(std::span<const uint8_t> buffer,
           std::span<const uint8_t> hash) const;
  virtual std::optional<bool> sign_verify(std::span<const uint8_t> buffer,
                                          std::span<const uint8_t> hash,
                                          std::span<const uint8_t> sign) const;
  virtual std::optional<std::string>
  ecdh(std::span<const uint8_t> buffer, std::span<const uint8_t> pubkey) const;

  virtual ~BaseModule() noexcept;
};
} // namespace bitcoinfuzz
