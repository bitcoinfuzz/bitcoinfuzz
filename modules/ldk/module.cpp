#include <span>

#include "module.h"

extern "C" bool ldk_des_invoice(const char* input);

namespace bitcoinfuzz
{
    namespace module
    {
        Ldk::Ldk(void) : BaseModule("Ldk") {}

        std::optional<bool> Ldk::deserialize_invoice(std::string str) const
        {
            bool result = ldk_des_invoice(str.c_str());
            return result;
        }

    }
}
