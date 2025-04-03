#pragma once

#include <optional>
#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <bitcoinfuzz/basemodule.h>

namespace bitcoinfuzz
{
    namespace module
    {
        typedef char* (*DecodeInvoiceFunc)(const char* input);
        typedef void (*FreeStringFunc)(char* ptr);
        typedef void (*CleanupResources)();

        class NLightning : public BaseModule
        {
        public:
            NLightning(void);
            std::optional<std::string> deserialize_invoice(std::string str) const override;
            ~NLightning() noexcept override = default;

        private:
            static DecodeInvoiceFunc decodeInvoice;
            static DecodeInvoiceFunc decodeLightningCoreInvoice;  // Added function pointer
            static FreeStringFunc freeString;
            static CleanupResources cleanupResources;
        };
    }
}