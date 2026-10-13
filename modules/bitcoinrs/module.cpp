#include <span>

#include "bitcoin_rs_lib/bitcoin_rs_lib.h"
#include "module.h"

namespace bitcoinfuzz {
namespace module {
BitcoinRs::BitcoinRs(void) : BaseModule("BitcoinRs") {}

std::optional<std::string>
BitcoinRs::deserialize_block(std::span<const uint8_t> buffer) const {
  return TakeResult(bitcoin_rs_des_block(buffer.data(), buffer.size()));
}
} // namespace module
} // namespace bitcoinfuzz
