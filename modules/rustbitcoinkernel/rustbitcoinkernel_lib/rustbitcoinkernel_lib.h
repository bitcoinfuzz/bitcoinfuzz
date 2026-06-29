#include <cstdint>

extern "C" char *rustbitcoinkernel_block(const uint8_t *data, size_t len);
extern "C" char *rustbitcoinkernel_transaction(const uint8_t *data, size_t len);
extern "C" void kernel_free_c_string(char *ptr);
