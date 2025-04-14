#include <cstddef>
#include <cstdint>

extern "C" {
    bool embit_miniscript_parse(const char* input);
    bool embit_descriptor_parse(const char* input);
    char* embit_psbt_parse(const uint8_t* data, size_t len);
}