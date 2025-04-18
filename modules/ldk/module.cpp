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

        std::optional<std::string> Ldk::construct_htlc_success_tx(std::string commitment_tx_hex, uint32_t htlc_index, std::string preimage, uint64_t fee_rate) const
        {
            auto result = ldk_construct_htlc_success_tx(
                commitment_tx_hex.c_str(),
                htlc_index,
                preimage.c_str(),
                fee_rate);
            
            if (result == nullptr) {
                return std::nullopt;
            }
            
            std::string result_str(result);
            ldk_free_string(result);
            return result_str;
        }
        
        std::optional<std::string> Ldk::construct_htlc_timeout_tx(std::string commitment_tx_hex, uint32_t htlc_index, uint32_t cltv_expiry, uint64_t fee_rate) const
        {
            auto result = ldk_construct_htlc_timeout_tx(
                commitment_tx_hex.c_str(),
                htlc_index,
                cltv_expiry,
                fee_rate);
            
            if (result == nullptr) {
                return std::nullopt;
            }
            
            std::string result_str(result);
            ldk_free_string(result);
            return result_str;
        }

    }
}
