#include <span>

#include "module.h"

extern "C" bool rust_bitcoin_script(const uint8_t *data, size_t len);
extern "C" char* rust_bitcoin_des_block(const uint8_t *data, size_t len);
extern "C" char* rust_bitcoin_script_asm(const char* input);

namespace bitcoinfuzz
{
    namespace module
    {
        Rustbitcoin::Rustbitcoin(void) : BaseModule("Rustbitcoin") {}

        std::optional<bool> Rustbitcoin::script_parse(std::span<const uint8_t> buffer) const
        {
            return rust_bitcoin_script(buffer.data(), buffer.size());
        }

        std::optional<std::vector<bool>> Rustbitcoin::deserialize_block(std::span<const uint8_t> buffer) const
        {
            std::string result{rust_bitcoin_des_block(buffer.data(), buffer.size())};
            if (result == "unsupported segwit version") {
                return std::nullopt;
            }
            std::vector<bool> final_result{"true" == result};
            return final_result;
        }
    }
}
