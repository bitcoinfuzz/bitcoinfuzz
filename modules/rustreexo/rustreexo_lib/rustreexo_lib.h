#include <cstdint>

extern "C" char *rustreexo_stump_modify(const uint8_t *buffer,
                                        size_t buffer_len);
extern "C" void rustreexo_free_string(void *ptr);
