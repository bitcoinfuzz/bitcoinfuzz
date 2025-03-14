#include "span"
#include "module.h"
#include "rust_modules_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        RustModules::RustModules(void) : BaseModule("RustModules") {}

        // LDK functionality
        std::optional<bool> RustModules::deserialize_invoice(std::string str) const
        {
            bool result = ldk_des_invoice(str.c_str());
            return result;
        }
        
        // Rust Bitcoin functionality
        std::optional<std::string> RustModules::script_parse(std::span<const uint8_t> buffer) const
        {
            auto script{rust_bitcoin_script(buffer.data(), buffer.size())};
            std::string result(script);
            free_c_string(script);
            return result;
        }

        std::optional<std::vector<bool>> RustModules::deserialize_block(std::span<const uint8_t> buffer) const
        {
            std::string result{rust_bitcoin_des_block(buffer.data(), buffer.size())};
            if (result == "unsupported segwit version") {
                return std::nullopt;
            }
            std::vector<bool> final_result{"true" == result};
            return final_result;
        }
        
        // Miniscript functionality
        std::optional<bool> RustModules::descriptor_parse(std::string str) const
        {
            // Skip some descriptors
            if ((str.find("raw") != std::string::npos) || (str.find("combo") != std::string::npos)) return std::nullopt;
            return rust_miniscript_descriptor_parse(str.c_str());
        }

        std::optional<bool> RustModules::miniscript_parse(std::string str) const
        {
            return rust_miniscript_miniscript_parse(str.c_str());
        }
    }
}