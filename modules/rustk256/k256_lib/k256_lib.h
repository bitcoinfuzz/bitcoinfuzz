#include <cstdint>

extern "C" char *k256_private_to_public_key(const uint8_t *buffer);
extern "C" char *k256_sign_compact(const uint8_t *buffer, const uint8_t *hash);
extern "C" char *k256_sign_der(const uint8_t *buffer, const uint8_t *hash);
extern "C" bool k256_sign_verify(const uint8_t *buffer, const uint8_t *hash,
                                 const uint8_t *sign, size_t len);
extern "C" char *k256_ecdh(const uint8_t *buffer, const uint8_t *pubkey);
extern "C" void k256_free_string(void *ptr);
