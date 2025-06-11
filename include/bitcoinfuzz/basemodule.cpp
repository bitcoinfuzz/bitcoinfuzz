#include "basemodule.h"
#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace bitcoinfuzz
{
    BaseModule::~BaseModule() noexcept {} // Ensures vtable for `Module` is created

    std::optional<std::string> BaseModule::script_parse(std::span<const uint8_t> buffer) const
    {
        return std::nullopt;
    }

    std::optional<std::vector<bool>> BaseModule::deserialize_block(std::span<const uint8_t> buffer) const
    {
        return std::nullopt;
    }

    std::optional<bool> BaseModule::script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const
    {
        return std::nullopt;
    }

    std::optional<bool> BaseModule::descriptor_parse(std::string str) const
    {
        return std::nullopt;
    }

    std::optional<bool> BaseModule::miniscript_parse(std::string str) const
    {
        return std::nullopt;
    }

    std::optional<std::string> BaseModule::script_asm(std::span<const uint8_t> buffer) const
    {
        return std::nullopt;
    }

    std::optional<std::string> BaseModule::deserialize_invoice(std::string str) const
    {
        return std::nullopt;
    }

    std::optional<std::string> BaseModule::address_parse(std::string str) const
    {
        return std::nullopt;
    }

    std::optional<std::string> BaseModule::psbt_parse(std::span<const uint8_t> buffer) const
    {
        return std::nullopt;
    }

    std::optional<std::string> BaseModule::addrv2_parse(std::span<const uint8_t> buffer) const
    {
        return std::nullopt;
    }
    
    std::optional<std::string> BaseModule::deserialize_offer(std::string str) const
    {
        return std::nullopt;
    }

    std::optional<std::string> BaseModule::deserialize_invoice_request(std::string str) const
    {
        return std::nullopt;
    }
}
