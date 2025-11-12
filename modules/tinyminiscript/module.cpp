#include <span>
#include "module.h"
#include "./tiny_miniscript_lib/tiny_miniscript_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Tinyminiscript::Tinyminiscript(void) : BaseModule("Tinyminiscript") {}

        std::optional<bool> Tinyminiscript::descriptor_parse(std::string str) const
        {
            return tiny_miniscript_descriptor_parse(str.c_str());
        }
    }
}
