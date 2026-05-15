#ifndef _PWM_H_
#define _PWM_H_
    #include "config.h"
    #include "STC8H_PWM.h"
    #include "STC8G_H_NVIC.h"
    #include "STC8G_H_Switch.h"

    //#define MOTOR_POSITIVE 		TRUE
    #define MOTOR_POSITIVE FALSE
		
		//#define		MOTOR_LEFTRIGHT				TRUE
		#define		MOTOR_LEFTRIGHT				FALSE


    #define CRUISE_NO_CORRECT 0
    #define CRUISE_LEFT_CORRECT 1
    #define CRUISE_RIGHT_CORRECT 2
	
    #define CRUISE_CORRECT_MODE 		CRUISE_LEFT_CORRECT

#ifdef		SHIP_BIG
#define			CORRECT_PERIOD_TIME				100
#define			CORRECT_START_TIME				120

#elif  defined(SHIP_MID)
#define			CORRECT_PERIOD_TIME				100
#define			CORRECT_START_TIME				120
#else
#define			CORRECT_PERIOD_TIME				100
#define			CORRECT_START_TIME				120
#endif

    typedef enum{
        DIRECTION_FORWARD=0,
        DIRECTION_BACK,
        DIRECTION_LEFT,
        DIRECTION_RIGHT,


        // DIRECTION_FORWARD=0,
        // DIRECTION_STOP,
        // DIRECTION_BACK,
        // DIRECTION_LEFT,
        // DIRECTION_MID,
        // DIRECTION_RIGHT,
    }MOVE_DIRECTION;

    typedef enum{
        CRUISE_STOP = 0,
        CRUISE_LOW,
        CRUISE_MIDDLE,
        CRUISE_HIGH,
        
    }CRUISE_CONTROMODE;

    void DCMotor_Pwm_Init(void);
    void DcMotor_MRight_PWM_Set(uint8_t onoff,uint8_t accelerator);
    void DcMotor_MLeft_PWM_Set(uint8_t onoff,uint8_t accelerator);
    void DcMotor_Direction_Set(uint8_t direction);
    void DcMotor_PWM_CruiseControl_Set_Handle(void);

    uint8_t Get_DcMotor_PWM_CruiseControl_Mode(void);
    void Set_DcMotor_PWM_CruiseControl_Mode(uint8_t mode);
#endif

