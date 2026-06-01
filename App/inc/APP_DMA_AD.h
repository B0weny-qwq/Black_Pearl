/**
 * @file    APP_DMA_AD.h
 * @brief   DMA + ADC 示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示 DMA 搬运 ADC 数据。当前项目默认不启用此示例，
 * 仅保留作外设参考。
 */

#ifndef __APP_DMA_AD_H_
#define __APP_DMA_AD_H_

#include	"config.h"

void DMA_AD_init(void);
void Sample_DMA_AD(void);

#endif

