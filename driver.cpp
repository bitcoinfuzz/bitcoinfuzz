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

    void Driver::InvoiceDeserializationTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        std::string invoice{provider.ConsumeRemainingBytesAsString()};
        std::optional<std::string> last_response{std::nullopt};
        for (auto &module : modules)
        {
            std::optional<std::string> res{module.second->deserialize_invoice(invoice)};
            if (!res.has_value()) continue;
            if (last_response.has_value()) {
                assert(*res == *last_response);
            }

            last_response = res.value();
        }
    }

    void Driver::AddressParseTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        std::string address{provider.ConsumeRemainingBytesAsString()};

        std::optional<std::string> last_response{std::nullopt};
        std::string last_module_name;

        for(auto module: modules)
        {
            std::optional<std::string> res{module.second->address_parse(address)};
            if(!res.has_value()) continue;

            if(last_response.has_value()) 
            {
                if(*res != *last_response)
                {
                    std::cout << "Input address: " << address << "\n";
                    std::cout << "MISMATCH DETECTED between " << last_module_name << " and " << module.first << "!" << "\n";
                    std::cout << "  " << last_module_name << ": " << *last_response << "\n";
                    std::cout << "  " << module.first << ": " << *res << "\n";
                }

                assert(*res == *last_response);
            }
            last_response = *res;
            last_module_name = module.first;
        }
    }

    void Driver::PSBTParseTarget(std::span<const uint8_t> buffer) const
    {
        std::optional<std::string> last_response{std::nullopt};
        std::string last_module_name;

        for (auto& module : modules)
        {
            std::optional<std::string> res{module.second->psbt_parse(buffer)};
            if (!res.has_value()) continue;

            if (last_response.has_value()) 
            {
                if (*res != *last_response)
                {
                    std::cout << "Input PSBT (truncated): ";
                    for (size_t i = 0; i < std::min(size_t(32), buffer.size()); ++i)
                        printf("%02x", buffer[i]);
                    if (buffer.size() > 32) std::cout << "...";
                    std::cout << " (" << buffer.size() << " bytes)\n";
                    
                    std::cout << "MISMATCH DETECTED between " << last_module_name << " and " << module.first << "!" << "\n";
                    
                    // Find and highlight the differences
                    std::string last = *last_response;
                    std::string current = *res;
                    
                    // Print the full outputs only if they're reasonably sized
                    if (last.size() < 1000 && current.size() < 1000) {
                        std::cout << "  " << last_module_name << ": " << last << "\n";
                        std::cout << "  " << module.first << ": " << current << "\n";
                    } else {
                        // Find first differing position
                        size_t pos = 0;
                        while (pos < last.size() && pos < current.size() && last[pos] == current[pos]) pos++;
                        
                        // Print context around the difference
                        size_t context = 20;
                        size_t start = (pos > context) ? pos - context : 0;
                        
                        std::cout << "  Difference at position " << pos << "\n";
                        std::cout << "  " << last_module_name << " (excerpt): ..." << last.substr(start, context * 2) << "...\n";
                        std::cout << "  " << module.first << " (excerpt): ..." << current.substr(start, context * 2) << "...\n";
                    }
                }

                assert(*res == *last_response);
            }
            last_response = *res;
            last_module_name = module.first;
        }
    }

    void Driver::HtlcSuccessTxTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        
        // extracting params for  HTLC success tx
        std::string commitment_tx_hex = provider.ConsumeRandomLengthString(1024);
        uint32_t htlc_index = provider.ConsumeIntegral<uint32_t>();
        std::string preimage = provider.ConsumeRandomLengthString(32);
        uint64_t fee_rate = provider.ConsumeIntegralInRange<uint64_t>(1, 1000);
        
        // to check non-empty value
        std::map<std::string, std::string> responses;
        
        for (auto& module : modules)
        {
            std::optional<std::string> res{module.second->construct_htlc_success_tx(
                commitment_tx_hex, htlc_index, preimage, fee_rate)};
            
            if (res.has_value() && !res->empty()) {
                responses[module.first] = *res;
            }
        }
        
        if (responses.size() >= 2) {
            std::string first_module;
            std::string first_response;
            
            bool first = true;
            
            for (const auto& [module_name, response] : responses) {
                if (first) {
                    first_module = module_name;
                    first_response = response;
                    first = false;
                } else {
                    if (response != first_response) {
                        std::cout << "HTLC success tx construction mismatch:" << std::endl;
                        std::cout << "Module " << first_module << " returned: " << first_response << std::endl;
                        std::cout << "Module " << module_name << " returned: " << response << std::endl;
                        
                        compareTransactions(first_response, response);
                        assert(response == first_response);
                    }
                }
            }
        }
    }

    void Driver::HtlcTimeoutTxTarget(std::span<const uint8_t> buffer) const
    {
        FuzzedDataProvider provider(buffer.data(), buffer.size());
        
        // extracting params for  HTLC timeout tx
        std::string commitment_tx_hex = provider.ConsumeRandomLengthString(1024);
        uint32_t htlc_index = provider.ConsumeIntegral<uint32_t>();
        uint32_t cltv_expiry = provider.ConsumeIntegral<uint32_t>();
        uint64_t fee_rate = provider.ConsumeIntegralInRange<uint64_t>(1, 1000);
        
        std::map<std::string, std::string> responses;
        
        for (auto& module : modules)
        {
            std::optional<std::string> res{module.second->construct_htlc_timeout_tx(
                commitment_tx_hex, htlc_index, cltv_expiry, fee_rate)};
            
            if (res.has_value() && !res->empty()) {
                responses[module.first] = *res;
            }
        }
        
        if (responses.size() >= 2) {
            std::string first_module;
            std::string first_response;
            
            bool first = true;
            
            for (const auto& [module_name, response] : responses) {
                if (first) {
                    first_module = module_name;
                    first_response = response;
                    first = false;
                } else {
                    if (response != first_response) {
                        std::cout << "HTLC timeout tx construction mismatch:" << std::endl;
                        std::cout << "Module " << first_module << " returned: " << first_response << std::endl;
                        std::cout << "Module " << module_name << " returned: " << response << std::endl;
                        
                        compareTransactions(first_response, response);
                        assert(response == first_response);
                    }
                }
            }
        }
    }


    void Driver::compareTransactions(const std::string& tx1_hex, const std::string& tx2_hex) const
    {
        std::cout << "Transaction 1 length: " << tx1_hex.length() << " bytes" << std::endl;
        std::cout << "Transaction 2 length: " << tx2_hex.length() << " bytes" << std::endl;
        
        size_t compare_len = std::min(tx1_hex.length(), tx2_hex.length());
        compare_len = std::min(compare_len, size_t(64)); // limit to 64 chars
        
        if (compare_len > 0) {
            std::cout << "Transaction 1 prefix: " << tx1_hex.substr(0, compare_len) << std::endl;
            std::cout << "Transaction 2 prefix: " << tx2_hex.substr(0, compare_len) << std::endl;
        }
    }

    void Driver::Run(const uint8_t *data, const size_t size, const std::string &target) const
    {
        std::span<const uint8_t> buffer{data, size};
        if (target == "script") {
            this->ScriptTarget(buffer);
        } else if (target == "deserialize_block") {
            this->BlockDeserializationTarget(buffer);
        } else if (target == "script_eval") {
            this->ScriptEvalTarget(buffer);
        } else if (target == "descriptor_parse") {
            this->DescriptorParseTarget(buffer);
        } else if (target == "miniscript_parse") {
	        this->MiniscriptParseTarget(buffer);
        } else if (target == "script_asm") {
            this->ScriptAsmTarget(buffer);
	    } else if (target == "deserialize_invoice") {
            this->InvoiceDeserializationTarget(buffer);
        } else if (target == "address_parse") {
            this->AddressParseTarget(buffer);
        } else if (target == "psbt_parse") {
            this->PSBTParseTarget(buffer);
        } else if (target == "construct_htlc_success_tx") {
            this->HtlcSuccessTxTarget(buffer);
        } else if (target == "construct_htlc_timeout_tx") {
            this->HtlcTimeoutTxTarget(buffer);
        } else {
            std::cout << "Target not defined!" << std::endl;
            assert(false);
        }
    };

}
