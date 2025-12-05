#include <cstdint>

extern "C" char *rustbitcoinkernel_block(const uint8_t *data, size_t len);
extern "C" void free_c_string(char *ptr);
