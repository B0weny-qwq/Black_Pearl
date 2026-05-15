#include "power_Adc.h"
#include "STC8G_H_ADC.h"

uint8_t power_Level = POWER_LEVEL_0;

void Power_Gpio_Init(void){
    GPIO_InitTypeDef	GPIO_InitStructure;        
	GPIO_InitStructure.Pin  = GPIO_Pin_0;
	GPIO_InitStructure.Mode = GPIO_HighZ;//GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure);
}

uint16_t Power_ADC_Read(void){
    return Get_ADCResult(ADC_CH8);
}

uint8_t Power_ADC_Get_Level(void){
	return power_Level;
}

//6.92     1465-1483
// 7.48     1549
// 7.58     1570
// 7.6V    1616
// 7.8V		1680
// 8.3v      1772
void Power_Check_Handle(void){
	static uint8_t powerCheckTimes = 0;
	uint16_t batVolt = 0;
	if(powerCheckTimes < 100){
		powerCheckTimes ++;
		return;
	}
	powerCheckTimes = 0;
	batVolt = Power_ADC_Read();
	//printf("power Adc==%u\r\n",batVolt);
	if(batVolt >= BATT_VOLT_FULL){
		// 2.9
		power_Level = POWER_LEVEL_4;
	}else if(batVolt >= BATT_VOLT_LEVEL3){
		// 2.5
		power_Level = POWER_LEVEL_3;
	}else if(batVolt >= BATT_VOLT_LEVEL2){
		// 2.3
		power_Level = POWER_LEVEL_2;
	}else if(batVolt >= BATT_VOLT_LEVEL1){
		// 2.0
		power_Level = POWER_LEVEL_1;
	}else {
		power_Level = POWER_LEVEL_0;
	}


}

