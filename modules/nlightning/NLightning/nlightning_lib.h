#include <cstdint>

extern "C" char* nlightning_deserialize_invoice(const char* input);
extern "C" void nlightning_free_string(char* ptr);
