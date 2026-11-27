#include "module.h"
#include <jnibridge.h>

namespace bitcoinfuzz {
namespace module {
Eclair::Eclair(void) : BaseModule("Eclair") {}

std::optional<std::string> Eclair::deserialize_invoice(std::string str) const {
  return CallJvm("EclairWrapper", "decodeBolt11Invoice", str);
}

std::optional<std::string> Eclair::deserialize_offer(std::string str) const {
  return CallJvm("EclairWrapper", "decodeOffer", str);
}

std::optional<std::string>
Eclair::deserialize_bolt12_invoice(std::string str) const {
  return CallJvm("EclairWrapper", "decodeBolt12Invoice", str);
}

std::optional<std::string>
Eclair::shachain(std::span<const uint8_t> ops) const {
  return CallJvm("EclairWrapper", "shachain", ops);
}
} // namespace module
} // namespace bitcoinfuzz
