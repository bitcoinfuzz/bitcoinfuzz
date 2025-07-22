#include <optional>
#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <bitcoinfuzz/basemodule.h>

namespace bitcoinfuzz
{
    namespace module
    {
        class Rustbitcoin : public BaseModule
        {
        public:
            Rustbitcoin(void);
            std::optional<std::string> script_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> deserialize_block(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> address_parse(std::string str) const override;
            std::optional<std::string> psbt_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> addrv2_parse(std::span<const uint8_t> buffer) const override;
            std::optional<uint32_t> cmpctblocks_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> parse_p2p_message(std::span<const uint8_t> buffer) const override;
            ~Rustbitcoin() noexcept override = default;
        };

    }
}
