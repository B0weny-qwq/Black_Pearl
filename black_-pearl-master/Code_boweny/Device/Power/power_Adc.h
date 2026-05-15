#ifndef __POWER_ADC_H__
#define __POWER_ADC_H__

#include "config.h"

typedef enum
{
    POWER_LEVEL_0 = 0,
    POWER_LEVEL_1,
    POWER_LEVEL_2,
    POWER_LEVEL_3,
    POWER_LEVEL_4
} Power_Level_t;

#ifdef BOARD_12V
#define BATT_VOLT_FULL    2000
#define BATT_VOLT_LEVEL3  1900
#define BATT_VOLT_LEVEL2  1730
#define BATT_VOLT_LEVEL1  1620
#define BATT_VOLT_LOW     1530
#else
#define BATT_VOLT_FULL    1710
#define BATT_VOLT_LEVEL3  1630
#define BATT_VOLT_LEVEL2  1530
#define BATT_VOLT_LEVEL1  1420
#define BATT_VOLT_LOW     1330
#endif

void Power_Gpio_Init(void);
u16 Power_ADC_Read(void);
u8 Power_ADC_Get_Level(void);
void Power_Check_Handle(void);

#endif
