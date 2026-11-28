#include <bitcoinfuzz/ffi.h>

extern "C" bf_result rustreexo_stump_modify(const uint8_t *buffer,
                                            size_t buffer_len);
extern "C" bf_result rustreexo_stump_update(const uint8_t *add_hashes_flat,
                                            size_t add_count,
                                            const uint8_t *del_hashes_flat,
                                            size_t del_count,
                                            const uint8_t *new_add_hashes_flat,
                                            size_t new_add_count);
