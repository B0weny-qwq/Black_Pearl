/**
 * @file    MainLoop.h
 * @brief   主循环对外状态查询接口。
 *
 * @details
 * 本头文件暴露当前运行档的统一只读查询接口，供无线协议层、控制层和
 * 自动驾驶读取航向 ready 状态、原始融合航向、北向校准后航向和陀螺角速度。
 *
 * `MainLoop_GetRawHeadingDeg100()` 返回 AHRS/HeadingEstimator 的原始融合航向。
 * `MainLoop_GetHeadingDeg100()` 在原始航向上叠加 `NorthCalib` 输出的
 * `north_offset_cd`，作为手动 yaw 自稳、E 键定速巡航和 GPS 自动巡航共用口径。
 */
#ifndef __MAIN_LOOP_H
#define __MAIN_LOOP_H

#include "config.h"

void MainLoop_Bootstrap(void);
void MainLoop_RunOnce(void);
u8 MainLoop_IsHeadingReady(void);
/* AHRS/HeadingEstimator 输出的原始融合航向，不叠加 NorthCalib 偏移。 */
u16 MainLoop_GetRawHeadingDeg100(void);
/* 叠加 NorthCalib 后的统一导航航向。 */
u16 MainLoop_GetHeadingDeg100(void);
u8 MainLoop_IsMagHeadingFallback(void);
int16 MainLoop_GetHeadingRelativeDeg100(void);
int16 MainLoop_GetGyroZDps100(void);

#endif
