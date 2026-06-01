/**
 * @file    APP_PWM.h
 * @brief   PWMA/PWMB 输出示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本头文件声明官方 PWM 输出示例。当前项目真实电机输出由
 * `Code_boweny/Device/Motor` 结合底层 PWM 驱动管理，不直接依赖本示例。
 *
 * @note 当前职责边界：
 * - 仅用于 PWM 示例演示；
 * - 不负责当前船体左右电机控制策略。
 */

#ifndef __APP_PWM_H_
#define __APP_PWM_H_

#include "config.h"

void Sample_PWMA_Output(void);
void PWMA_Output_init(void);

void Sample_PWMB_Output(void);
void PWMB_Output_init(void);

#endif

