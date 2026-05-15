/**
 * @file    Motor.h
 * @brief   Dual DC motor PWM driver for STC32G PWMA channel 3/4.
 * @author  boweny
 * @date    2026-04-27
 * @version v1.0
 *
 * @hardware
 *   Left motor:
 *     PWM3N_2 -> MLA -> P2.5
 *     PWM3P_2 -> MLB -> P2.4
 *   Right motor:
 *     PWM4N_2 -> MRA -> P2.7
 *     PWM4P_2 -> MRB -> P2.6
 *
 * @note
 *   MRA/MLA are N outputs, MRB/MLB are P outputs. Positive speed uses P as
 *   the active side; negative speed swaps polarity and uses N as active side.
 */

#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "..\..\..\User\Config.h"

#define MOTOR_PWM_PERIOD        1000U
#define MOTOR_SPEED_MAX         1000
#define MOTOR_DEFAULT_DEADTIME  0

typedef enum
{
    MOTOR_LEFT = 0,
    MOTOR_RIGHT = 1
} Motor_Id_t;

void Motor_Init(void);
void Motor_SetSpeed(Motor_Id_t motor, int16 speed);
void Motor_SetBothSpeed(int16 left_speed, int16 right_speed);
void Motor_Stop(Motor_Id_t motor);
void Motor_StopAll(void);
int16 Motor_GetSpeed(Motor_Id_t motor);

#endif
