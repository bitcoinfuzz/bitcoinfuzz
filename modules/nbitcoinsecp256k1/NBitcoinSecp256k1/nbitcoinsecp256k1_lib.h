#include <bitcoinfuzz/ffi.h>
#include <cstdint>

extern "C" bf_result
nbitcoinsecp256k1_private_to_public_key(const uint8_t *buffer);

extern "C" bf_result nbitcoinsecp256k1_sign_compact(const uint8_t *buffer,
                                                    const uint8_t *hash);

extern "C" bf_result nbitcoinsecp256k1_sign_der(const uint8_t *buffer,
                                                const uint8_t *hash);

extern "C" bool nbitcoinsecp256k1_sign_verify(const uint8_t *buffer,
                                              const uint8_t *hash,
                                              const uint8_t *sign,
                                              uint32_t signLen);

extern "C" bf_result nbitcoinsecp256k1_ecdh(const uint8_t *buffer,
                                            const uint8_t *pubkey);

extern "C" bf_result
nbitcoinsecp256k1_schnorr_verify(const uint8_t *private_key_32,
                                 const uint8_t *hash_32, const uint8_t *sig_64);
