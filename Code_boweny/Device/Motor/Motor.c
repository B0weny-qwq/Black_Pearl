/**
 * @file    Motor.c
 * @brief   Dual DC motor PWM driver for PWMA3/PWMA4 complementary outputs.
 * @author  boweny
 * @date    2026-04-27
 * @version v1.0
 */

#include "Motor.h"
#include "..\..\..\Driver\inc\STC32G_PWM.h"
#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\..\Driver\inc\STC32G_NVIC.h"

static PWMx_Duty g_motor_pwm_duty;
static int16 g_left_speed = 0;
static int16 g_right_speed = 0;

static u16 Motor_AbsSpeedToDuty(int16 speed)
{
    u16 abs_speed;

    if (speed < 0) {
        if (speed < -MOTOR_SPEED_MAX) {
            speed = -MOTOR_SPEED_MAX;
        }
        abs_speed = (u16)(-speed);
    } else {
        if (speed > MOTOR_SPEED_MAX) {
            speed = MOTOR_SPEED_MAX;
        }
        abs_speed = (u16)speed;
    }

    return (u16)(((u32)abs_speed * MOTOR_PWM_PERIOD) / MOTOR_SPEED_MAX);
}

static int16 Motor_LimitSpeed(int16 speed)
{
    if (speed > MOTOR_SPEED_MAX) {
        return MOTOR_SPEED_MAX;
    }
    if (speed < -MOTOR_SPEED_MAX) {
        return -MOTOR_SPEED_MAX;
    }
    return speed;
}

static void Motor_LeftOutputEnable(u8 enable)
{
    if (enable) {
        PWMA_CC3E_Enable();
        PWMA_CC3NE_Enable();
        PWMA_ENO |= (ENO3P | ENO3N);
    } else {
        PWMA_ENO &= (u8)~(ENO3P | ENO3N);
        PWMA_CC3E_Disable();
        PWMA_CC3NE_Disable();
    }
}

static void Motor_RightOutputEnable(u8 enable)
{
    if (enable) {
        PWMA_CC4E_Enable();
        PWMA_CC4NE_Enable();
        PWMA_ENO |= (ENO4P | ENO4N);
    } else {
        PWMA_ENO &= (u8)~(ENO4P | ENO4N);
        PWMA_CC4E_Disable();
        PWMA_CC4NE_Disable();
    }
}

static void Motor_LeftSetForwardPolarity(u8 forward)
{
    if (forward) {
        PWMA_CC3P_HighValid();
        PWMA_CC3NP_LowValid();
    } else {
        PWMA_CC3P_LowValid();
        PWMA_CC3NP_HighValid();
    }
}

static void Motor_RightSetForwardPolarity(u8 forward)
{
    if (forward) {
        PWMA_CC4P_HighValid();
        PWMA_CC4NP_LowValid();
    } else {
        PWMA_CC4P_LowValid();
        PWMA_CC4NP_HighValid();
    }
}

void Motor_Init(void)
{
    PWMx_InitDefine pwm_init;

    P2_MODE_IO_PU(GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
    PWM3_USE_P24P25();
    PWM4_USE_P26P27();

    g_motor_pwm_duty.PWM1_Duty = 0;
    g_motor_pwm_duty.PWM2_Duty = 0;
    g_motor_pwm_duty.PWM3_Duty = 0;
    g_motor_pwm_duty.PWM4_Duty = 0;

    pwm_init.PWM_Mode = CCMRn_PWM_MODE1;
    pwm_init.PWM_Duty = 0;
    pwm_init.PWM_EnoSelect = 0;
    PWM_Configuration(PWM3, &pwm_init);
    PWM_Configuration(PWM4, &pwm_init);

    pwm_init.PWM_Period = MOTOR_PWM_PERIOD;
    pwm_init.PWM_DeadTime = MOTOR_DEFAULT_DEADTIME;
    pwm_init.PWM_MainOutEnable = ENABLE;
    pwm_init.PWM_CEN_Enable = ENABLE;
    PWM_Configuration(PWMA, &pwm_init);

    PWMA_OC3_OUT_0();
    PWMA_OC3N_OUT_0();
    PWMA_OC4_OUT_0();
    PWMA_OC4N_OUT_0();

    Motor_LeftSetForwardPolarity(1);
    Motor_RightSetForwardPolarity(1);
    Motor_StopAll();

    NVIC_PWM_Init(PWMA, DISABLE, Priority_0);
}

void Motor_SetSpeed(Motor_Id_t motor, int16 speed)
{
    u16 duty;

    speed = Motor_LimitSpeed(speed);
    duty = Motor_AbsSpeedToDuty(speed);

    if (motor == MOTOR_LEFT) {
        if (speed == 0) {
            Motor_Stop(MOTOR_LEFT);
            return;
        }

        Motor_LeftOutputEnable(0);
        Motor_LeftSetForwardPolarity((u8)(speed > 0));
        g_motor_pwm_duty.PWM3_Duty = duty;
        UpdatePwm(PWM3, &g_motor_pwm_duty);
        Motor_LeftOutputEnable(1);
        g_left_speed = speed;
    } else {
        if (speed == 0) {
            Motor_Stop(MOTOR_RIGHT);
            return;
        }

        Motor_RightOutputEnable(0);
        Motor_RightSetForwardPolarity((u8)(speed > 0));
        g_motor_pwm_duty.PWM4_Duty = duty;
        UpdatePwm(PWM4, &g_motor_pwm_duty);
        Motor_RightOutputEnable(1);
        g_right_speed = speed;
    }
}

void Motor_SetBothSpeed(int16 left_speed, int16 right_speed)
{
    Motor_SetSpeed(MOTOR_LEFT, left_speed);
    Motor_SetSpeed(MOTOR_RIGHT, right_speed);
}

void Motor_Stop(Motor_Id_t motor)
{
    if (motor == MOTOR_LEFT) {
        Motor_LeftOutputEnable(0);
        g_motor_pwm_duty.PWM3_Duty = 0;
        UpdatePwm(PWM3, &g_motor_pwm_duty);
        g_left_speed = 0;
    } else {
        Motor_RightOutputEnable(0);
        g_motor_pwm_duty.PWM4_Duty = 0;
        UpdatePwm(PWM4, &g_motor_pwm_duty);
        g_right_speed = 0;
    }
}

void Motor_StopAll(void)
{
    Motor_Stop(MOTOR_LEFT);
    Motor_Stop(MOTOR_RIGHT);
}

int16 Motor_GetSpeed(Motor_Id_t motor)
{
    if (motor == MOTOR_LEFT) {
        return g_left_speed;
    }
    return g_right_speed;
}
