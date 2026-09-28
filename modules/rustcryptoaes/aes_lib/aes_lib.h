#include <bitcoinfuzz/ffi.h>

extern "C" bf_result rustcrypto_aes256_cbc(const uint8_t *key,
                                           const uint8_t *iv, bool pad,
                                           const uint8_t *data, size_t len);
