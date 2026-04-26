/**
 * @file    PID.h
 * @brief   Fixed-point PID controller.
 * @author  boweny
 * @date    2026-04-27
 * @version v1.0
 *
 * @details
 * - Provides a reusable positional PID controller.
 * - Uses Q10 fixed-point gains, no floating-point dependency.
 * - Supports output clamp and integral clamp for anti-windup.
 */

#ifndef __PID_H__
#define __PID_H__

#include "config.h"

#define PID_GAIN_Q              10
#define PID_GAIN_SCALE          (1L << PID_GAIN_Q)

#define PID_GAIN_FROM_INT(x)    ((int16)((x) << PID_GAIN_Q))
#define PID_GAIN_FROM_Q10(x)    ((int16)(x))

typedef struct
{
    int16 kp;
    int16 ki;
    int16 kd;

    int16 target;
    int16 output_min;
    int16 output_max;
    int32 integral_min;
    int32 integral_max;

    int32 integral;
    int16 prev_error;
    u8 initialized;
} PID_Controller_t;

void PID_Init(PID_Controller_t *pid,
              int16 kp, int16 ki, int16 kd,
              int16 output_min, int16 output_max,
              int32 integral_min, int32 integral_max);

void PID_Reset(PID_Controller_t *pid);
void PID_SetTarget(PID_Controller_t *pid, int16 target);
void PID_SetGains(PID_Controller_t *pid, int16 kp, int16 ki, int16 kd);
void PID_SetOutputLimit(PID_Controller_t *pid, int16 output_min, int16 output_max);
void PID_SetIntegralLimit(PID_Controller_t *pid, int32 integral_min, int32 integral_max);
int16 PID_Update(PID_Controller_t *pid, int16 measured);
int16 PID_UpdateTarget(PID_Controller_t *pid, int16 target, int16 measured);

#endif
