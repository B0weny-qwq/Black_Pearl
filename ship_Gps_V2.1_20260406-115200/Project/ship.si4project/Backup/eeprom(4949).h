#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <intrins.h>

#define		SECTOR_SIZE				512



void ep_saveShipConfig(unsigned char *data_buf,unsigned char data_len);

void ep_readShipConfig(unsigned char *data_buf,unsigned char data_len);


#endif /* 9_6_OLED_OLED_H_ */
