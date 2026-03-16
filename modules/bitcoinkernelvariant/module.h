#include <bitcoinfuzz/basemodule.h>
#include <span>

namespace bitcoinfuzz {
namespace module {
class BitcoinKernelVariant : public BaseModule {
public:
  BitcoinKernelVariant(void);
  std::optional<std::string>
  kernel_block(std::span<const uint8_t> buffer) const override;
  std::optional<std::string>
  kernel_transaction(std::span<const uint8_t> buffer) const override;
  ~BitcoinKernelVariant() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
