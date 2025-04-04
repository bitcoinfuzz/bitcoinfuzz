#ifndef LIGHTNINGCORE_LIB_H
#define LIGHTNINGCORE_LIB_H

#ifdef __cplusplus
/* C++ specific includes */
#include <cstdint>
extern "C" {
#else
/* C specific includes */
#include <stdint.h>
#include <stdbool.h>
#endif

char *clightning_des_invoice(const char* input);

#ifdef __cplusplus
}
#endif

#endif /* LIGHTNINGCORE_LIB_H */
