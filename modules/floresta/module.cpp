#include <span>

#include "module.h"
#include "floresta_lib/floresta_lib.h"

namespace bitcoinfuzz {
namespace module {
Floresta::Floresta(void) : BaseModule("Floresta") {}

std::optional<std::string>
Floresta::addrv2_parse(std::span<const uint8_t> buffer) const {
  auto result_ptr = floresta_addrv2(buffer.data(), buffer.size());
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  floresta_free_c_string(result_ptr);
  return result;
}

} // namespace module
} // namespace bitcoinfuzz
