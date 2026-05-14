#ifndef __MAIN_LOOP_H
#define __MAIN_LOOP_H

#include "config.h"

void MainLoop_Bootstrap(void);
void MainLoop_RunOnce(void);
u8 MainLoop_IsHeadingReady(void);
u16 MainLoop_GetHeadingDeg100(void);
int16 MainLoop_GetHeadingRelativeDeg100(void);

#endif
