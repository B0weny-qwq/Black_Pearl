#include "power_Adc.h"

#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\..\Driver\inc\STC32G_ADC.h"

static u8 g_power_level = POWER_LEVEL_0;

void Power_Gpio_Init(void)
{
    P0_MODE_IN_HIZ(GPIO_Pin_1);
}

u16 Power_ADC_Read(void)
{
    return Get_ADCResult(ADC_CH9);
}

u8 Power_ADC_Get_Level(void)
{
    return g_power_level;
}

void Power_Check_Handle(void)
{
    static u8 power_check_times = 0U;
    u16 bat_volt;

    if (power_check_times < 100U) {
        power_check_times++;
        return;
    }
    power_check_times = 0U;

    bat_volt = Power_ADC_Read();

    if (bat_volt >= BATT_VOLT_FULL) {
        g_power_level = POWER_LEVEL_4;
    } else if (bat_volt >= BATT_VOLT_LEVEL3) {
        g_power_level = POWER_LEVEL_3;
    } else if (bat_volt >= BATT_VOLT_LEVEL2) {
        g_power_level = POWER_LEVEL_2;
    } else if (bat_volt >= BATT_VOLT_LEVEL1) {
        g_power_level = POWER_LEVEL_1;
    } else {
        g_power_level = POWER_LEVEL_0;
    }
}
