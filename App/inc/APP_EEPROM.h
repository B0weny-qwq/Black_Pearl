/**
 * @file    APP_EEPROM.h
 * @brief   EEPROM 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示通过串口命令访问片上 EEPROM。当前项目涉及校准数据持久化时，
 * 主要由上层业务模块自行组织数据格式，本示例默认不在主链路启用。
 *
 * @note 当前职责边界：
 * - 仅演示 EEPROM 基础访问；
 * - 不定义项目级校准数据协议或 A/B 槽策略。
 */

#ifndef __APP_EEPROM_H_
#define __APP_EEPROM_H_

#include "config.h"

void Sample_EEPROM(void);
void EEPROM_init(void);

#endif

