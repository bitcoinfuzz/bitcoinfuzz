#include <span>

#include "module.h"
#include "lnd_wrapper/libscript.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Lnd::Lnd(void) : BaseModule("Lnd") {}

        std::optional<std::string> Lnd::deserialize_invoice(std::string str) const
        {
            auto result = LndDeserializeInvoice(str.c_str());
            std::string result_str(result);
            free(result);
            return result_str;
        }

        std::optional<std::string> Lnd::parse_p2p_lightning_message(std::span<const uint8_t> buffer) const
        {
            auto result = LndParseP2pLightningMessage(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            if (result == nullptr) {
                return std::nullopt;
            }
            std::string result_str(result);
            free(result);
            return result_str;
        }
    }
}
