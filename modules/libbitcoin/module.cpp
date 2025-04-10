#include "module.h"
#include <bitcoin/system.hpp>
#include <span>

namespace bitcoinfuzz {
    namespace module {

        Libbitcoin::Libbitcoin(void) : BaseModule("Libbitcoin") {}

        std::optional<std::string> Libbitcoin::script_parse(std::span<const uint8_t> buffer) const
        {
            try {
                bc::system::data_chunk data(buffer.begin(), buffer.end());
            
                bc::system::chain::script script;
                
                script = bc::system::chain::script(data, false);
                
                if (!script.is_valid())
                    return "INVALID";

                return script.to_string(0);
            }
            catch (const std::exception&) {
                return std::nullopt;
            }
        }
    } // namespace module
} // namespace bitcoinfuzz