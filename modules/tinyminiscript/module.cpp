#include "module.h"
#include "./tiny_miniscript_lib/tiny_miniscript_lib.h"
#include <bitcoinfuzz/result.h>
#include <span>

namespace bitcoinfuzz {
namespace module {
Tinyminiscript::Tinyminiscript(void) : BaseModule("Tinyminiscript") {}

std::optional<bool> Tinyminiscript::descriptor_parse(std::string str) const {
  // Tinyminiscript does not implement raw script descriptors.
  if (str.starts_with("raw("))
    return Skip();
  // Tinyminiscript does not support Taproot satisfaction checks.
  if (str.find("tr(") != std::string::npos)
    return Skip();
  return tiny_miniscript_descriptor_parse(str.c_str());
}
} // namespace module
} // namespace bitcoinfuzz
