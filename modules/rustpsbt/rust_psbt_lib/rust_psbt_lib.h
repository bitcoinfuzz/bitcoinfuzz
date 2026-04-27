#include <cstdint>

extern "C" char *rust_psbt_psbt_parse(const uint8_t *data, size_t len);
extern "C" void rust_psbt_free_c_string(char *ptr);
