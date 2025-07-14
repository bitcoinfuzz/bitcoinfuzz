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
        class Ldk : public BaseModule
        {
        public:
            Ldk(void);
            std::optional<std::string> deserialize_invoice(std::string str) const override;
            std::optional<std::string> deserialize_offer(std::string str) const override;
            std::optional<std::string> parse_p2p_lightning_message(std::span<const uint8_t> buffer) const override;
            ~Ldk() noexcept override = default;
        };

    }
}
