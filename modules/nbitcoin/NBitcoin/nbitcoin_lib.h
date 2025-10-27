#include <cstdint>

extern "C" bool nbitcoin_miniscript_parse(const char* input);

extern "C" bool nbitcoin_descriptor_parse(const char* input);

extern "C" bool nbitcoin_script_eval(const uint8_t* input_data, int32_t input_data_length, uint32_t flags, uint32_t version);
