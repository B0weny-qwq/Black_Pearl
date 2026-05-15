
#include <intrins.H>
//#include "common.h"
#include "STC8G_H_EEPROM.h"
#include "eeprom.h"



//------------------------------------------------------------------------------------------
#define WIRELESS_CONFIG_ADDRESS   	0X0
#define SYSTEM_CONFIG_ADDRESS   	512


//------------------------------------------------------------------------------------------
void ep_saveShipConfig(unsigned char *data_buf,unsigned char data_len)
{	
	int addr = WIRELESS_CONFIG_ADDRESS;

	EEPROM_SectorErase(addr);
    EEPROM_write_n(addr,data_buf,data_len);
}

void ep_readShipConfig(unsigned char *data_buf,unsigned char data_len)
{	
	int addr = WIRELESS_CONFIG_ADDRESS;

	EEPROM_read_n(addr,data_buf,data_len);
}



SYSTEM_CONFIG systemConfig;

void System_Config_Init(void){
    memset(&systemConfig,0,sizeof(SYSTEM_CONFIG));
    EEPROM_read_n(SYSTEM_CONFIG_ADDRESS,(uint8_t *)&systemConfig,sizeof(SYSTEM_CONFIG));
	if (systemConfig.offset_x == 0xffff)
	{	
		memset(&systemConfig,0,sizeof(SYSTEM_CONFIG));
    }
}

int16_t cfg_get_x_offset(void)
{	
	return systemConfig.offset_x;
}

int16_t cfg_get_y_offset(void)
{	
	return systemConfig.offset_y;
}


void System_Config_Save(int16_t x_offset,int16_t y_offset){

	systemConfig.offset_x = x_offset;
	systemConfig.offset_y = y_offset;
    EEPROM_SectorErase(SYSTEM_CONFIG_ADDRESS);
    EEPROM_write_n(SYSTEM_CONFIG_ADDRESS,(uint8_t *)&systemConfig,sizeof(SYSTEM_CONFIG));
}



