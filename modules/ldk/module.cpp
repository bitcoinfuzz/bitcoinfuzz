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

        std::optional<std::string> Ldk::deserialize_offer(std::string str) const
        {
            auto result = ldk_des_offer(str.c_str());
            if (result == nullptr) {
                return std::nullopt;
            }
            std::string result_str(result);
            ldk_free_string(result);
            return result_str;
        }

        std::optional<std::string> Ldk::parse_p2p_lightning_message(std::span<const uint8_t> buffer) const
        {
            auto result = ldk_parse_p2p_lightning_message(buffer.data(), buffer.size());
            if (result == nullptr) {
                return std::nullopt;
            }
            std::string result_str(result);
            ldk_free_string(result);
            return result_str;
        }
    }
}
