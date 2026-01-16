#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {
class Floresta : public BaseModule {
public:
  Floresta(void);
  std::optional<std::string>
  addrv2_parse(std::span<const uint8_t> buffer) const override;
  ~Floresta() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
