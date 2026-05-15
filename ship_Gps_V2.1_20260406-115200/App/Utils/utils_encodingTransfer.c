#include "utils_encodingTransfer.h"

#if 0
uint8_t charsToHex(uint8_t *asc){
    uint8_t hex=0;
    if((asc[0]>='0')&&(asc[0]<='9')){
        hex=asc[0]-0x30;
    }else if((asc[0]>='a')&&(asc[0]<='f')){
        hex=asc[0]-'a'+0xa;
    }else if((asc[0]>='A')&&(asc[0]<='F')){
        hex=asc[0]-'A'+0xa;
    }

    hex = hex<<4;

    if((asc[1]>='0')&&(asc[1]<='9')){
        hex+=(asc[1]-0x30);
    }else if((asc[1]>='a')&&(asc[1]<='f')){
        hex+=(asc[1]-'a'+0xa);
    }else if((asc[1]>='A')&&(asc[1]<='F')){
        hex+=(asc[1]-'A'+0xa);
    } 
    return hex;
}

uint8_t getBits(uint8_t num,uint8_t index){
    return (num>>index)&0x01;
}

#endif
