#include "STC8G_H_Timer.h"
#include "STC8G_H_GPIO.h"
#include "STC8G_H_NVIC.h"

#include "config.h"
#include "timer.h"

#include "DcMotor_Gpio.h"
#include "pwm.h"
#include "pwm_Gpio.h"
#include "LT8920_SPI.h"
#include "led.h"
#include "gyro.h"

#include "gps_uart.h"

#include "adc_config.h"
#include "power_Adc.h"
#include "wirelessProtocal.h"
#include "autoDrive.h"
#include "pwm_Speed.h"
#include "compass.h"
#include "eeprom.h"



#define		SOFT_VERSION			"1.03.02"

#if 0
void timer_Runing_Handle(void){
	static uint16_t timer10msFlag = 0;
	timer10msFlag++;
	if(timer10msFlag%10==0){
		// 100ms
		Radio_progress();
	}else if(timer10msFlag%100==0){
		// 1000 ms
	}else if(timer10msFlag%100==0){

	}
}
#endif

void main(void)
{
	EAXSFR();	
	SysTimer_Init();

	System_Config_Init();
	
# if 0
	Gpio_Print_Init();	
#endif
	Led_IO_Init();

	ADC_Config();
	Power_Gpio_Init();
	//NrefCheck();
	
	PWM_Gpio_Config();
	DCMotor_Pwm_Init();


	DcMotor_Gpio_Config();

	SI2C_Gpio_Init();
	Pwm_Speed_Init();
	// wireless
	//LT89xx_INIT();
	
	// gps
	Gps_Uart_Gpio_Init();
	Gps_Uart_Init();

	EA = 1;

	printf("GPS Mainboard V:%s\r\n",SOFT_VERSION);
	
	delay_ms(200);
	// wireless
	LT89xx_INIT();
	
	

	Compass_Init();
	
	autodrv_init();
	
	
	
	while (1)
	{
		if(SysTimer_Get_10MsFlag()==TRUE)
		{
			SysTimer_Set_10MsFlag(FALSE);

			Radio_progress();
			// timer_Runing_Handle();
			
			Compass_Angel_adjust_Handle();
			
			Red_Led_Refresh_Handle();

			Ship_Led_Refresh_Handle();

			DcMotor_Handle();

			//Gps_Uart_Dma_OutTime_Handle();
			Gps_Uart_Data_Resolve();
			
			Power_Check_Handle();

			autoDrive_Handle();

			WirelessProtocal_Accelerator_OutTime_Handle();
		}
		
	}
}



