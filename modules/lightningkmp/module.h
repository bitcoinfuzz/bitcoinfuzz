#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class LightningKmp : public BaseModule {
public:
  LightningKmp(void);
  std::optional<std::string>
  deserialize_invoice(std::string str) const override;
  std::optional<std::string> deserialize_offer(std::string str) const override;
  ~LightningKmp() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz