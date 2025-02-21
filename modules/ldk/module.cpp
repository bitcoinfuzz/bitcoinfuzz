#include <span>

#include "module.h"

extern "C" char *ldk_des_invoice(const uint8_t *data, size_t len);

namespace bitcoinfuzz
{
    namespace module
    {
        Ldk::Ldk(void) : BaseModule("Ldk") {}

        std::optional<std::string> Ldk::deserialize_invoice(std::span<const uint8_t> buffer) const
        {
            char *result = ldk_des_invoice(buffer.data(), buffer.size());
            if (result == nullptr)
            {
                return std::nullopt;
            }
            std::string invoice_str{result};
            free(result);
            return invoice_str;
        }

    }
}
