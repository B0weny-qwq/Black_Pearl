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
    }POWER_LEVEL;

#ifdef			BOARD_12V
	// 11.46V-- 2100    
	#define			BATT_VOLT_FULL					2000
	#define			BATT_VOLT_LEVEL3				1900
	#define			BATT_VOLT_LEVEL2				1730
	#define			BATT_VOLT_LEVEL1				1620
	#define			BATT_VOLT_LOW					1530
#else
	#define			BATT_VOLT_FULL					1710
	#define			BATT_VOLT_LEVEL3				1630
	#define			BATT_VOLT_LEVEL2				1530
	#define			BATT_VOLT_LEVEL1				1420
	#define			BATT_VOLT_LOW					1330
#endif

    void Power_Gpio_Init(void);
    uint16_t Power_ADC_Read(void);
	uint8_t Power_ADC_Get_Level(void);
	void Power_Check_Handle(void);
#endif
