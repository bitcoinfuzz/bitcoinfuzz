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

        std::optional<std::string> Ldk::deserialize_invoice_request(std::string str) const
        {
            auto result = ldk_des_invoice_request(str.c_str());
            if (result == nullptr) {
                return std::nullopt;
            }
            std::string result_str(result);
            ldk_free_string(result);
            return result_str;
        }
    }
}
