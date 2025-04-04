#include <iostream>
#include <span>

#include "module.h"
#include "clightning_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        CLightning::CLightning(void) : BaseModule("CLightning") {}

        std::optional<std::string> CLightning::deserialize_invoice(std::string str) const
        {
            auto result = clightning_des_invoice(str.c_str());
            if (result == nullptr) {
                   return std::nullopt;
            }
            std::string result_str(result);
            free(result);
            return result_str;
        }
    }
}
