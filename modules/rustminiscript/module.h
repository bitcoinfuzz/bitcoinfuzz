#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Rustminiscript : public BaseModule {
public:
  Rustminiscript(void);
  std::optional<bool> descriptor_parse(std::string str) const override;
  std::optional<bool> miniscript_parse(std::string str) const override;
  ~Rustminiscript() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
