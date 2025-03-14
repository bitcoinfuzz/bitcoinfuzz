#ifndef RUST_MODULES_LIB_H
#define RUST_MODULES_LIB_H

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// LDK functions
bool ldk_des_invoice(const char* input);

// Rust Bitcoin functions
char* rust_bitcoin_script(const uint8_t *data, size_t len);
char* rust_bitcoin_des_block(const uint8_t *data, size_t len);
char* rust_bitcoin_script_asm(const char* input);
void free_c_string(char* ptr);

// Miniscript functions
bool rust_miniscript_descriptor_parse(const char* input);
bool rust_miniscript_miniscript_parse(const char* input);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // RUST_MODULES_LIB_H