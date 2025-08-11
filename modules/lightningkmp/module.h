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
        class LightningKmp : public BaseModule
        {
        public:
            LightningKmp(void);
            std::optional<std::string> deserialize_invoice(std::string str) const override;
            ~LightningKmp() noexcept override = default;
        };
    }
}