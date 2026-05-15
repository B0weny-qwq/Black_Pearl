#include "utils_checkSum.h"

uint8_t cal_checksum_XOR(uint8_t *data_m,uint16_t len)
{	 
	uint8_t val = 0;
	uint16_t i=0;
	for (;i<len;i++)
		val ^= data_m[i];
    return val;
}
