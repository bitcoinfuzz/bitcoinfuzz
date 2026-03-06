#include "module.h"
#include "utreexo_wrapper/libutreexo_wrapper.h"
#include <vector>

namespace bitcoinfuzz {
namespace module {
Utreexo::Utreexo(void) : BaseModule("Utreexo") {}

std::optional<std::string> Utreexo::stump_modify_add(
    const std::vector<std::vector<uint8_t>> &add_hashes) const {
  std::vector<uint8_t> flat;
  flat.reserve(add_hashes.size() * 32);
  for (const auto &hash : add_hashes)
    flat.insert(flat.end(), hash.begin(), hash.end());

  ByteArray leaf_hashes{.data = reinterpret_cast<char *>(flat.data()),
                        .length = static_cast<int>(flat.size())};

  auto result = UtreexoStumpUpdate(leaf_hashes);
  if (!result)
    return std::nullopt;
  std::string result_str(result);
  free(result);
  return result_str;
}
} // namespace module
} // namespace bitcoinfuzz
