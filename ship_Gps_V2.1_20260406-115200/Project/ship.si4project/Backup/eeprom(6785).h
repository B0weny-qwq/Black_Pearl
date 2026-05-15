#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <intrins.h>

#define		SECTOR_SIZE				512



void ep_saveShipConfig(unsigned char *data_buf,unsigned char data_len);

void ep_readShipConfig(unsigned char *data_buf,unsigned char data_len);




typedef struct{
    int16_t offset_x;
    int16_t offset_y;
    int16_t offset_z;
}SYSTEM_CONFIG;


void System_Config_Init(void);

void System_Config_Save(int16_t x_offset,int16_t y_offset);
int16_t cfg_get_x_offset(void);

int16_t cfg_get_y_offset(void);


#endif /* 9_6_OLED_OLED_H_ */
