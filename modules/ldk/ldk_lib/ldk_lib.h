#include <cstdint>

extern "C" char* ldk_des_invoice(const char* input);

extern "C" void ldk_free_string(const char* ptr);

extern "C" char* ldk_construct_htlc_success_tx(const char* commitment_tx_hex, uint32_t htlc_index, const char* preimage, uint64_t fee_rate);

extern "C" char* ldk_construct_htlc_timeout_tx(const char* commitment_tx_hex, uint32_t htlc_index, uint32_t cltv_expiry, uint64_t fee_rate);