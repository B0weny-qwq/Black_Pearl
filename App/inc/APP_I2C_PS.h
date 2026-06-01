/**
 * @file    APP_I2C_PS.h
 * @brief   I2C 轮询示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示基于硬件 I2C 的轮询式访问。当前工程真正的传感器访问由
 * `Code_boweny/Device/QMI8658`、`QMC6309` 等模块负责，本示例默认不进入主链路。
 *
 * @note 当前职责边界：
 * - 仅提供 I2C 示例调用方式；
 * - 不承载具体设备协议、姿态解算或导航逻辑。
 */

#ifndef __APP_I2C_PS_H_
#define __APP_I2C_PS_H_

#include "config.h"

void Sample_I2C_PS(void);
void I2C_PS_init(void);

#endif

