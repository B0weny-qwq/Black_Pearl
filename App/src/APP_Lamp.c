/**
 * @file    APP_Lamp.c
 * @brief   板载灯光示例实现
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件实现最基础的板载 LED 示例，当前 `APP_config()` 仍会调用 `Lamp_init()`，
 * 因而这是少数还留在当前工程初始化链路中的官方 App 示例实现之一。
 *
 * @note 当前职责边界：
 * - 仅负责板级 LED IO 初始化与翻转示例；
 * - 不参与船控、无线、AHRS 或 GPS 业务逻辑。
 */

#include	"APP_Lamp.h"
#include	"STC32G_GPIO.h"

/***************	Function Description	****************

This module blinks a single LED on P3.6.

Download with 24MHz system clock (configurable in config.h).

******************************************/

//========================================================================
// Function: Lamp_init
// Description: User initialization routine.
// Parameter: None.
// Return: None.
// Version: V1.0, 2020-09-28
//========================================================================
void Lamp_init(void)
{
	P3_MODE_OUT_PP(GPIO_Pin_6);		// P3.6 push-pull output
	P36 = 1;						// default idle level
}

//========================================================================
// Function: Sample_Lamp
// Description: User application routine.
// Parameter: None.
// Return: None.
// Version: V1.0, 2020-09-23
//========================================================================
void Sample_Lamp(void)
{
	P36 = !P36;
}
