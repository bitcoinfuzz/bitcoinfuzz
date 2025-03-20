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
            ~Embit() noexcept override = default;
        };

    }
}
