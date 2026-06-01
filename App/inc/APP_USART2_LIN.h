/**
 * @file    APP_USART2_LIN.h
 * @brief   UART2 LIN 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示基于 UART2 的 LIN 扩展。当前 UART2 已被 GPS 接收链路占用，
 * 因此本示例在项目中必须保持关闭。
 *
 * @note 当前职责边界：
 * - 仅作 LIN 示例参考；
 * - 不能与当前 GPS UART2 路由同时启用。
 */

#ifndef __APP_USART2_LIN_H_
#define __APP_USART2_LIN_H_

#include	"config.h"

void USART2_LIN_init(void);
void Sample_USART2_LIN(void);

#endif

