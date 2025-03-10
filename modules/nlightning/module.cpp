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
        DecodeInvoiceFunc NLightning::decodeInvoice = nullptr;

        NLightning::NLightning(void) : BaseModule("NLightning")
        {
            // Attempt to load library with different extensions based on platform
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
        }

        std::optional<bool> NLightning::deserialize_invoice(std::string str) const
        {
            return decodeInvoice(str.c_str());
        }
    }
}