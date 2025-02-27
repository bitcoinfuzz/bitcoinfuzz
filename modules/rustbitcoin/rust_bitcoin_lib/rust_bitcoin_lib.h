#include <cstdint>

extern "C" char* rust_bitcoin_script(const uint8_t *data, size_t len);
extern "C" char* rust_bitcoin_des_block(const uint8_t *data, size_t len);
extern "C" char* rust_bitcoin_script_asm(const char* input);
extern "C" void free_c_string(char* ptr);
