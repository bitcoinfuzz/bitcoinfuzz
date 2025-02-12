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
        class Bitcoin : public BaseModule
        {
        public:
            Bitcoin(void);
            std::optional<bool> script_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::vector<bool>> deserialize_block(std::span<const uint8_t> buffer) const override;
            std::optional<bool> script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const override;
            std::optional<bool> descriptor_parse(std::string str) const override;
            ~Bitcoin() noexcept override = default;
        };

    }
}
