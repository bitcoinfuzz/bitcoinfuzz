#include <span>
#include "module.h"
#include "NLightning/nlightning_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        NLightning::NLightning(void) : BaseModule("NLightning") {}
        std::optional<std::string> NLightning::deserialize_invoice(std::string str) const
        {
            return nlightning_deserialize_invoice(str.c_str());
        }
    }
}
