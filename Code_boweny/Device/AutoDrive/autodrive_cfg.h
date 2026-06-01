/**
 * @file    autodrive_cfg.h
 * @brief   AutoDrive 配置存取接口。
 *
 * @note 当前职责边界：
 * - 该接口只描述返航配置的读取/保存。
 * - AutoDrive 业务状态机与控制输出不在本头文件中实现。
 */
#ifndef __AUTODRIVE_CFG_H__
#define __AUTODRIVE_CFG_H__

#include "autodrive.h"

void AutoDriveCfg_Init(void);
void AutoDriveCfg_Load(AutoDrive_ReturnConfig_t *cfg);
u8 AutoDriveCfg_Save(const AutoDrive_ReturnConfig_t *cfg);

#endif
