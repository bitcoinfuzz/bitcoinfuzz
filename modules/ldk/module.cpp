#include <span>

#include "ldk_lib/ldk_lib.h"
#include "module.h"

namespace bitcoinfuzz {
namespace module {
Ldk::Ldk(void) : BaseModule("Ldk") {}

std::optional<std::string> Ldk::deserialize_invoice(std::string str) const {
  return TakeResult(ldk_des_invoice(str.c_str()));
}

std::optional<std::string> Ldk::deserialize_offer(std::string str) const {
  return TakeResult(ldk_des_offer(str.c_str()));
}

std::optional<std::string>
Ldk::parse_p2p_lightning_message(std::span<const uint8_t> buffer) const {
  return TakeResult(
      ldk_parse_p2p_lightning_message(buffer.data(), buffer.size()));
}

std::optional<std::string>
Ldk::decode_onion(std::span<const uint8_t> buffer) const {
  return TakeResult(ldk_decode_onion(buffer.data(), buffer.size()));
}
} // namespace module
} // namespace bitcoinfuzz
