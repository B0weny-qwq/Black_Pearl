/**
 * @file    APP_SPI_PS.h
 * @brief   SPI 轮询示例接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本示例演示基础 SPI 轮询收发。当前项目无线链路已由 `lt8920` 与对应端口层接管，
 * 不依赖本示例作为业务入口。
 *
 * @note 当前职责边界：
 * - 仅作官方外设示例；
 * - 不直接服务当前无线协议或电机控制逻辑。
 */

#ifndef __APP_SPI_PS_H_
#define __APP_SPI_PS_H_

#include "config.h"

void Sample_SPI_PS(void);
void SPI_PS_init(void);

#endif

