#include <bitcoinfuzz/basemodule.h>
#include <span>

namespace bitcoinfuzz {
namespace module {
class BitcoinKernel : public BaseModule {
public:
  BitcoinKernel(void);
  std::optional<std::string>
  kernel_block(std::span<const uint8_t> buffer) const override;
  ~BitcoinKernel() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz