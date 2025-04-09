#include <span>

#include "module.h"
#include "ldk_lib/ldk_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Ldk::Ldk(void) : BaseModule("Ldk") {}

        std::optional<std::string> Ldk::deserialize_invoice(std::string str) const
        {
            auto result = ldk_des_invoice(str.c_str());
            if (result == nullptr) {
                return std::nullopt;
            }
            std::string result_str(result);
            ldk_free_string(result);
            return result_str;
        }

        std::optional<std::string> Ldk::parse_channel_announcement(const std::vector<uint8_t>& data) const
        {
            if (data.empty()) return std::nullopt;
            
            char* result = ldk_parse_channel_announcement(data.data(), data.size());
            if (result == nullptr) {
                return std::nullopt;
            }
            
            std::string str_result(result);
            ldk_free_string(result);
            
            if (str_result.empty()) return std::nullopt;
            return str_result;
        }

    }
}
