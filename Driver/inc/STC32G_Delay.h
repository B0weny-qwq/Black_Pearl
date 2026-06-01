/**
 * @file    STC32G_Delay.h
 * @brief   STC32G 延时接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本头文件提供简单阻塞延时接口。当前项目在设备上电、复位和少量外设初始化阶段
 * 仍可能使用这些能力，但主循环实时逻辑不应过度依赖阻塞延时。
 */

#ifndef	__STC32G_DELAY_H
#define	__STC32G_DELAY_H

#include	"config.h"

void delay_ms(unsigned int ms);

#endif
