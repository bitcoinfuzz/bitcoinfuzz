#include <bitcoinfuzz/ffi.h>

extern "C" bf_result rust_psbt_psbt_v0_parse(const uint8_t *data, size_t len);
extern "C" bf_result rust_psbt_psbt_v2_parse(const uint8_t *data, size_t len);
