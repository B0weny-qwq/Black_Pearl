/**
 * @file    APP_INT_UART.h
 * @brief   外部中断转串口示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例用于演示外部中断事件与串口打印联动。当前项目默认不启用，
 * 仅保留作为官方参考实现。
 *
 * @note 当前职责边界：
 * - 属于示例模块，不在当前船控主链路中运行；
 * - 若恢复启用，需要检查日志串口和中断分配是否与现有业务冲突。
 */

#ifndef __INTTOUART_H_
#define __INTTOUART_H_

#include "config.h"

void INTtoUART_init(void);
void Sample_INTtoUART(void);

#endif

