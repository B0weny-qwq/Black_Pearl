#include "DcMotor_Gpio.h"
uint8_t dcMotor_Front_Run_MillSeconds = 0,dcMotor_Back_Run_MillSeconds = 0;
void DcMotor_Gpio_Config(void){
	GPIO_InitTypeDef	GPIO_InitStructure;        
	GPIO_InitStructure.Pin  = GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure);

	DcMotor_FRONT_Set(FALSE);
	DcMotor_BACK_Set(FALSE);

}


void DcMotor_FRONT_Set(uint8_t onoff){
	P02 = onoff;
}

void DcMotor_BACK_Set(uint8_t onoff){
	P03 = onoff;
} 

void DcMotor_Front_Set_Run_MillSeconds(uint16_t millSeconds){
	dcMotor_Front_Run_MillSeconds = millSeconds;
}

void DcMotor_Back_Set_Run_MillSeconds(uint16_t millSeconds){
	dcMotor_Back_Run_MillSeconds = millSeconds;
}

void DcMotor_Handle(void){
	if(dcMotor_Front_Run_MillSeconds>10){
		dcMotor_Front_Run_MillSeconds -= 10;
		DcMotor_FRONT_Set(TRUE);
	}else{
		dcMotor_Front_Run_MillSeconds = 0;
		DcMotor_FRONT_Set(FALSE);
	}

	if(dcMotor_Back_Run_MillSeconds>10){
		dcMotor_Back_Run_MillSeconds -= 10;
		DcMotor_BACK_Set(TRUE);
	}else{
		dcMotor_Back_Run_MillSeconds = 0;
		DcMotor_BACK_Set(FALSE);
	}

}

