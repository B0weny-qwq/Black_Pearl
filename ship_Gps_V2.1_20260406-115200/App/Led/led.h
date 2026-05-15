#ifndef _LED_H_
#define _LED_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_Delay.h"

    typedef enum{
        LED_ON = 0,
        LED_REFRESH,
        LED_OFF,
    }LED_MODE;

    void Led_IO_Init(void);
    void Red_Led_Switch(uint8_t onfoff);
    void Red_Led_Refresh_Handle(void);
    
    void Ship_Led_Switch(uint8_t onfoff);
    void Ship_Led_Refresh_Handle(void);

    void Ship_LED_Set_Handle(void);
#endif
