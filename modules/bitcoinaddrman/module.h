#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {
class Bitcoinaddrman : public BaseModule {
public:
  Bitcoinaddrman(void);
  std::optional<std::string>
  addrman_serialize(std::span<const uint8_t> buffer) const override;
  ~Bitcoinaddrman() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
