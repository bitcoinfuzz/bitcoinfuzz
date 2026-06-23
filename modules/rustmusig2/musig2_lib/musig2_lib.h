#include <cstdint>

extern "C" char *musig2_key_agg(const uint8_t *seckeys, size_t num_keys);
extern "C" char *
musig2_sign_session(const uint8_t *seckeys, size_t num_keys,
                    const uint8_t *msg32, const uint8_t *nonce_seeds,
                    bool use_xonly_tweak, const uint8_t *xonly_tweak,
                    bool use_plain_tweak, const uint8_t *plain_tweak);
extern "C" void musig2_free_string(void *ptr);
