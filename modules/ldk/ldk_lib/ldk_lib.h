#include <cstddef>
#include <cstdint>

extern "C" char* ldk_des_invoice(const char* input);

extern "C" void ldk_free_string(const char* ptr);

extern "C" char* ldk_parse_channel_announcement(const unsigned char* data, size_t len);