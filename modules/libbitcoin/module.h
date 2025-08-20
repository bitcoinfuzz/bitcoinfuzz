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
        class Libbitcoin : public BaseModule
        {
            public:
                Libbitcoin(void);
                std::optional<std::string> script_parse(std::span<const uint8_t> buffer) const override;
                std::optional<std::string> deserialize_block(std::span<const uint8_t> buffer) const override;
                ~Libbitcoin() noexcept override = default;
        };
    }
}
