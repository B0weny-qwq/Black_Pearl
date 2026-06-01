/**
 * @file    System_init.h
 * @brief   板级启动初始化与传感器总线准备接口。
 *
 * @details
 * 本文件定义系统上电初始化入口 `SYS_Init()`，以及供传感器共用的
 * `Sensor_I2C_prepare()` 总线恢复接口。
 *
 * 当前职责边界：
 * - `SYS_Init()` 负责 GPIO、Timer0、ADC、UART、I2C 与中断总开关初始化；
 * - `Sensor_I2C_prepare()` 负责恢复 `P1.4/P1.5` 与传感器 I2C 路由；
 * - 无线/GPS 的业务轮询不在此处进行，而是由 `MainLoop_RunOnce()` 驱动。
 */
#ifndef __SYSTEM_INIT_H_
#define __SYSTEM_INIT_H_

#include "config.h"

void SYS_Init(void);
/* 在访问 QMC6309/QMI8658 前，重新恢复传感器 I2C 复用与上拉状态。 */
void Sensor_I2C_prepare(void);
extern u8 g_qmi8658_ready;

#endif

