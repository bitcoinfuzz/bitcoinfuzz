#include <cstdint>

extern "C" bool nbitcoin_miniscript_parse(const char *input);

extern "C" bool nbitcoin_verify_script(const uint8_t *script_sig,
                                       int32_t script_sig_length,
                                       const uint8_t *script_pubkey,
                                       int32_t script_pubkey_length);

extern "C" bool nbitcoin_script_eval(const uint8_t *input_data,
                                     int32_t input_data_length, uint32_t flags,
                                     uint32_t version);

extern "C" char *nbitcoin_bip32_master_keygen(const uint8_t *data, size_t len);

extern "C" char *nbitcoin_psbt_parse(const uint8_t *data, size_t len);

extern "C" char *nbitcoin_bip32_deserialize_extended_key(const uint8_t *data,
                                                         size_t len);

extern "C" char *nbitcoin_sign_schnorr(const uint8_t *privkey,
                                       const uint8_t *hash, const uint8_t *aux);

extern "C" char *nbitcoin_bip32_derive_from_path(const uint8_t *data,
                                                 size_t len);

extern "C" void nbitcoin_free_c_string(void *ptr);
