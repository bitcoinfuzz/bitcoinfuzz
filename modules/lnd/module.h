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
            std::optional<std::string> parse_p2p_lightning_message(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> decode_legacy_onion(std::span<const uint8_t> buffer) const override;
            ~Lnd() noexcept override = default;
        };

    }
}