/**
 * @file    Task.c
 * @brief   软定时 tick 维护实现。
 *
 * @details
 * 当前实现只保留 Timer0 毫秒 tick 计数与空的 `Task_Pro_Handler_Callback()`。
 * `Task_GetTickMs()` 已成为运行期公共时间基准，供 AHRS、QMI8658 状态机、
 * ShipProtocol、NorthCalib 和其它超时逻辑复用。
 */
/**
 * @note 当前职责边界：
 * - 本文件只维护 Timer0 毫秒 tick 与兼容任务钩子。
 * - AHRS、NorthCalib、ShipProtocol 等模块复用这里的统一时间基准。
 */
#include "Task.h"

static volatile u32 g_task_tick_ms = 0;

void Task_Marks_Handler_Callback(void)
{
    /** @brief Timer0 中断每 1ms 调用一次该钩子。 */
    g_task_tick_ms++;
}

u32 Task_GetTickMs(void)
{
    u32 tick;
    u8 ea_bak;

    ea_bak = EA;
    EA = 0;
    tick = g_task_tick_ms;
    EA = ea_bak;

    return tick;
}

void Task_Pro_Handler_Callback(void)
{
    /* 保留旧任务槽位以兼容历史工程；当前运行期业务已迁移到
     * MainLoop_RunOnce() 和各模块自调度器。 */
}
