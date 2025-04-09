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

        std::optional<std::string> Lnd::parse_channel_announcement(const std::vector<uint8_t>& data) const
        {
            if (data.empty()) 
                return std::nullopt;
            
            char* result = LNDParseChannelAnnouncement(const_cast<void*>(static_cast<const void*>(data.data())), data.size());
            if (result == nullptr) 
                return std::nullopt;
            
            std::string str_result(result);
            free(result);
            
            if (str_result.empty())
                return std::nullopt;
            return str_result;
        }
    }
}
