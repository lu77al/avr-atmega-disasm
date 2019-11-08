#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <stdint.h>

char hex2val(char c);
uint8_t hex2byte(const char *hex);
uint16_t hex2word(const char *hex);

#endif
