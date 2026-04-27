#include "module.h"
#include "bitcoinfuzz/basemodule.h"
#include "rustreexo_lib/rustreexo_lib.h"
#include <cstdint>
#include <vector>

namespace bitcoinfuzz {
namespace module {
Rustreexo::Rustreexo(void) : BaseModule("Rustreexo") {}

std::optional<std::string> Rustreexo::stump_modify_add(
    const std::vector<std::vector<uint8_t>> &add_hashes) const {
  std::vector<uint8_t> flat;
  flat.reserve(add_hashes.size() * 32);
  for (const auto &hash : add_hashes)
    flat.insert(flat.end(), hash.begin(), hash.end());

  char *p = rustreexo_stump_modify(flat.data(), add_hashes.size());
  if (!p)
    return std::nullopt;
  std::string s(p);
  rustreexo_free_string(p);
  return s;
}

} // namespace module
} // namespace bitcoinfuzz
