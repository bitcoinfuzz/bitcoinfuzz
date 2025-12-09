#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Tinyminiscript : public BaseModule {
public:
  Tinyminiscript(void);
  std::optional<bool> descriptor_parse(std::string str) const override;
  ~Tinyminiscript() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
