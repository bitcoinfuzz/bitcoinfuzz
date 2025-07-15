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
        class Btcd : public BaseModule
        {
        public:
            Btcd(void);
            std::optional<bool> script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const override;
            std::optional<std::string> script_asm(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> deserialize_block(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> address_parse(std::string str) const override;
            std::optional<std::string> psbt_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::string> addrv2_parse(std::span<const uint8_t> buffer) const override;
            ~Btcd() noexcept override = default;
        };

    }
}
