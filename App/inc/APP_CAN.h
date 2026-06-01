/**
 * @file    APP_CAN.h
 * @brief   CAN 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示 STC32G CAN 外设。当前船控项目无线链路不依赖 CAN，总体默认禁用。
 */

#ifndef __APP_CAN_H_
#define __APP_CAN_H_

#include	"config.h"

void CAN_init(void);
void Sample_CAN(void);

#endif

