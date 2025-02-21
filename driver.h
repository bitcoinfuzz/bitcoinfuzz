#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <vector>
#include <utility>

#include <bitcoinfuzz/basemodule.h>

namespace bitcoinfuzz
{
    class Driver
    {
    private:
        std::map<std::string, std::shared_ptr<BaseModule>> modules;

    public:
        void LoadModule(std::shared_ptr<BaseModule> module);
        void ScriptTarget(std::span<const uint8_t>) const;
        void ScriptEvalTarget(std::span<const uint8_t> buffer) const;
        void BlockDeserializationTarget(std::span<const uint8_t> buffer) const;
        void DescriptorParseTarget(std::span<const uint8_t> buffer) const;
        void InvoiceDeserializationTarget(std::span<const uint8_t> buffer) const;
        void Run(const uint8_t *data, const size_t size, const std::string &target) const;
    };
}
