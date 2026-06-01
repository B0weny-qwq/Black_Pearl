/**
 * @file    APP_AD_UART.h
 * @brief   ADC 转 UART 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示 ADC 采样并通过串口输出结果。当前项目默认禁用该模块，
 * 因为其示例资源分配可能与 GPS 所使用的 UART2/Timer2 链路冲突。
 *
 * @note 当前职责边界：
 * - 仅作官方示例参考；
 * - 当前工程主链路不依赖本模块；
 * - 重新启用前必须重新核对 Timer2 与 UART2 资源占用。
 */

#ifndef __ADTOUART_H_
#define __ADTOUART_H_

#include	"config.h"

void ADtoUART_init(void);
void Sample_ADtoUART(void);

#endif

