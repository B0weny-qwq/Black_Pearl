/**
 * @file    APP_Lamp.h
 * @brief   板载灯光与数码管示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 当前 `App/` 层中，`Lamp` 是仍然保留启用的少数官方示例模块之一，
 * 主要提供板载灯光、数码管和按键的基础演示能力，供上电观察与简单调试使用。
 *
 * @note 当前职责边界：
 * - 本模块仅负责板级显示/按键示例；
 * - 不参与船控、无线、AHRS、GPS 等业务逻辑；
 * - 若复用其 IO 资源，需要先核对是否与当前硬件分配冲突。
 */

#ifndef __LAMP_H_
#define __LAMP_H_

#include "config.h"

void Lamp_init(void);
void Sample_Lamp(void);

#endif

