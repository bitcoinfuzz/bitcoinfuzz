#include <optional>
#include <span>
#include <iostream>

#include "module.h"
#include <bitcoin/system.hpp>


using namespace bc;

namespace bitcoinfuzz {
namespace module {
Libbitcoin::Libbitcoin(void) : BaseModule("Libbitcoin") {}

template<typename T>
T span_to_data(std::span<const uint8_t> buffer) {
    // Convert std::span to libbitcoin's data_chunk type
    return T(buffer.begin(), buffer.end());
}

std::optional<std::string> Libbitcoin::script_parse(std::span<const uint8_t> buffer) const {
    try {
        const data_chunk script_data = span_to_data<data_chunk>(buffer);
        
        if (script_data.empty()) {
            return "0";
        }
        
        const chain::script script(script_data, false); // Match Bitcoin Core's less strict parsing
        
        // Check for unspendable scripts first
        if (script.is_unspendable()) {
            return "0";
        }
        
        // Get operations once and reuse
        auto ops = script.operations();
        
        // Match Bitcoin Core's SigOps counting method
        auto sigops_count = script.sigops(false);
        
        // Witness program detection
        bool is_witness_program = script.is_witness_program_pattern(ops);
        
        // Push-only check
        bool is_push_only = script.is_push_only(ops);
        
        std::string result;
        result.reserve(16); // Pre-allocate to avoid reallocations
        result += std::to_string(sigops_count);
        result += (is_witness_program ? '1' : '0');
        result += (is_push_only ? '1' : '0');
        
        return result;
        
    } catch (const std::exception& e) {
        return "0";
    }
}


std::optional<std::string> Libbitcoin::deserialize_block(std::span<const uint8_t> buffer) const {
    const data_chunk block_data = span_to_data<data_chunk>(buffer);
    chain::block block;
    
    if (!block.from_data(block_data, true)) {
        return std::nullopt;
    }
    
    if (block.transactions().empty()) {
        return "0";
    }
    
    // libbitcoin has check() method which performs all the mutation checks
    code validation_result = block.check();
    if (validation_result != error::success) {
        return "0";
    }
    
    return encode_hash(block.hash());
}

} // namespace module
} // namespace bitcoinfuzz
