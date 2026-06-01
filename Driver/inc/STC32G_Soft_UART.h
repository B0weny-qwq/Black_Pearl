/**
 * @file    STC32G_Soft_UART.h
 * @brief   STC32G 软件串口接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本头文件提供软件模拟 UART 的基础接口。当前项目核心串口通信均使用硬件 UART，
 * 软件串口未进入主链路。
 */

#ifndef	__STC32G_SOFT_UART_H
#define	__STC32G_SOFT_UART_H

#include	"config.h"

void	TxSend(u8 dat);
void 	PrintString(unsigned char code *puts);

#endif
