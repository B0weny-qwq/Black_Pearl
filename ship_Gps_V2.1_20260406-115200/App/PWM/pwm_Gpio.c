#include "pwm_Gpio.h"

void PWM_Gpio_Config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;        
	GPIO_InitStructure.Pin  = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_4|GPIO_Pin_5;
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P2,&GPIO_InitStructure);

	P26 = 0;
	P27 = 0;
	P24 = 0;
	P25 = 0;
}


