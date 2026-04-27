#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class NLightning : public BaseModule {
public:
  NLightning(void);
  std::optional<std::string>
  deserialize_invoice(std::string str) const override;
  ~NLightning() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz