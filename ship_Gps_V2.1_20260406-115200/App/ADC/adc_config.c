#include "adc_config.h"
#include "STC8G_H_Delay.h"
uint16_t Nref = 0;

/******************* AD配置函数 *******************/
void ADC_Config(void)
{
	ADC_InitTypeDef		ADC_InitStructure;		//结构定义

	ADC_InitStructure.ADC_SMPduty   = 31;		//ADC 模拟信号采样时间控制, 0~31（注意： SMPDUTY 一定不能�?�置小于 10�??
	ADC_InitStructure.ADC_CsSetup   = 0;		//ADC 通道选择时间控制 0(默�??),1
	ADC_InitStructure.ADC_CsHold    = 1;		//ADC 通道选择保持时间控制 0,1(默�??),2,3
	ADC_InitStructure.ADC_Speed     = ADC_SPEED_2X16T;		//设置 ADC 工作时钟频率	ADC_SPEED_2X1T~ADC_SPEED_2X16T
	ADC_InitStructure.ADC_AdjResult = ADC_LEFT_JUSTIFIED;	//ADC结果调整,	ADC_LEFT_JUSTIFIED,ADC_RIGHT_JUSTIFIED
	ADC_Inilize(&ADC_InitStructure);		//初�?�化
	ADC_PowerControl(ENABLE);				//ADC电源开�??, ENABLE或DISABLE
	NVIC_ADC_Init(DISABLE,Priority_0);		//�??�??使能, ENABLE/DISABLE; 优先�??(低到�??) Priority_0,Priority_1,Priority_2,Priority_3
}

#if 0
void NrefCheck(void){
	static uint8_t nrfCheckTimes = 10;
	uint16_t nowNref = 0;
	while(nrfCheckTimes > 0){
		nrfCheckTimes--;
		nowNref = Get_ADCResult(15);
		// printf("nrf now==%b02x\r\n",nowNref);
		Nref += nowNref;
		delay_ms(20);
	}
	Nref = Nref/10;
}

uint16_t Adc_Get_Nrf(void){
	return Nref;
}

#endif


