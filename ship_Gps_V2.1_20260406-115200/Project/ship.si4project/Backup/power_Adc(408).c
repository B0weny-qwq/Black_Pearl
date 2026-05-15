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

void Power_Check_Handle(void){
	static uint8_t powerCheckTimes = 0;
	uint16_t batVolt = 0;
	if(powerCheckTimes < 100){
		powerCheckTimes ++;
		return;
	}
	powerCheckTimes = 0;
	batVolt = Power_ADC_Read();
	// printf("power Adc==%b02x,%b02x\r\n",batVolt,Adc_Get_Nrf());
	if(batVolt >= 0x0f){
		// >3.1V
		power_Level = POWER_LEVEL_5;
	}else if(batVolt >= 0x0e){
		// 2.9
		power_Level = POWER_LEVEL_4;
	}else if(batVolt >= 0x0c){
		// 2.5
		power_Level = POWER_LEVEL_3;
	}else if(batVolt >= 0x0b){
		// 2.3
		power_Level = POWER_LEVEL_2;
	}else if(batVolt >= 0x09){
		// 2.0
		power_Level = POWER_LEVEL_1;
	}else {
		power_Level = POWER_LEVEL_0;
	}


}

