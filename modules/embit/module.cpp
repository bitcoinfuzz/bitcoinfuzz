#include <span>

#include "module.h"
#include "embit_lib/embit_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Embit::Embit(void) : BaseModule("Embit") {}

        std::optional<bool> Embit::miniscript_parse(std::string str) const
        {
            return embit_miniscript_parse(str.c_str());
        }

        std::optional<bool> Embit::descriptor_parse(std::string str) const
        {
            // Skip these fragments since Embit hasn't implemented them
            const std::vector<std::string> fragments = {"combo(", "thresh(", "raw(", "rawtr("};
            for (const auto& fragment : fragments) {
                if (str.find(fragment) != std::string::npos) {
                    return std::nullopt;
                }
            }
            return embit_descriptor_parse(str.c_str());
        }

        std::optional<std::string> Embit::psbt_parse(std::span<const uint8_t> buffer) const 
        {
            auto result_ptr = embit_psbt_parse(buffer.data(), buffer.size());
            if (result_ptr == nullptr) return std::nullopt;
        
            std::string result(result_ptr);
            free(result_ptr);
            return result;
        }
    }
}
