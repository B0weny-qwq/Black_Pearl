
#include <intrins.H>
//#include "common.h"
#include "STC8G_H_EEPROM.h"
#include "eeprom.h"



//------------------------------------------------------------------------------------------
#define WIRELESS_CONFIG_ADDRESS   	0X0


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


