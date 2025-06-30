#include <span>
#include "module.h"
#include "NBitcoin/nbitcoin_lib.h"

namespace bitcoinfuzz
{
    namespace module
    {
        NBitcoin::NBitcoin(void) : BaseModule("NBitcoin") {}
        std::optional<bool> NBitcoin::miniscript_parse(std::string str) const
        {
            return nbitcoin_miniscript_parse(str.c_str());
        }
        std::optional<bool> NBitcoin::descriptor_parse(std::string str) const
        {
            return nbitcoin_descriptor_parse(str.c_str());
        }
    }
}
