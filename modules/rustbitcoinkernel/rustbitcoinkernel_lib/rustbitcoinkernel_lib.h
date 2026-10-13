#include <bitcoinfuzz/ffi.h>

extern "C" bf_result rustbitcoinkernel_block(const uint8_t *data, size_t len);
extern "C" bf_result rustbitcoinkernel_transaction(const uint8_t *data,
                                                   size_t len);
extern "C" bf_result rustbitcoinkernel_block_check(const uint8_t *data,
                                                   size_t len);
