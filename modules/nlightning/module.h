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
        typedef bool (*DecodeInvoiceFunc)(const char* input);

        class NLightning : public BaseModule
        {
        public:
            NLightning(void);
            std::optional<bool> deserialize_invoice(std::string str) const override;
            ~NLightning() noexcept override = default;

        private:
            static DecodeInvoiceFunc decodeInvoice;
        };

    }
}