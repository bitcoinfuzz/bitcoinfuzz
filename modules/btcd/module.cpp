#include <span>
#include <cstring>

#include "module.h"
#include "btcd_wrapper/libbtcd_wrapper.h"

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

            return BTCDEvalScript(script_data, /*flags=*/0, version) == 1;
        }

        std::optional<std::string> Btcd::parse_p2p_message(std::span<const uint8_t> buffer) const
        {
            ByteArray message_data{
                .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
                .length = static_cast<int>(buffer.size())};

            const auto message_res = BTCDParseP2PMessage(message_data);

            if (message_res == nullptr) {
                return std::nullopt;
            }
            if (strlen(message_res) == 0) {
                free(message_res);
                return std::nullopt;
            }

            std::string res(message_res);
            free(message_res);
            return res;
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

        std::optional<std::string> Btcd::deserialize_block(std::span<const uint8_t> buffer) const
        {
            ByteArray script_data{
                .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
                .length = static_cast<int>(buffer.size())};

            auto pointer{BTCDDesBlock(script_data)};
            std::string result(pointer);
            BTCDFreeString(pointer);
            if (result == "unsupported segwit version") {
                return std::nullopt;
            }
            return result;
        }

        std::optional<std::string> Btcd::addrv2_parse(std::span<const uint8_t> buffer) const
        {
            ByteArray addrv2;
            addrv2.data = (char*)buffer.data();
            addrv2.length = buffer.size();

            char* result = BTCDAddrv2(addrv2);
            if (!result) return std::nullopt;

            std::string res(result);
            BTCDFreeString(result);
            return res;
        }

        std::optional<std::string> Btcd::psbt_parse(std::span<const uint8_t> buffer) const
        {
            ByteArray script;
            script.data = (char*)buffer.data();
            script.length = buffer.size();

            char* result = BTCDParsePSBT(script);
            if (!result) return std::nullopt;

            std::string res(result);
            BTCDFreeString(result);
            return res;
        }

        std::optional<std::string> Btcd::address_parse(std::string str) const
        {
            ByteArray data;
            data.data = const_cast<char*>(str.data());
            data.length = static_cast<int>(str.size());

            char* result = BTCDAddress(data);
            if (!result) return std::nullopt;

            std::string res(result);
            BTCDFreeString(result);
            return res;
        }

        std::optional<std::string> Btcd::transaction_eval(std::span<const uint8_t> buffer) const
        {
            ByteArray tx;
            tx.data = (char*)buffer.data();
            tx.length = buffer.size();

            char* result = BTCDTransactionEval(tx);

            std::string res(result);
            BTCDFreeString(result);
            return res;
        }

    } // namespace module
} // namespace bitcoinfuzz
