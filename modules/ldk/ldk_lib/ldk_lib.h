#include <bitcoinfuzz/ffi.h>

extern "C" bf_result ldk_des_invoice(const char *input);

extern "C" bf_result ldk_des_offer(const char *input);

extern "C" bf_result ldk_parse_p2p_lightning_message(const uint8_t *data,
                                                     size_t len);

extern "C" bf_result ldk_decode_onion(const uint8_t *data, size_t len);
