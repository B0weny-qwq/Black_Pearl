/**
 * @file    APP_DMA_SPI_PS.h
 * @brief   DMA + SPI 轮询示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示 DMA 协助 SPI 传输。当前无线与传感器业务均未直接走此示例。
 */

#ifndef __APP_DMA_SPI_PS_H_
#define __APP_DMA_SPI_PS_H_

#include "config.h"

void Sample_DMA_SPI_PS(void);
void DMA_SPI_PS_init(void);

#endif

