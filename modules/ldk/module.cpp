#include <span>

#include "module.h"

extern "C" char *ldk_des_invoice(const uint8_t *data, size_t len);

namespace bitcoinfuzz
{
    namespace module
    {
        Ldk::Ldk(void) : BaseModule("Ldk") {}

        std::optional<std::vector<std::string>> Ldk::des_invoice(std::span<const uint8_t> buffer) const
        {
            std::string result{ldk_des_invoice(buffer.data(), buffer.size())};
            return result;
        }

    }
}
