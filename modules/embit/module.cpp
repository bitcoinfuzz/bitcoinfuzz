#include <span>

#include "module.h"

extern "C" bool embit_miniscript_miniscript_parse(const char* input);

namespace bitcoinfuzz
{
    namespace module
    {
        Embit::Embit(void) : BaseModule("Embit") {}

        std::optional<bool> Embit::miniscript_parse(std::string str) const
        {
            bool result = embit_miniscript_miniscript_parse(str.c_str());
            return result;
        }

    }
}
