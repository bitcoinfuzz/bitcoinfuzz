#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Pybitcoinkernel : public BaseModule {
public:
  Pybitcoinkernel(void);
  std::optional<std::string>
  kernel_block(std::span<const uint8_t> buffer) const override;
  ~Pybitcoinkernel() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
