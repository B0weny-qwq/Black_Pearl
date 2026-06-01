/**
 * @file    APP_RTC.h
 * @brief   RTC 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示 STC32G RTC 外设的初始化与简单使用。当前项目主链路不依赖该示例，
 * 保留它主要用于后续参考或独立实验。
 *
 * @note 当前职责边界：
 * - 只负责 RTC 示例；
 * - 不参与当前导航、姿态或无线控制链路。
 */

#ifndef __APP_RTC_H_
#define __APP_RTC_H_

#include "config.h"

void Sample_RTC(void);
void RTC_init(void);

#endif

