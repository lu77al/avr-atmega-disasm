#include "hex_utils.h"

//----------------------------------------------------------------------
char hex2val(char c)
{
    if( c >= '0' && c <= '9' )
        return c - '0';
    else if( c >= 'A' && c <= 'F' )
        return 0x0A + c - 'A';
    return 0;
}

//----------------------------------------------------------------------
uint8_t hex2byte(const char *hex)
{
    return uint8_t((hex2val(hex[0]) << 4) + hex2val(hex[1]));
}

//----------------------------------------------------------------------
uint16_t hex2word(const char *hex)
{
    return uint16_t((hex2byte(hex) << 8) + hex2byte(&hex[2]));
}
