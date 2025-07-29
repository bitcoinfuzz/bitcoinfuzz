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
        class Eclair : public BaseModule
        {
        public:
            Eclair(void);
            std::optional<std::string> deserialize_invoice(std::string str) const override;
            std::optional<std::string> deserialize_offer(std::string str) const override;
            ~Eclair() noexcept override = default;
        };

    }
}