#ifndef RUST_MODULES_H
#define RUST_MODULES_H

#include <optional>
#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <string>
#include <bitcoinfuzz/basemodule.h>

namespace bitcoinfuzz
{
    namespace module
    {
        class RustModules : public BaseModule
        {
        public:
            RustModules(void);
            
            // LDK functionality
            std::optional<bool> deserialize_invoice(std::string str) const override;
            
            // Rust Bitcoin functionality
            std::optional<std::string> script_parse(std::span<const uint8_t> buffer) const override;
            std::optional<std::vector<bool>> deserialize_block(std::span<const uint8_t> buffer) const override;
            
            // Miniscript functionality
            std::optional<bool> descriptor_parse(std::string str) const override;
            std::optional<bool> miniscript_parse(std::string str) const override;
            
            ~RustModules() noexcept override = default;
        };
    }
}

#endif // RUST_MODULES_H