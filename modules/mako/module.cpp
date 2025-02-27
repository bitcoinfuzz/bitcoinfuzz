#include <span>

#include "module.h"
#include "types.h"
#include "script.h"
#include "tx.h"


namespace bitcoinfuzz {
namespace module {
Mako::Mako(void) : BaseModule("Mako") {}

std::optional<std::string> Mako::script_parse(std::span<const uint8_t> buffer) const
{
    return std::nullopt;
}

std::optional<std::string> Mako::script_asm(std::span<const uint8_t> buffer) const
{
    btc_script_t script;
    btc_script_init(&script);
    btc_script_set(&script, buffer.data(), buffer.size());
    return btc_script_asm(&script);
}

std::optional<bool> Mako::script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const
{
    btc_script_t script;
    btc_script_init(&script);
    btc_script_set(&script, input_data.data(), input_data.size());

    btc_stack_t stack;
    btc_stack_init(&stack);

    // Verify
    int result = btc_script_execute(&script,
                                    &stack,
                                    flags,
                                    nullptr,
                                    0,
                                    0,
                                    version,
                                    nullptr);  // No cache

    // Cleanup
    btc_script_clear(&script);
    btc_stack_clear(&stack);

    return result == 0;
}

std::optional<std::vector<bool>> Mako::deserialize_block(std::span<const uint8_t> buffer) const
{
    return std::nullopt;
}

} // namespace module
} // namespace bitcoinfuzz
