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

static u16 Motor_SpeedToDuty(int16 speed)
{
    int32 duty;

    if (speed < 0) {
        if (speed < -MOTOR_SPEED_MAX) {
            speed = -MOTOR_SPEED_MAX;
        }
    } else {
        if (speed > MOTOR_SPEED_MAX) {
            speed = MOTOR_SPEED_MAX;
        }
    }

    duty = (int32)(MOTOR_PWM_PERIOD / 2U);
    duty += ((int32)speed * (int32)(MOTOR_PWM_PERIOD / 2U)) / (int32)MOTOR_SPEED_MAX;

    if (duty < 0) {
        duty = 0;
    } else if (duty > (int32)MOTOR_PWM_PERIOD) {
        duty = (int32)MOTOR_PWM_PERIOD;
    }

    return (u16)duty;
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
    (void)forward;

    /* Left motor wiring is fixed on the board:
     * PWM3N -> MLA -> HIN, PWM3P -> MLB -> LIN#
     * To get a real complementary pair at the pins, HIN must see the
     * active-high phase while LIN# sees the active-low phase. */
    PWMA_CC3P_LowValid();
    PWMA_CC3NP_LowValid();
}

static void Motor_RightSetForwardPolarity(u8 forward)
{
    (void)forward;

    /* Right motor wiring is mirrored on the board:
     * PWM4P -> MRB -> HIN, PWM4N -> MRA -> LIN#
     * Keep both outputs in high-valid polarity so the P/N hardware
     * complement maps directly to HIN/LIN# active levels. */
    PWMA_CC4P_HighValid();
    PWMA_CC4NP_HighValid();
}

void Motor_Init(void)
{
    PWMx_InitDefine pwm_init;

    P2_MODE_OUT_PP(GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
    P2_PULL_UP_DISABLE(GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
    PWM3_USE_P24P25();
    PWM4_USE_P26P27();

    g_motor_pwm_duty.PWM1_Duty = 0;
    g_motor_pwm_duty.PWM2_Duty = 0;
    g_motor_pwm_duty.PWM3_Duty = MOTOR_PWM_PERIOD / 2U;
    g_motor_pwm_duty.PWM4_Duty = MOTOR_PWM_PERIOD / 2U;

    pwm_init.PWM_Mode = CCMRn_PWM_MODE1;
    pwm_init.PWM_Duty = MOTOR_PWM_PERIOD / 2U;
    pwm_init.PWM_Period = MOTOR_PWM_PERIOD;
    pwm_init.PWM_DeadTime = MOTOR_DEFAULT_DEADTIME;
    pwm_init.PWM_MainOutEnable = DISABLE;
    pwm_init.PWM_CEN_Enable = DISABLE;

    /* Configure both P/N outputs as a complementary pair up front so
     * PWM3/4 do not start from a half-configured state. */
    pwm_init.PWM_EnoSelect = ENO3P | ENO3N;
    PWM_Configuration(PWM3, &pwm_init);
    pwm_init.PWM_EnoSelect = ENO4P | ENO4N;
    PWM_Configuration(PWM4, &pwm_init);

    pwm_init.PWM_MainOutEnable = ENABLE;
    pwm_init.PWM_CEN_Enable = ENABLE;
    PWM_Configuration(PWMA, &pwm_init);

    PWMA_OC3_OUT_0();
    PWMA_OC3N_OUT_0();
    PWMA_OC4_OUT_0();
    PWMA_OC4N_OUT_0();

    Motor_LeftSetForwardPolarity(1);
    Motor_RightSetForwardPolarity(1);
    Motor_LeftOutputEnable(1);
    Motor_RightOutputEnable(1);
    Motor_StopAll();

    NVIC_PWM_Init(PWMA, DISABLE, Priority_0);
}

void Motor_SetSpeed(Motor_Id_t motor, int16 speed)
{
    u16 duty;

    speed = Motor_LimitSpeed(speed);
    duty = Motor_SpeedToDuty(speed);

    if (motor == MOTOR_LEFT) {
        g_motor_pwm_duty.PWM3_Duty = duty;
        UpdatePwm(PWM3, &g_motor_pwm_duty);
        g_left_speed = speed;
    } else {
        g_motor_pwm_duty.PWM4_Duty = duty;
        UpdatePwm(PWM4, &g_motor_pwm_duty);
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
        g_motor_pwm_duty.PWM3_Duty = MOTOR_PWM_PERIOD / 2U;
        UpdatePwm(PWM3, &g_motor_pwm_duty);
        g_left_speed = 0;
    } else {
        g_motor_pwm_duty.PWM4_Duty = MOTOR_PWM_PERIOD / 2U;
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
