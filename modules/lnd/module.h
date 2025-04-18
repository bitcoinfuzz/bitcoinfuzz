#include <optional>
#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <bitcoinfuzz/basemodule.h>

namespace bitcoinfuzz
{
    namespace module
    {
        class Lnd : public BaseModule
        {
        public:
            Lnd(void);
            std::optional<std::string> deserialize_invoice(std::string str) const override;
            std::optional<std::string> construct_htlc_success_tx(std::string commitment_tx_hex, uint32_t htlc_index, std::string preimage, uint64_t fee_rate) const override;
            std::optional<std::string> construct_htlc_timeout_tx(std::string commitment_tx_hex, uint32_t htlc_index, uint32_t cltv_expiry, uint64_t fee_rate) const override;

            ~Lnd() noexcept override = default;
        };

    }
}