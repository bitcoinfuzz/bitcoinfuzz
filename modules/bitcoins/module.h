#pragma once

#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {

class BitcoinS : public BaseModule {
public:
  BitcoinS(void);

  std::optional<std::string>
  bip32_master_keygen(std::span<const uint8_t> buffer) const override;

  std::optional<std::string> bip32_deserialize_extended_key(
      std::span<const uint8_t> buffer) const override;

  std::optional<std::string>
  psbt_parse(std::span<const uint8_t> buffer) const override;

  ~BitcoinS() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
