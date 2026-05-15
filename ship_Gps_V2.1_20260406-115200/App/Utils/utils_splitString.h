#ifndef _UTILS_SPLITSTRING_H_
#define _UTILS_SPLITSTRING_H_
    #include "config.h"

    uint16_t split_string_ByChar(uint8_t * src, uint8_t ch, uint8_t * dest, uint8_t * index, uint16_t max);
    uint8_t  get_NTime_String_ByChar(uint8_t *data_m,uint16_t dataLen,uint8_t flagchar,uint8_t ntime,uint8_t *saveTable,uint8_t maxLen);

#endif
