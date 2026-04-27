#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Pyhdwallet : public BaseModule {
public:
  Pyhdwallet(void);
  std::optional<std::string>
  bip32_master_keygen(std::span<const uint8_t> buffer) const override;
  ~Pyhdwallet() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz