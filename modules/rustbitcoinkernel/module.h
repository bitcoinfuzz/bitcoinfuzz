#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Rustbitcoinkernel : public BaseModule {
public:
  Rustbitcoinkernel(void);
  std::optional<std::string>
  kernel_block(std::span<const uint8_t> buffer) const override;
  ~Rustbitcoinkernel() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
