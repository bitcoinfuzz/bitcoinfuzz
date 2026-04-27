#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class RustPsbt : public BaseModule {
public:
  RustPsbt(void);
  std::optional<std::string>
  psbt_parse(std::span<const uint8_t> buffer) const override;
  ~RustPsbt() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
