#include <cstdint>

extern "C" char *bitcoin_addrman_serialize(const uint8_t *data, size_t len);
extern "C" void bitcoin_addrman_free_string(char *ptr);
