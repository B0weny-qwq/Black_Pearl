/**
 * @file    NorthCalib.h
 * @brief   D 键 GPS 北向校准接口。
 *
 * @details
 * 本模块负责双击 D 后的 GPS 航迹北向校准、EEPROM A/B 双槽保存和航向
 * `north_offset_cd` 输出。
 *
 * 当前关系：
 * - `ship_protocol.c` 负责 D 键双击检测、E 键取消、遥控输入转发和 busy 期控制权隔离；
 * - `MainLoop_GetHeadingDeg100()` 负责在统一航向输出处叠加本模块 offset；
 * - `AutoDrive` 去点/返航状态机不直接感知本模块内部细节。
 */

#ifndef __NORTH_CALIB_H__
#define __NORTH_CALIB_H__

#include "config.h"

typedef enum
{
    NORTH_CALIB_FAIL_NONE = 0,          /**< 无失败。 */
    NORTH_CALIB_FAIL_GPS_NOT_READY,     /**< GPS 未满足定位/卫星条件。 */
    NORTH_CALIB_FAIL_HEADING_NOT_READY, /**< 融合航向尚未可用。 */
    NORTH_CALIB_FAIL_REMOTE_TIMEOUT,    /**< 遥控输入超时。 */
    NORTH_CALIB_FAIL_AUTODRIVE_BUSY,    /**< AutoDrive 正在占用导航链路。 */
    NORTH_CALIB_FAIL_MANUAL_OVERRIDE,   /**< 校准期间检测到人工接管。 */
    NORTH_CALIB_FAIL_DISTANCE_SHORT,    /**< GPS 航迹距离不足。 */
    NORTH_CALIB_FAIL_YAW_UNSTABLE,      /**< 融合航向波动过大。 */
    NORTH_CALIB_FAIL_OFFSET_JUMP,       /**< 新旧北向偏移跳变过大。 */
    NORTH_CALIB_FAIL_EEPROM,            /**< EEPROM 读写或校验失败。 */
    NORTH_CALIB_FAIL_TIMEOUT,           /**< 校准状态机超时。 */
    NORTH_CALIB_FAIL_USER_CANCEL        /**< 用户按 E 主动取消校准。 */
} NorthCalib_FailReason_t;

void NorthCalib_Init(void);
/** @brief 输入最新遥控快照，使超时和人工接管门控保持在本模块内部。 */
void NorthCalib_UpdateRemoteInput(u8 lr, u8 ud, u8 key, u32 now_ms);
/** @brief 尝试进入北向校准状态机，忙或被拒绝时返回 0。 */
u8 NorthCalib_RequestStart(void);
void NorthCalib_Poll(void);
void NorthCalib_Cancel(u8 reason);
u8 NorthCalib_IsBusy(void);
/** @brief 返回当前生效的有符号北向偏移，单位 0.01 deg。 */
int16 NorthCalib_GetHeadingOffsetCd(void);

#endif
