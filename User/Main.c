/**
 * @file    Main.c
 * @brief   固件主入口。
 *
 * @details
 * 当前主入口只负责串起启动初始化和主循环：
 * `SYS_Init() -> MainLoop_Bootstrap() -> while(1) MainLoop_RunOnce()`
 *
 * 业务调度、传感器轮询、无线协议、AutoDrive 和 AHRS 都不直接写在这里，
 * 而是统一下沉到 MainLoop。
 */
#include "config.h"
#include "system_init.h"
#include "MainLoop.h"

/*---------------------------------------------------------------------*/
/* 主入口                                                              */
/*---------------------------------------------------------------------*/
void main(void)
{
    SYS_Init();
    MainLoop_Bootstrap();

    /* 所有运行期业务统一在 MainLoop_RunOnce() 内推进。 */
    while (1)
    {
        MainLoop_RunOnce();
    }
}
