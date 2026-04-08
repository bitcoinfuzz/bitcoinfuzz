#include <cstdint>

extern "C" char *musig2_key_agg(const uint8_t *pubkeys, size_t num_keys);
extern "C" void musig2_free_string(void *ptr);
