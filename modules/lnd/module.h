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
        class Lnd : public BaseModule
        {
        public:
            Lnd(void);
            std::optional<bool> deserialize_invoice(std::string str) const override;
            ~Lnd() noexcept override = default;
        };

    }
}