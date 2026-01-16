#pragma once

extern "C" {
char *floresta_addrv2(const unsigned char *data, size_t len);
void floresta_free_c_string(char *ptr);
}
