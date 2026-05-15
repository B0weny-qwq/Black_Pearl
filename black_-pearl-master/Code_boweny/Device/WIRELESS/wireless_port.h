#ifndef __WIRELESS_PORT_H__
#define __WIRELESS_PORT_H__

#include "config.h"

#define WIRELESS_PORT_ANT1  0U
#define WIRELESS_PORT_ANT2  1U

s8 WirelessPort_Init(void);
s8 WirelessPort_Deinit(void);
void WirelessPort_SetCs(u8 level);
void WirelessPort_SetRst(u8 level);
void WirelessPort_SetAntSel(u8 ant_sel);
void WirelessPort_SetRxEn(u8 level);
void WirelessPort_SetTxEn(u8 level);
void WirelessPort_DelayMs(u16 ms);
void WirelessPort_DelayUs(u16 us);
u8 WirelessPort_SpiTransfer(u8 value);

#endif
