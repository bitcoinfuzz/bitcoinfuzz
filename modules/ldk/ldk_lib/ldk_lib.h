#include <cstdint>

extern "C" char* ldk_des_invoice(const char* input, size_t length);

extern "C" char* ldk_des_offer(const char* input, size_t length);

extern "C" void ldk_free_string(const char* ptr);