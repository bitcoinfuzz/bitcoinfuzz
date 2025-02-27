#include <cassert>
#include <set>
#include <algorithm>
#include <unistd.h>
#include <string>
#include <span>
#include <fuzzer/FuzzedDataProvider.h>

#include "driver.h"
#include <bitcoinfuzz/basemodule.h>

namespace bitcoinfuzz
{
    void Driver::LoadModule(std::shared_ptr<BaseModule> module)
    {
        modules[module->name] = module;
    }

    void Driver::ScriptTarget(std::span<const uint8_t> buffer) const
    {
        std::optional<std::string> last_response{std::nullopt};
        for (auto& module : modules)
        {
            std::optional<std::string> res{module.second->script_parse(buffer)};
            if (!res.has_value()) continue;
            if (last_response.has_value()) assert(*res == *last_response);
            last_response = *res;
        }
    }

    void Driver::BlockDeserializationTarget(std::span<const uint8_t> buffer) const
    {
        std::optional<std::vector<bool>> last_response{std::nullopt};
        for (auto& module : modules)
        {
            std::optional<std::vector<bool>> res{module.second->deserialize_block(buffer)};
            if (!res.has_value() || res->empty()) continue;
            if (last_response.has_value()) assert(*res == *last_response);
            last_response = res.value();
        }
    }

    void Driver::ScriptEvalTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        std::vector<uint8_t> input_data = provider.ConsumeBytes<uint8_t>(
            provider.ConsumeIntegralInRange<size_t>(0, 1024)
        );

        auto flags = provider.ConsumeIntegral<unsigned int>();

        std::optional<bool> last_response{std::nullopt};
        for (auto& module : modules)
        {
            std::optional<bool> res{module.second->script_eval(input_data, flags, /*version=*/0)};
            if (!res.has_value()) continue;
            if (last_response.has_value()) assert(*res == *last_response);
            last_response = *res;
        }
    }

    void Driver::DescriptorParseTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        std::string desc{provider.ConsumeRemainingBytesAsString()};
        std::optional<bool> last_response{std::nullopt};
        for (auto& module : modules)
        {
            std::optional<bool> res{module.second->descriptor_parse(desc)};
            if (!res.has_value()) continue;
            if (last_response.has_value()) assert(*res == *last_response);
            last_response = *res;
        }
    }

    void Driver::MiniscriptParseTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        std::string miniscript{provider.ConsumeRemainingBytesAsString()};
        // Skip these cases
        if (miniscript == "1" || miniscript == "0") return;
        std::optional<bool> last_response{std::nullopt};
        for (auto& module : modules)
        {
            std::optional<bool> res{module.second->miniscript_parse(miniscript)};
            if (!res.has_value()) continue;
            if (last_response.has_value()) assert(*res == *last_response);
            last_response = *res;
        }
    }

    void Driver::ScriptAsmTarget(std::span<const uint8_t> buffer) const
    {
        std::optional<std::string> last_response{std::nullopt};
        for (auto& module : modules)
        {
            std::optional<std::string> res{module.second->script_asm(buffer)};
            if (!res.has_value()) continue;
            if (last_response.has_value()) {
                assert(*res == *last_response);
            }
            last_response = *res;
        }
    }

    void Driver::Run(const uint8_t *data, const size_t size, const std::string &target) const
    {
        std::span<const uint8_t> buffer{data, size};
        if (target == "script") {
            this->ScriptTarget(buffer);
        } else if (target == "block_deserialization") {
            this->BlockDeserializationTarget(buffer);
        } else if (target == "script_eval") {
            this->ScriptEvalTarget(buffer);
        } else if (target == "descriptor_parse") {
            this->DescriptorParseTarget(buffer);
        } else if (target == "miniscript_parse") {
	        this->MiniscriptParseTarget(buffer);
        } else if (target == "script_asm") {
            this->ScriptAsmTarget(buffer);
	    } else {
            std::cout << "Target not defined!" << std::endl;
            assert(false);
        }
    };

}
