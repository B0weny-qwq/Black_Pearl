/**
 * @file    ShipControl.h
 * @brief   船体统一运动控制层接口。
 *
 * @details
 * 本模块统一拥有最终电机输出权，负责仲裁手动开环、手动航向自稳、
 * E 键定速巡航和 GPS 航向保持请求。无线协议和自动驾驶模块只提交
 * 输入或目标航向，不直接写电机。
 */

#ifndef __SHIP_CONTROL_H__
#define __SHIP_CONTROL_H__

#include "config.h"

/**
 * @brief 船体控制模式。
 */
typedef enum
{
    SHIP_CONTROL_MODE_STOP = 0,              /**< 停止状态，电机目标为 0。 */
    SHIP_CONTROL_MODE_MANUAL_OPEN_LOOP,      /**< 手动开环控制，摇杆直接混合为左右电机目标。 */
    SHIP_CONTROL_MODE_MANUAL_YAW_HOLD,       /**< 手动直线航向保持，使用进入自稳时锁定的航向闭环修正。 */
    SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD,   /**< E 键定速巡航，保持进入巡航时的航向。 */
    SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD,  /**< GPS 导航航向保持，由 AutoDrive 持续提交目标航向。 */
    SHIP_CONTROL_MODE_FAILSAFE_STOP          /**< 失效保护停止状态。 */
} ShipControl_Mode_t;

/**
 * @brief 停止控制输出的原因码。
 */
typedef enum
{
    SHIP_CONTROL_STOP_REASON_NONE = 0,       /**< 无明确停止原因。 */
    SHIP_CONTROL_STOP_REASON_MANUAL_CENTER,  /**< 手动摇杆回中触发停止。 */
    SHIP_CONTROL_STOP_REASON_MANUAL_TIMEOUT, /**< 手动控制帧超时触发停止。 */
    SHIP_CONTROL_STOP_REASON_REMOTE_TIMEOUT, /**< 遥控链路超时触发停止。 */
    SHIP_CONTROL_STOP_REASON_CRUISE_KEY,     /**< 巡航按键或巡航退出条件触发停止。 */
    SHIP_CONTROL_STOP_REASON_GPS_NAV_STOP,   /**< GPS 导航主动退出触发停止。 */
    SHIP_CONTROL_STOP_REASON_HEADING_LOST,   /**< 航向不可用，闭环控制无法继续。 */
    SHIP_CONTROL_STOP_REASON_FAILSAFE        /**< 失效保护触发停止。 */
} ShipControl_StopReason_t;

/**
 * @brief 初始化统一控制层运行状态和 PID 控制器。
 */
void ShipControl_Init(void);

/**
 * @brief 控制层周期服务函数。
 *
 * @param now_ms 当前系统 tick，单位 ms。
 *
 * @details
 * 周期刷新手动控制和定速巡航闭环输出，同时按配置限频打印控制日志。
 */
void ShipControl_Tick(u32 now_ms);

/**
 * @brief 更新最新遥控输入。
 *
 * @param lr     左右摇杆原始值，中心值为 100。
 * @param ud     前后摇杆原始值，中心值为 100。
 * @param key    遥控按键位。
 * @param now_ms 收到该输入时的系统 tick，单位 ms。
 *
 * @details
 * 该接口只提交最新输入。实际电机输出仍由控制层按内部节拍刷新。
 */
void ShipControl_UpdateManualInput(u8 lr, u8 ud, u8 key, u32 now_ms);

/**
 * @brief 请求进入定速航向保持。
 *
 * @param heading_cd 目标航向，单位 0.01 度，范围会被归一化到 [0, 36000)。
 * @param base_speed 巡航基础速度，正值前进，负值后退，0 使用默认巡航速度。
 */
void ShipControl_RequestCruise(u16 heading_cd, int16 base_speed);

/**
 * @brief 请求 GPS 原地对准目标航向。
 *
 * @param target_heading_cd 目标航向，单位 0.01 度。
 *
 * @details
 * 对准阶段基础速度为 0，仅输出受限差速用于原地转向。
 */
void ShipControl_RequestGpsAlign(u16 target_heading_cd);

/**
 * @brief 请求 GPS 导航航向保持。
 *
 * @param target_heading_cd AutoDrive 计算出的目标航向，单位 0.01 度。
 * @param base_speed        导航基础速度，正值前进，负值后退。
 */
void ShipControl_RequestGpsNav(u16 target_heading_cd, int16 base_speed);

/**
 * @brief 停止所有控制输出并复位闭环状态。
 *
 * @param reason 停止原因，取值见 @ref ShipControl_StopReason_t。
 */
void ShipControl_Stop(u8 reason);

/**
 * @brief 如果当前处于 GPS 导航模式，则停止 GPS 导航输出。
 */
void ShipControl_StopGpsNav(void);

/**
 * @brief 复位航向保持 PID、目标航向和限斜率状态。
 */
void ShipControl_ResetYawHoldController(void);

/**
 * @brief 判断当前是否处于自动类模式。
 *
 * @return 1 表示定速巡航或 GPS 导航中，0 表示非自动模式。
 */
u8 ShipControl_IsAutoMode(void);

/**
 * @brief 获取当前控制模式。
 *
 * @return 当前模式，取值见 @ref ShipControl_Mode_t。
 */
u8 ShipControl_GetMode(void);

/**
 * @brief 获取用于上层判断的手动前进油门幅度。
 *
 * @return 前进油门幅度，0 表示无有效前进油门。
 */
u8 ShipControl_GetManualAccelerator(void);

#endif
