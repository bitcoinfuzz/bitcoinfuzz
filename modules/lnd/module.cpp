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

        std::optional<std::string> Lnd::construct_htlc_success_tx(std::string commitment_tx_hex, uint32_t htlc_index, std::string preimage, uint64_t fee_rate) const
        {
            auto result = LndConstructHtlcSuccessTx(
                commitment_tx_hex.c_str(),
                htlc_index,
                preimage.c_str(),
                fee_rate);
            
            if (result == nullptr) {
                return std::nullopt;
            }
            
            std::string result_str(result);
            free(result);
            return result_str;
        }

        std::optional<std::string> Lnd::construct_htlc_timeout_tx(std::string commitment_tx_hex, uint32_t htlc_index, uint32_t cltv_expiry, uint64_t fee_rate) const
        {
            auto result = LndConstructHtlcTimeoutTx(
                commitment_tx_hex.c_str(),
                htlc_index,
                cltv_expiry,
                fee_rate);
            
            if (result == nullptr) {
                return std::nullopt;
            }
            
            std::string result_str(result);
            free(result);
            return result_str;
        }
    }
}
