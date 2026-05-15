#include "timer.h"

uint8_t SysTimer_10Ms_Flag = 0,SysTimer_1Ms_Times = 0;
void  SysTimer_Init(void){
#if 0
	Timer1_Stop();
	Timer1_CLK_Select(TIM_CLOCK_1T);		//定时器时�?T模式
	Timer1_CLK_Output(DISABLE);
	T1_Load(65536UL - (MAIN_Fosc / 1000));
	Timer1_Run(ENABLE);
	Timer1_Interrupt(Priority_1);
#else
	TIM_InitTypeDef		TIM_InitStructure;                  //结构定义
	TIM_InitStructure.TIM_Mode      = TIM_16BitAutoReload;  //指定工作模式,   TIM_16BitAutoReload,TIM_16Bit,TIM_8BitAutoReload,TIM_16BitAutoReloadNoMask
	TIM_InitStructure.TIM_ClkSource = TIM_CLOCK_1T;         //指定时钟�?     TIM_CLOCK_1T,TIM_CLOCK_12T,TIM_CLOCK_Ext
	TIM_InitStructure.TIM_ClkOut    = DISABLE;              //是否输出高速脉�? ENABLE或DISABLE
	TIM_InitStructure.TIM_Value     = 65536UL - (MAIN_Fosc / 1000UL);   //初�?
	TIM_InitStructure.TIM_Run       = ENABLE;               //是否初始化后启动定时�? ENABLE或DISABLE
	Timer_Inilize(Timer1,&TIM_InitStructure);               //初始化Timer0	  Timer0,Timer1,Timer2,Timer3,Timer4
	NVIC_Timer1_Init(ENABLE,Priority_0);    //中断使能, ENABLE/DISABLE; 优先�?低到�? Priority_0,Priority_1,Priority_2,Priority_3
#endif	


}

void SysTimer_Set_10MsFlag(uint8_t flag){
    SysTimer_10Ms_Flag = flag;
}

uint8_t SysTimer_Get_10MsFlag(void){
    return SysTimer_10Ms_Flag;
}

extern uint8_t idata cruise_calib_time;

void SysTimer_1ms_Interrupt(void){

    SysTimer_1Ms_Times++;
    if(SysTimer_1Ms_Times > 10){
        SysTimer_1Ms_Times = 0;
        SysTimer_Set_10MsFlag(TRUE);
		cruise_calib_time ++;
    }
}

void Timer1_ISR_Handler (void) interrupt TMR1_VECTOR		
{
    // T1_Load(65536UL - (MAIN_Fosc / 1000));
    SysTimer_1ms_Interrupt();
}
