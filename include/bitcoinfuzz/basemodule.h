#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace bitcoinfuzz
{
    class BaseModule
    {
    public:
        const std::string name;

        BaseModule(const std::string &name) : name(name) {}

        virtual std::optional<std::string> script_parse(std::span<const uint8_t> buffer) const;
        virtual std::optional<std::string> deserialize_block(std::span<const uint8_t> buffer) const;
        virtual std::optional<bool> script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const;
        virtual std::optional<bool> descriptor_parse(std::string str) const;
        virtual std::optional<bool> miniscript_parse(std::string str) const;
        virtual std::optional<std::string> script_asm(std::span<const uint8_t> buffer) const;
        virtual std::optional<std::string> deserialize_invoice(std::string str) const;
        virtual std::optional<std::string> address_parse(std::string str) const;
        virtual std::optional<std::string> psbt_parse(std::span<const uint8_t> buffer) const;
        virtual std::optional<std::string> addrv2_parse(std::span<const uint8_t> buffer) const;
        virtual std::optional<std::string> deserialize_offer(std::string str) const;
        virtual std::optional<int> cmpctblocks_parse(std::span<const uint8_t> buffer) const;
        virtual ~BaseModule() noexcept;
    };
}
