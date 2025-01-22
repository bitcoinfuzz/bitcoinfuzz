#include <cstdint>

extern "C" bool rust_bitcoin_script(const unsigned char* input, unsigned long length);
extern "C" char* rust_bitcoin_des_block(const uint8_t *data, size_t len);
