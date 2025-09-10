#include <cstdint>

extern "C" char* ldk_des_invoice(const char* input);

extern "C" char* ldk_des_offer(const char* input);

extern "C" char* ldk_parse_p2p_lightning_message(const uint8_t *data, size_t len);

extern "C" void ldk_free_string(const char* ptr);