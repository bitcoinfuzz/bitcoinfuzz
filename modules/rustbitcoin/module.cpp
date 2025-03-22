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

        std::optional<std::string> Rustbitcoin::deserialize_block(std::span<const uint8_t> buffer) const
        {
            auto pointer{rust_bitcoin_des_block(buffer.data(), buffer.size())};
            if (!pointer) {
                return "0"; // Return "0" for failed deserialization
            }
            std::string result(pointer);
            free_c_string(pointer);
            
            if (result == "unsupported segwit version") {
                return std::string(); // Return empty string for null case
            }
            
            return result; // Return block hash for successful deserialization
        }
    }
}
