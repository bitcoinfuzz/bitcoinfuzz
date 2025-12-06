#include <bitcoinfuzz/basemodule.h>
#include <optional>
#include <span>

namespace bitcoinfuzz {
namespace module {
class Decred_secp256k1 : public BaseModule {
public:
  Decred_secp256k1(void);
  std::optional<std::string>
  private_to_public_key(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  sign_compact(std::span<const uint8_t> buffer,
               std::span<const uint8_t> hash) const override;
  std::optional<std::string>
  sign_der(std::span<const uint8_t> buffer,
           std::span<const uint8_t> hash) const override;
  std::optional<bool> sign_verify(std::span<const uint8_t> buffer,
                                  std::span<const uint8_t> hash,
                                  std::span<const uint8_t> sign) const override;
  std::optional<std::string>
  ecdh(std::span<const uint8_t> buffer,
       std::span<const uint8_t> pubkey) const override;
  ~Decred_secp256k1() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz
