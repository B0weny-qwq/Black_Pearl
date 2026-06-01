/**
 * @file    Task.h
 * @brief   软定时任务调度与系统毫秒 tick 接口。
 *
 * @details
 * 本文件保留原工程任务调度组件定义，并对外暴露 `Task_GetTickMs()`。
 * 当前项目里，Timer0 毫秒 tick 除了服务通用任务调度，还被 AHRS、
 * QMI8658 状态机、NorthCalib 和 ShipProtocol 用于超时与周期判断。
 */
/**
 * @note 当前职责边界：
 * - 本头文件对外暴露 Timer0 毫秒 tick 与兼容任务钩子接口。
 * - 当前真实运行期业务已下沉到 MainLoop 和模块自调度器。
 */
#ifndef		__TASK_H
#define		__TASK_H

#include	"config.h"

typedef struct 
{
	u8 Run;               //任务状态：Run/Stop
	u16 TIMCount;         //定时计数器
	u16 TRITime;          //重载计数器
	void (*TaskHook) (void); //任务函数
} TASK_COMPONENTS;       

void Task_Marks_Handler_Callback(void);
void Task_Pro_Handler_Callback(void);

/**
 * @brief   获取 Timer0 维护的系统毫秒计数
 * @return  系统启动后的毫秒 tick，32 位自然回绕
 *
 * @details
 * 该接口在内部短暂关闭总中断，保证 32 位 tick 读取过程不会被 Timer0 ISR 打断。
 * 当前主要用于 AHRS 姿态融合计算真实 `dt_ms`。
 */
u32 Task_GetTickMs(void);

#endif
