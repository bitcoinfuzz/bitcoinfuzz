#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class CLightning : public BaseModule {
public:
  CLightning(void);
  std::optional<std::string>
  deserialize_invoice(std::string str) const override;
  std::optional<std::string> deserialize_offer(std::string str) const override;
  std::optional<std::string>
  parse_p2p_lightning_message(std::span<const uint8_t> buffer) const override;
  ~CLightning() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz