#ifndef __AUTODRIVE_CFG_H__
#define __AUTODRIVE_CFG_H__

#include "autodrive.h"

void AutoDriveCfg_Init(void);
void AutoDriveCfg_Load(AutoDrive_ReturnConfig_t *cfg);
u8 AutoDriveCfg_Save(const AutoDrive_ReturnConfig_t *cfg);

#endif
