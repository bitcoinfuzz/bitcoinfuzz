#include <span>

#include "module.h"
#include "rust_bitcoin_lib/rust_bitcoin_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Rustbitcoin::Rustbitcoin(void) : BaseModule("Rustbitcoin") {}

        std::optional<std::string> Rustbitcoin::script_parse(std::span<const uint8_t> buffer) const
        {
            auto script{rust_bitcoin_script(buffer.data(), buffer.size())};
            std::string result(script);
            free_c_string(script);
            return result;
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
