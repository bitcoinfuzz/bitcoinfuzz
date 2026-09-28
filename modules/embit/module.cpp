#include "module.h"
#include <bitcoinfuzz/result.h>
#include <pybridge.h>
#include <span>
#include <string>
#include <string_view>

namespace {
// Cuts the text at its first NUL, as the modules that take a C string do.
std::span<const uint8_t> UntilNul(const std::string &str) {
  const std::string_view text{str.c_str()};
  return {reinterpret_cast<const uint8_t *>(text.data()), text.size()};
}
} // namespace

namespace bitcoinfuzz {
namespace module {
Embit::Embit(void) : BaseModule("Embit") {}

std::optional<bool> Embit::miniscript_parse(std::string str) const {
  return CallPythonBool("embit", "miniscript_parse", UntilNul(str));
}

std::optional<bool> Embit::descriptor_parse(std::string str) const {
  // Skip these fragments since Embit hasn't implemented them
  const std::vector<std::string> fragments = {"combo(", "thresh(", "raw(",
                                              "rawtr(", "pk("};
  for (const auto &fragment : fragments) {
    if (str.find(fragment) != std::string::npos) {
      return Skip();
    }
  }
  return CallPythonBool("embit", "descriptor_parse", UntilNul(str));
}

std::optional<std::string>
Embit::psbt_v0_parse(std::span<const uint8_t> buffer) const {
  return CallPython("embit", "psbt_v0_parse", buffer);
}

std::optional<std::string>
Embit::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return CallPython("embit", "bip32_master_keygen", buffer);
}

std::optional<std::string>
Embit::bip32_deserialize_extended_key(std::span<const uint8_t> buffer) const {
  return CallPython("embit", "bip32_deserialize_extended_key", buffer);
}

} // namespace module
} // namespace bitcoinfuzz
