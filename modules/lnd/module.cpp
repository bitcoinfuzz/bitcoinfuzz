#include <span>

#include "module.h"
#include "lnd_wrapper/libscript.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Lnd::Lnd(void) : BaseModule("Lnd") {}

        std::optional<bool> Lnd::deserialize_invoice(std::string str) const
        {
            bool result = LndDeserializeInvoice(str.c_str());
            return result;
        }

    }
}
