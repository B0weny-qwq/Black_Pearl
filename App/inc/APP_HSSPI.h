/**
 * @file    APP_HSSPI.h
 * @brief   高速 SPI 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示高速 SPI 外设。当前无线驱动使用专用 `lt8920` 端口实现，
 * 不直接依赖本示例。
 */

#ifndef __APP_HSSPI_H_
#define __APP_HSSPI_H_

#include "config.h"

void Sample_HSSPI(void);
void HSSPI_init(void);

#endif

