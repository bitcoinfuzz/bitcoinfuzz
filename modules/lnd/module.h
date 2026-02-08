#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Lnd : public BaseModule {
public:
  Lnd(void);
  std::optional<std::string>
  deserialize_invoice(std::string str) const override;
  std::optional<std::string>
  parse_p2p_lightning_message(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  decode_onion(std::span<const uint8_t> buffer) const override;
  ~Lnd() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz