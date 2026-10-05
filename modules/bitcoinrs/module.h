#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {
class BitcoinRs : public BaseModule {
public:
  BitcoinRs(void);
  std::optional<std::string>
  deserialize_block(std::span<const uint8_t> buffer) const override;
  ~BitcoinRs() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
