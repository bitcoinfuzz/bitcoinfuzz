#include <span>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#define LIB_HANDLE HMODULE
#define LOAD_LIBRARY(name) LoadLibraryA(name)
#define GET_PROC_ADDRESS(handle, name) GetProcAddress(handle, name)
#define CLOSE_LIBRARY(handle) FreeLibrary(handle)
#define GET_ERROR() GetLastError()
#define LIBRARY_PATH "./modules/nlightning/src/bin/NLightning.CppBridge.dll"
#else
#include <dlfcn.h>
#define LIB_HANDLE void *
#define LOAD_LIBRARY(name) dlopen(name, RTLD_NOW)
#define GET_PROC_ADDRESS(handle, name) dlsym(handle, name)
#define CLOSE_LIBRARY(handle) dlclose(handle)
#define GET_ERROR() dlerror()
#ifdef __APPLE__
#define LIBRARY_PATH "./modules/nlightning/src/bin/NLightning.CppBridge.dylib"
#else
#define LIBRARY_PATH "./modules/nlightning/src/bin/NLightning.CppBridge.so"
#endif
#endif

#include "module.h"

namespace bitcoinfuzz
{
    namespace module
    {
        FreeStringFunc NLightning::freeString = nullptr;
        DecodeInvoiceFunc NLightning::decodeInvoice = nullptr;
        DecodeInvoiceFunc NLightning::decodeLightningCoreInvoice = nullptr;  // Initialize new function pointer
        CleanupResources NLightning::cleanupResources = nullptr;

        NLightning::NLightning(void) : BaseModule("NLightning")
        {
            LIB_HANDLE libHandle = LOAD_LIBRARY(LIBRARY_PATH);

            if (!libHandle)
            {
                std::cerr << "Failed to load NLightning library: ";
#ifdef _WIN32
                char errorMessage[256];
                FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    GET_ERROR(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    errorMessage,
                    sizeof(errorMessage),
                    NULL);
                std::cerr << errorMessage;
#else
                std::cerr << GET_ERROR();
#endif
                std::cerr << std::endl;
                return;
            }

            if (decodeInvoice == nullptr)
                decodeInvoice = (DecodeInvoiceFunc)GET_PROC_ADDRESS(libHandle, "DecodeInvoice");

            if (!decodeInvoice)
            {
                std::cerr << "Failed to find DecodeInvoice symbol" << std::endl;
                CLOSE_LIBRARY(libHandle);
                return;
            }

            if (decodeLightningCoreInvoice == nullptr)
                decodeLightningCoreInvoice = (DecodeInvoiceFunc)GET_PROC_ADDRESS(libHandle, "DecodeLightningCoreInvoice");

            if (!decodeLightningCoreInvoice)
            {
                std::cerr << "Failed to find DecodeLightningCoreInvoice symbol" << std::endl;
                CLOSE_LIBRARY(libHandle);
                return;
            }

            if (freeString == nullptr)
                freeString = (FreeStringFunc)GET_PROC_ADDRESS(libHandle, "FreeString");

            if (!freeString)
            {
                std::cerr << "Failed to find FreeString symbol" << std::endl;
                CLOSE_LIBRARY(libHandle);
                return;
            }

            if (cleanupResources == nullptr)
                cleanupResources = (CleanupResources)GET_PROC_ADDRESS(libHandle, "CleanupResources");

            if (!cleanupResources)
            {
                std::cerr << "Failed to find CleanupResources symbol" << std::endl;
                CLOSE_LIBRARY(libHandle);
                return;
            }
        }

        std::optional<std::string> NLightning::deserialize_invoice(std::string str) const
        {
            // Check if the invoice is from Lightning Core
            if (str.rfind("lnbc", 0) == 0)  // If it starts with "lnbc"
            {
                char* result = decodeLightningCoreInvoice(str.c_str());  // Call new function

                if (result == nullptr)
                {
                    cleanupResources();
                    return std::nullopt;
                }

                std::string resultStr(result);
                freeString(result);
                cleanupResources();

                return resultStr;
            }

            // Default behavior
            char* result = decodeInvoice(str.c_str());
            
            if (result == nullptr) {
                cleanupResources();
                return std::nullopt;
            }
            
            std::string resultStr(result);
            freeString(result);
            cleanupResources();
           
            
            return resultStr;
        }
    }
}