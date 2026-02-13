#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class NBitcoin : public BaseModule {
public:
  NBitcoin(void);
  std::optional<bool> miniscript_parse(std::string str) const override;
  std::optional<bool> descriptor_parse(std::string str) const override;
  std::optional<bool> script_eval(const std::vector<uint8_t> &input_data,
                                  unsigned int flags,
                                  size_t version) const override;
  std::optional<std::string>
  bip32_master_keygen(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  psbt_parse(std::span<const uint8_t> buffer) const override;
  std::optional<std::string> bip32_deserialize_extended_key(
      std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  sign_schnorr(std::span<const uint8_t> buffer, std::span<const uint8_t> hash,
               std::span<const uint8_t> aux) const override;
  ~NBitcoin() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz
