#ifndef _POWER_ADC_H_
#define _POWER_ADC_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_Timer.h"
    #include "STC8G_H_Delay.h"

    typedef enum{
        POWER_LEVEL_0 = 0,
        POWER_LEVEL_1,
        POWER_LEVEL_2,
        POWER_LEVEL_3,
        POWER_LEVEL_4,
        POWER_LEVEL_5,
    }POWER_LEVEL;

    void Power_Gpio_Init(void);
    uint16_t Power_ADC_Read(void);

#endif
