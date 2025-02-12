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
        class Rustminiscript : public BaseModule
        {
        public:
            Rustminiscript(void);
            std::optional<bool> descriptor_parse(std::string str) const override;
            ~Rustminiscript() noexcept override = default;
        };

    }
}
