/**
 * @file    Motor.h
 * @brief   双直流电机 PWM 驱动接口。
 * @author  boweny
 * @date    2026-05-05
 * @version v1.0
 *
 * @details
 * 基于 STC32G PWMA 通道 3/4 驱动左右两路直流电机，提供初始化、
 * 单电机调速、双电机调速、停止和速度查询接口。
 *
 * @hardware
 * - 左电机：PWM3N_2 -> MLA -> P2.5，PWM3P_2 -> MLB -> P2.4
 * - 右电机：PWM4N_2 -> MRA -> P2.7，PWM4P_2 -> MRB -> P2.6
 *
 * @note
 * 速度为正时使用 P 输出侧作为有效驱动侧，速度为负时交换极性。
 *
 * @see     Code_boweny/Device/Motor/Motor.c
 */

#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "..\..\..\User\Config.h"

#define MOTOR_PWM_PERIOD        1000U  /**< PWM 周期计数值。 */
#define MOTOR_SPEED_MAX         1000   /**< 电机速度绝对值上限。 */
#define MOTOR_DEFAULT_DEADTIME  0      /**< 默认 PWM 死区时间配置。 */

/**
 * @brief   电机编号。
 */
typedef enum
{
    MOTOR_LEFT = 0,   /**< 左侧电机。 */
    MOTOR_RIGHT = 1   /**< 右侧电机。 */
} Motor_Id_t;

typedef struct
{
    u16 mla_duty;
    u16 mlb_duty;
    u16 mra_duty;
    u16 mrb_duty;
    u16 period;
} Motor_PwmSnapshot_t;

/**
 * @brief   初始化电机 PWM 输出和内部速度状态。
 * @return  none
 */
void Motor_Init(void);

/**
 * @brief      设置指定电机速度。
 * @param[in]  motor  电机编号，取值见 Motor_Id_t。
 * @param[in]  speed  目标速度，范围 -MOTOR_SPEED_MAX ~ MOTOR_SPEED_MAX。
 * @return     none
 */
void Motor_SetSpeed(Motor_Id_t motor, int16 speed);

/**
 * @brief      同时设置左右电机速度。
 * @param[in]  left_speed   左电机目标速度。
 * @param[in]  right_speed  右电机目标速度。
 * @return     none
 */
void Motor_SetBothSpeed(int16 left_speed, int16 right_speed);

/**
 * @brief      停止指定电机。
 * @param[in]  motor  电机编号，取值见 Motor_Id_t。
 * @return     none
 */
void Motor_Stop(Motor_Id_t motor);

/**
 * @brief   停止左右两路电机。
 * @return  none
 */
void Motor_StopAll(void);

/**
 * @brief      获取指定电机当前记录速度。
 * @param[in]  motor  电机编号，取值见 Motor_Id_t。
 * @return     当前速度；电机编号非法时返回 0。
 */
int16 Motor_GetSpeed(Motor_Id_t motor);

void Motor_GetPwmSnapshot(Motor_PwmSnapshot_t *snapshot);

#endif
