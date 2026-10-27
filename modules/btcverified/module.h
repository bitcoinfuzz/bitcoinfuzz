#include <bitcoinfuzz/basemodule.h>
#include <span>

namespace bitcoinfuzz {
namespace module {
class BtcVerified : public BaseModule {
public:
  BtcVerified(void);
  std::optional<std::string>
  kernel_transaction_check(std::span<const uint8_t> buffer) const override;
  ~BtcVerified() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
