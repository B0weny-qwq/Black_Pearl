/**
 * @file    Motor.c
 * @brief   板级双直流电机 PWM 执行层。
 * @author  boweny
 * @date    2026-04-27
 * @version v1.1
 *
 * @details
 * 本文件只负责把带符号的左右电机速度命令转换为当前硬件板级所需的
 * PWMA3/PWMA4 互补输出形式。
 *
 * @note 当前职责边界：
 * - ShipControl 负责给出最终左右电机目标。
 * - Motor.c 负责限幅、映射并将目标写入 PWM 硬件。
 */

#include "Motor.h"
#include "..\..\..\Driver\inc\STC32G_PWM.h"
#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\..\Driver\inc\STC32G_NVIC.h"

#ifndef MOTOR_PWM_EDGE_MARGIN
#define MOTOR_PWM_EDGE_MARGIN  40U
#endif

static PWMx_Duty g_motor_pwm_duty;
static int16 g_left_speed = 0;
static int16 g_right_speed = 0;
static int16 g_left_target_speed = 0;
static int16 g_right_target_speed = 0;
static u8 g_motor_ready = 0U;

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

    if (duty < (int32)MOTOR_PWM_EDGE_MARGIN) {
        duty = (int32)MOTOR_PWM_EDGE_MARGIN;
    } else if (duty > (int32)(MOTOR_PWM_PERIOD - MOTOR_PWM_EDGE_MARGIN)) {
        duty = (int32)(MOTOR_PWM_PERIOD - MOTOR_PWM_EDGE_MARGIN);
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

static void Motor_LeftSetForwardPolarity(void)
{
    /* 左电机接线固定为 PWM3N->MLA->HIN、PWM3P->MLB->LIN#；
     * 为得到真实互补输出，HIN 接收高有效相位，LIN# 接收低有效相位。 */
    PWMA_CC3P_LowValid();
    PWMA_CC3NP_LowValid();
}

static void Motor_RightSetForwardPolarity(void)
{
    /* 右电机接线与左侧镜像：PWM4P->MRB->HIN、PWM4N->MRA->LIN#；
     * 两路保持高有效，使 P/N 硬件互补直接映射到 HIN/LIN# 有效电平。 */
    PWMA_CC4P_HighValid();
    PWMA_CC4NP_HighValid();
}

static void Motor_ApplySpeed(Motor_Id_t motor, int16 speed)
{
    u16 duty;

    speed = Motor_LimitSpeed(speed);
    if (speed == 0) {
        if (motor == MOTOR_LEFT) {
            g_motor_pwm_duty.PWM3_Duty = MOTOR_PWM_PERIOD / 2U;
            UpdatePwm(PWM3, &g_motor_pwm_duty);
            Motor_LeftOutputEnable(0);
            g_left_speed = 0;
        } else {
            g_motor_pwm_duty.PWM4_Duty = MOTOR_PWM_PERIOD / 2U;
            UpdatePwm(PWM4, &g_motor_pwm_duty);
            Motor_RightOutputEnable(0);
            g_right_speed = 0;
        }
        return;
    }

    duty = Motor_SpeedToDuty(speed);

    if (motor == MOTOR_LEFT) {
        g_motor_pwm_duty.PWM3_Duty = duty;
        UpdatePwm(PWM3, &g_motor_pwm_duty);
        Motor_LeftOutputEnable(1);
        g_left_speed = speed;
    } else {
        g_motor_pwm_duty.PWM4_Duty = duty;
        UpdatePwm(PWM4, &g_motor_pwm_duty);
        Motor_RightOutputEnable(1);
        g_right_speed = speed;
    }
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

    /* 先把 P/N 输出成对配置为互补模式，避免 PWM3/4 从半配置状态启动。 */
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

    Motor_LeftSetForwardPolarity();
    Motor_RightSetForwardPolarity();
    Motor_LeftOutputEnable(0);
    Motor_RightOutputEnable(0);
    g_left_speed = 0;
    g_right_speed = 0;
    g_left_target_speed = 0;
    g_right_target_speed = 0;
    g_motor_ready = 1U;
    Motor_StopAll();

    NVIC_PWM_Init(PWMA, DISABLE, Priority_0);
}

void Motor_SetSpeed(Motor_Id_t motor, int16 speed)
{
    speed = Motor_LimitSpeed(speed);
    if (motor == MOTOR_LEFT) {
        g_left_target_speed = speed;
    } else {
        g_right_target_speed = speed;
    }
}

void Motor_SetBothSpeed(int16 left_speed, int16 right_speed)
{
    g_left_target_speed = Motor_LimitSpeed(left_speed);
    g_right_target_speed = Motor_LimitSpeed(right_speed);
}

void Motor_Service(void)
{
    if (g_motor_ready == 0U) {
        return;
    }

    Motor_ApplySpeed(MOTOR_LEFT, g_left_target_speed);
    Motor_ApplySpeed(MOTOR_RIGHT, g_right_target_speed);
}

void Motor_Stop(Motor_Id_t motor)
{
    if (motor == MOTOR_LEFT) {
        g_left_target_speed = 0;
    } else {
        g_right_target_speed = 0;
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

void Motor_GetPwmSnapshot(Motor_PwmSnapshot_t *snapshot)
{
    if (snapshot == 0) {
        return;
    }

    snapshot->period = MOTOR_PWM_PERIOD;

    /* 左电机：PWM3N -> MLA，PWM3P -> MLB，两路低有效。 */
    snapshot->mla_duty = g_motor_pwm_duty.PWM3_Duty;
    snapshot->mlb_duty = (u16)(MOTOR_PWM_PERIOD - g_motor_pwm_duty.PWM3_Duty);

    /* 右电机：PWM4N -> MRA，PWM4P -> MRB，两路高有效。 */
    snapshot->mra_duty = (u16)(MOTOR_PWM_PERIOD - g_motor_pwm_duty.PWM4_Duty);
    snapshot->mrb_duty = g_motor_pwm_duty.PWM4_Duty;
}
