#ifndef _TIMER_H_
#define _TIMER_H_
    #include "config.h"
    #include "STC8G_H_Timer.h"
    #include "STC8G_H_NVIC.h"
    void SysTimer_Init(void);
    void SysTimer_Set_10MsFlag(uint8_t flag);
    uint8_t SysTimer_Get_10MsFlag(void);
void SysTimer_delay10ms(uint8_t delay_cnt);

void SysTimer_delayms(uint8_t delay_cnt);
#endif
