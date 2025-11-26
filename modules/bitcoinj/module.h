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
        class BitcoinJ : public BaseModule
        {
        public:
            BitcoinJ(void);
            std::optional<std::string> bip32_master_keygen(std::span<const uint8_t> buffer) const override;
            ~BitcoinJ() noexcept override = default;
        };
    }
}