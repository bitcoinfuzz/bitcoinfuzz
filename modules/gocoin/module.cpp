#include <cstring>
#include <span>

#include "gocoin_wrapper/libgocoin_wrapper.h"
#include "module.h"

namespace bitcoinfuzz {
namespace module {
Gocoin::Gocoin(void) : BaseModule("Gocoin") {}

std::optional<bool>
Gocoin::verify_script(const std::vector<uint8_t> &script_sig,
                      const std::vector<uint8_t> &script_pubkey) const {
  ByteArray script_data{.data = reinterpret_cast<char *>(
                            const_cast<uint8_t *>(script_sig.data())),
                        .length = static_cast<int>(script_sig.size())};

  ByteArray script_data2{.data = reinterpret_cast<char *>(
                             const_cast<uint8_t *>(script_pubkey.data())),
                         .length = static_cast<int>(script_pubkey.size())};

  return GocoinVerifyTxScript(script_data, script_data2) == 1;
}

std::optional<bool> Gocoin::script_eval(const std::vector<uint8_t> &input_data,
                                        unsigned int flags,
                                        size_t version) const {
  ByteArray script_data{.data = reinterpret_cast<char *>(
                            const_cast<uint8_t *>(input_data.data())),
                        .length = static_cast<int>(input_data.size())};

  int result = GocoinEvalScript(script_data, /*flags=*/0, version);

  if (result == 1) {
    return true;
  } else if (result == 2) {
    return false;
  }

  return std::nullopt;
}

} // namespace module
} // namespace bitcoinfuzz
