#include "module.h"
#include <jnibridge.h>

namespace bitcoinfuzz {
namespace module {
LightningKmp::LightningKmp(void) : BaseModule("LightningKmp") {}

std::optional<std::string>
LightningKmp::deserialize_invoice(std::string str) const {
  return CallJvm("lightningkmp/Wrapper", "decodeBolt11Invoice", str);
}

std::optional<std::string>
LightningKmp::deserialize_offer(std::string str) const {
  return CallJvm("lightningkmp/Wrapper", "decodeBolt12Offer", str);
}
} // namespace module
} // namespace bitcoinfuzz
