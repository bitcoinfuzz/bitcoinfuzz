#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Btcd : public BaseModule {
public:
  Btcd(void);
  std::optional<bool> script_eval(const std::vector<uint8_t> &input_data,
                                  unsigned int flags,
                                  size_t version) const override;
  std::optional<std::string>
  deserialize_block(std::span<const uint8_t> buffer) const override;
  std::optional<std::string> address_parse(std::string str) const override;
  std::optional<std::string>
  psbt_parse(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  addrv2_parse(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  parse_p2p_message(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  transaction_eval(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  bip32_master_keygen(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  sign_schnorr(std::span<const uint8_t> buffer, std::span<const uint8_t> hash,
               std::span<const uint8_t> aux) const override;
  ~Btcd() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
