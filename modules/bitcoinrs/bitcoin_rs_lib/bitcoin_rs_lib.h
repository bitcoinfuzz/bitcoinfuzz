#include <cstddef>
#include <cstdint>

extern "C" char *bitcoin_rs_des_block(const uint8_t *data, size_t len);
extern "C" void bitcoin_rs_free_c_string(char *ptr);
