#include <span>

#include "module.h"
#include "embit_lib/embit_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Embit::Embit(void) : BaseModule("Embit") {}

        std::optional<bool> Embit::miniscript_parse(std::string str) const
        {
            return embit_miniscript_parse(str.c_str());
        }

        std::optional<bool> Embit::descriptor_parse(std::string str) const
        {
            return embit_descriptor_parse(str.c_str());
        }
    }
}
