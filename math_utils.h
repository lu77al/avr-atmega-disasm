#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <stdint.h>

#define BIT(VAL,BIT) (VAL & (1 << BIT))
#define F16(VAL,BIT,WIDTH) ((VAL >> BIT) & ( 0xffff >> (16 - WIDTH) ))

char hex2val(char c);
uint8_t hex2byte(const char *hex);
uint16_t hex2word(const char *hex);

#endif
