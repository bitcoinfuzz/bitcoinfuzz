#include <optional>
#include <span>
#include <bitcoinfuzz/basemodule.h>


namespace bitcoinfuzz
{
    namespace module
    {
        class Mako : public BaseModule
        {
        public:
            Mako(void);
            std::optional<std::string> script_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::vector<bool>> deserialize_block(std::span<const uint8_t> buffer) const override;
            std::optional<bool> script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const override;
            std::optional<std::string> script_asm(std::span<const uint8_t> buffer) const override;
            ~Mako() noexcept override = default;
        };

    }
}
