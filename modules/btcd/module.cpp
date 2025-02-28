#include <span>

#include "module.h"
#include "btcd_wrapper/libscript.h"

namespace bitcoinfuzz
{
    namespace module
    {
        Btcd::Btcd(void) : BaseModule("Btcd") {}

        std::optional<bool> Btcd::script_eval(const std::vector<uint8_t> &input_data, unsigned int flags, size_t version) const
        {
            ByteArray script_data{
                .data = reinterpret_cast<char *>(const_cast<uint8_t *>(input_data.data())),
                .length = static_cast<int>(input_data.size())};

            return BTCDEvalScript(script_data, /*flags=*/0) == 1;
        }

        std::optional<std::string> Btcd::script_asm(std::span<const uint8_t> buffer) const
        {
            ByteArray script_data{
                .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
                .length = static_cast<int>(buffer.size())};

            const auto script_asm_res = BTCDScriptAsm(script_data);
            if (script_asm_res == "") return std::nullopt;
            std::string res(script_asm_res);
            free(script_asm_res);
            return res;
        }

    } // namespace module
} // namespace bitcoinfuzz
