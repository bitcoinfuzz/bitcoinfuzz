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
            char* resultPtr = nlightning_deserialize_invoice(str.c_str());
            
            if (resultPtr == nullptr) {
                return std::nullopt;
            }

            std::string result(resultPtr);
            nlightning_free_string(resultPtr);
            return result;
        }
    }
}
