#include "module.h"
#include "NLightning/nlightning_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {
NLightning::NLightning(void) : BaseModule("NLightning") {}
std::optional<std::string>
NLightning::deserialize_invoice(std::string str) const {
  return TakeResult(nlightning_deserialize_invoice(str.c_str()));
}
} // namespace module
} // namespace bitcoinfuzz
