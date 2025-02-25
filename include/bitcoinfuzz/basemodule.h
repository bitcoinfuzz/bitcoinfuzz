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

        virtual std::optional<bool> script_parse(std::span<const uint8_t> buffer) const;
        virtual std::optional<std::vector<bool>> deserialize_block(std::span<const uint8_t> buffer) const;
        virtual std::optional<bool> script_eval(const std::vector<uint8_t>& input_data, unsigned int flags, size_t version) const;
        virtual std::optional<bool> descriptor_parse(std::string str) const;
        virtual std::optional<bool> miniscript_parse(std::string str) const;
        virtual ~BaseModule() noexcept;
    };
}
