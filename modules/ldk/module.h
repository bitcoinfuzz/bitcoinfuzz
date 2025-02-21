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
            std::optional<std::string> deserialize_invoice(std::span<const uint8_t> buffer) const override;
            ~Ldk() noexcept override = default;
        };

    }
}
