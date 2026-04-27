#include <bitcoinfuzz/basemodule.h>
#include <optional>
#include <vector>

namespace bitcoinfuzz {
namespace module {
class Utreexo : public BaseModule {
public:
  Utreexo(void);

  std::optional<std::string> stump_modify_add(
      const std::vector<std::vector<uint8_t>> &add_hashes) const override;

  ~Utreexo() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz
