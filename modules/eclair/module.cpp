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
} // namespace module
} // namespace bitcoinfuzz
