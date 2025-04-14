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
        class Embit : public BaseModule
        {
        public:
            Embit(void);
            std::optional<bool> miniscript_parse(std::string str) const override;
            std::optional<bool> descriptor_parse(std::string str) const override;
            std::optional<std::string> psbt_parse(std::span<const uint8_t> buffer) const override;
            ~Embit() noexcept override = default;
        };

    }
}