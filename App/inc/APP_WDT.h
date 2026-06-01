/**
 * @file    APP_WDT.h
 * @brief   看门狗示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示看门狗初始化与喂狗流程。当前项目默认未把它接入主循环，
 * 但后续若需要增强抗死机能力，可参考此模块。
 *
 * @note 当前职责边界：
 * - 只提供 WDT 示例；
 * - 当前主链路是否喂狗由上层调度策略决定。
 */

#ifndef __APP_WDT_H_
#define __APP_WDT_H_

#include "config.h"

void Sample_WDT(void);
void WDT_init(void);

#endif

