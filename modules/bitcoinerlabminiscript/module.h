#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class BitcoinerlabMiniscript : public BaseModule {
public:
  BitcoinerlabMiniscript(void);
  std::optional<bool> miniscript_parse(std::string str) const override;
  ~BitcoinerlabMiniscript() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
