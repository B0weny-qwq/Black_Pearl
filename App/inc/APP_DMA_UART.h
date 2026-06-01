/**
 * @file    APP_DMA_UART.h
 * @brief   DMA + UART 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示 DMA 串口搬运。当前项目的日志和 GPS 串口链路未采用该示例路径，
 * 默认不启用。
 */

#ifndef __DMA_UART_H_
#define __DMA_UART_H_

#include	"config.h"

void DMA_UART_init(void);
void Sample_DMA_UART(void);

#endif

