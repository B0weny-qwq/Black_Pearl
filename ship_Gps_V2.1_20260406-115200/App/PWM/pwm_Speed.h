#ifndef _PWM_SPEED_H_
#define _PWM_SPEED_H_
    #include "config.h"

    typedef enum{
        PWM_SPEED_LEVEL_1 = 0,
        PWM_SPEED_LEVEL_2,
        PWM_SPEED_LEVEL_3,
        PWM_SPEED_LEVEL_4,
        PWM_SPEED_LEVEL_5,
    }PWM_SPEED_LEVEL;

    typedef struct{
        uint8_t leftSpeed;
        uint8_t rightSpeed;
    }PWM_DUAL_MOTOR_SPEED;

    typedef struct{
        uint8_t turnTimes;
		uint8_t speed_gain;
		uint8_t speed_divid;
		uint8_t is_left;
    }PWM_DUAL_MOTOR_TURN;


    void Pwm_Speed_Init(void);
    uint8_t Pwm_Speed_Get_Turn_Direction(void);
	void Compass_Angel_adjust_Handle(void);
	void nowPwmAccelerator_Set(uint8_t accelerator);

	uint8_t Pwm_Speed_Get_left(void);
	uint8_t Pwm_Speed_Get_right(void);

	void Pwm_adjust_reset(void);
	void comPass_cache_init_angel(void);
	void comPass_cache_init_angel_delay(void);
		
	extern	int16_t idata nowAveAngel,startAveAngel;
	
#endif
