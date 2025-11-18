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
        class Tinyminiscript : public BaseModule
        {
        public:
            Tinyminiscript(void);
            std::optional<bool> descriptor_parse(std::string str) const override;
            ~Tinyminiscript() noexcept override = default;
        };

    }
}
