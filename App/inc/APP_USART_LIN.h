/**
 * @file    APP_USART_LIN.h
 * @brief   UART1 LIN 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示基于 UART1 的 LIN 扩展。当前 UART1 已主要用于日志输出，
 * 因此本示例默认不启用。
 */

#ifndef __APP_USART_LIN_H_
#define __APP_USART_LIN_H_

#include	"config.h"

void USART_LIN_init(void);
void Sample_USART_LIN(void);

#endif

