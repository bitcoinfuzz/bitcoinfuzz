#include <bitcoinfuzz/basemodule.h>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Secp256k1 : public BaseModule {
public:
  Secp256k1(void);
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
  std::optional<std::string>
  sign_schnorr(std::span<const uint8_t> buffer, std::span<const uint8_t> hash,
               std::span<const uint8_t> aux) const override;
  std::optional<std::string>
  decode_ellswift(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  schnorr_verify(std::span<const uint8_t> privkey,
                 std::span<const uint8_t> hash,
                 std::span<const uint8_t> sign) const override;
  std::optional<std::string>
  musig2_key_agg(std::span<const uint8_t> buffer) const override;
  ~Secp256k1() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz