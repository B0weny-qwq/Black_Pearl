#include "pwm.h"

PWMx_Duty PWMA_Duty;
uint8_t dcMotor_PWM_CruiseControl_Mode = CRUISE_STOP;
void DCMotor_Pwm_Init(void){
	PWMx_InitDefine	PWMx_InitStructure;
    PWMA_Duty.PWM1_Duty = 0;
    PWMA_Duty.PWM2_Duty = 0;

	PWMx_InitStructure.PWM_Mode    =	CCMRn_PWM_MODE1;	//模式,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM_Duty    = PWMA_Duty.PWM1_Duty;	//PWM占空比时�?, 0~Period
	PWMx_InitStructure.PWM_EnoSelect   = ENO1P|ENO1N;	//输出通道选择,	ENO1P,ENO1N,ENO3P,ENO3N,ENO3P,ENO3N,ENO4P,ENO4N / ENO5P,ENO6P,ENO7P,ENO8P
	PWM_Configuration(PWM1, &PWMx_InitStructure);			//初�?�化PWM,  PWMA,PWMB


	PWMx_InitStructure.PWM_Mode    =	CCMRn_PWM_MODE1;	//模式,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM_Duty    = PWMA_Duty.PWM2_Duty;	//PWM占空比时�?, 0~Period
	PWMx_InitStructure.PWM_EnoSelect   = ENO3P|ENO3N;	//输出通道选择,	ENO1P,ENO1N,ENO3P,ENO3N,ENO3P,ENO3N,ENO4P,ENO4N / ENO5P,ENO6P,ENO7P,ENO8P
	PWM_Configuration(PWM3, &PWMx_InitStructure);			//初�?�化PWM,  PWMA,PWMB

	PWMx_InitStructure.PWM_Period   = 3072;					//周期时间,   0~65535
	PWMx_InitStructure.PWM_DeadTime = 100;					//死区发生器�?�置, 0~255
	PWMx_InitStructure.PWM_MainOutEnable= ENABLE;			//主输出使�?, ENABLE,DISABLE
	PWMx_InitStructure.PWM_CEN_Enable   = ENABLE;			//使能计数�?, ENABLE,DISABLE
	PWM_Configuration(PWMA, &PWMx_InitStructure);			//初�?�化PWM通用寄存�?,  PWMA,PWMB

    PWM1_SW(PWM1_SW_P20_P21); //left
	PWM3_SW(PWM3_SW_P24_P25); //right
	NVIC_PWM_Init(PWMA,DISABLE,Priority_0);

    DcMotor_Direction_Set(DIRECTION_FORWARD);
    DcMotor_MRight_PWM_Set(FALSE,0);
    DcMotor_MRight_PWM_Set(FALSE,0);
}

void DcMotor_MRight_PWM_Set(uint8_t onoff,uint8_t accelerator){
    // P12 = onoff;

    if(onoff){
        PWMA_Duty.PWM3_Duty = 1024+20*accelerator;
    }else{
        PWMA_Duty.PWM3_Duty = 0;
    }
    UpdatePwm(PWMA, &PWMA_Duty);
}

void DcMotor_MLeft_PWM_Set(uint8_t onoff,uint8_t accelerator){
	// P11 = onoff;

    if(onoff){
        PWMA_Duty.PWM1_Duty =  1024+20*accelerator;
    }else{
        PWMA_Duty.PWM1_Duty = 0;
    }
    UpdatePwm(PWMA, &PWMA_Duty);
}

void DcMotor_Direction_Set(uint8_t direction){
    static uint8_t lastDirection = 255;
    if(lastDirection != direction){
        lastDirection = direction;
    }else{
        return;
    }
    switch (lastDirection)
    {
        #if MOTOR_POSITIVE == TRUE
            case DIRECTION_FORWARD:{
        #else
            case DIRECTION_BACK:{                
        #endif


            PWMA_CC1E_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO1P;
			PWMA_CC1NE_Enable();		//开�?输入捕获/比较输出
			PWMA_ENO |= ENO1N;

            PWMA_CC3E_Enable();			//开�?输入捕获/比较输出
			PWMA_ENO |= ENO3P;
            PWMA_CC3NE_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO3N;



        }
        break;
        #if MOTOR_POSITIVE == TRUE
            case DIRECTION_BACK:{
        #else
            case DIRECTION_FORWARD:{
        #endif
        

            PWMA_CC1E_Enable();			
			PWMA_ENO |= ENO1P;
            PWMA_CC1NE_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO1N;

			PWMA_CC3E_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO3P;
            PWMA_CC3NE_Enable();		//开�?输入捕获/比较输出
			PWMA_ENO |= ENO3N;
        }
        break;
        #if MOTOR_LEFTRIGHT == TRUE
            case DIRECTION_LEFT:{
        #else
            case DIRECTION_RIGHT:{
        #endif
       
            PWMA_CC1E_Enable();			
			PWMA_ENO |= ENO1P;
            PWMA_CC1NE_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO1N;

            PWMA_CC3E_Enable();			//开�?输入捕获/比较输出
			PWMA_ENO |= ENO3P;
            PWMA_CC3NE_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO3N;
        }
        break;

        #if MOTOR_LEFTRIGHT == TRUE
            case DIRECTION_RIGHT:{
        #else
            case DIRECTION_LEFT:{
        #endif
        
            PWMA_CC1E_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO1P;
			PWMA_CC1NE_Enable();		//开�?输入捕获/比较输出
			PWMA_ENO |= ENO1N;

            
			PWMA_CC3E_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO3P;
            PWMA_CC3NE_Enable();		//开�?输入捕获/比较输出
			PWMA_ENO |= ENO3N;
        }
        break;

    }
}


void DcMotor_PWM_CruiseControl_Set_Handle(void){
    dcMotor_PWM_CruiseControl_Mode++;
    DcMotor_Direction_Set(DIRECTION_FORWARD);
    if(dcMotor_PWM_CruiseControl_Mode > CRUISE_HIGH){
        dcMotor_PWM_CruiseControl_Mode = CRUISE_STOP;
    }
    switch (dcMotor_PWM_CruiseControl_Mode)
    {
        case CRUISE_STOP:{
            DcMotor_MLeft_PWM_Set(FALSE,0);
            DcMotor_MRight_PWM_Set(FALSE,0);
        }
        break;

        case CRUISE_LOW:{
            DcMotor_MLeft_PWM_Set(TRUE,30);
            DcMotor_MRight_PWM_Set(TRUE,30);
        }
        break;

        case CRUISE_MIDDLE:{
            DcMotor_MLeft_PWM_Set(TRUE,60);
            DcMotor_MRight_PWM_Set(TRUE,60);
        }
        break;

        case CRUISE_HIGH:{
            DcMotor_MLeft_PWM_Set(TRUE,100);
            DcMotor_MRight_PWM_Set(TRUE,100);
        }
        break;

    }
    

}

uint8_t Get_DcMotor_PWM_CruiseControl_Mode(void){
    return dcMotor_PWM_CruiseControl_Mode;
}
 
void Set_DcMotor_PWM_CruiseControl_Mode(uint8_t mode){
     dcMotor_PWM_CruiseControl_Mode = mode;
}


