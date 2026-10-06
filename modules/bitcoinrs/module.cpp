#include <span>

#include "bitcoin_rs_lib/bitcoin_rs_lib.h"
#include "module.h"

namespace bitcoinfuzz {
namespace module {
BitcoinRs::BitcoinRs(void) : BaseModule("BitcoinRs") {}

std::optional<std::string>
BitcoinRs::deserialize_block(std::span<const uint8_t> buffer) const {
  auto pointer{bitcoin_rs_des_block(buffer.data(), buffer.size())};
  if (pointer == nullptr)
    return std::nullopt;
  std::string result(pointer);
  bitcoin_rs_free_c_string(pointer);
  if (result == "skip error") {
    return std::nullopt;
  }
  return result;
}
} // namespace module
} // namespace bitcoinfuzz
