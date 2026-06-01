/**
 * @file    autodrive.h
 * @brief   自动返航与定点导航模块公共接口。
 *
 * @details
 * AutoDrive 负责保存返航/钓点目标、维护自动导航状态机，并向上层暴露
 * 自动返航、去钓点、停止和诊断快照等接口。
 *
 * @note 当前职责边界：
 * - AutoDrive 负责目标点、状态机和目标航向规划。
 * - 最终航向控制和电机输出仍由 ShipControl 执行。
 * - 北向校准由 NorthCalib 独立维护，不在本头文件内实现。
 */
#ifndef __AUTODRIVE_H__
#define __AUTODRIVE_H__

#include "config.h"

typedef enum
{
    AUTO_DRIVE_IDLE = 0,
    AUTO_DRIVE_START,
    AUTO_DRIVE_GET_DIRECTION,
    AUTO_DRIVE_RUNING
} AutoDrive_State_t;

typedef enum
{
    AUTO_DRIVE_CLOSE = 0,
    AUTO_DRIVE_GO_FISISH_POSITION,
    AUTO_DRIVE_GO_HOME_POSITION
} AutoDrive_Mode_t;

typedef enum
{
    POSITION_NORTH = 1,
    POSITION_EAST_NORTH = 2,
    POSITION_EAST = 3,
    POSITION_EAST_SOUTH = 4,
    POSITION_SOUTH = 5,
    POSITION_WEST_SOUTH = 6,
    POSITION_WEST = 7,
    POSITION_WEST_NORTH = 8
} AutoDrive_Direction_t;

typedef struct
{
    u8 lon_ew;        /**< 经度半球字符，通常为 E/W。 */
    u16 lon_whole;    /**< 旧协议经度整数段，格式 dddmm。 */
    u16 lon_frac;     /**< 旧协议经度小数段，小数点后 4 位。 */
    u8 lat_ns;        /**< 纬度半球字符，通常为 N/S。 */
    u16 lat_whole;    /**< 旧协议纬度整数段，格式 ddmm。 */
    u16 lat_frac;     /**< 旧协议纬度小数段，小数点后 4 位。 */
} AutoDrive_PointRaw_t;

#define AUTODRIVE_LEGACY_POINT_WIRE_LEN 10U
#define AUTODRIVE_FISH_POINT_COUNT      5U

#define AUTODRIVE_FISH_CMD_BUSY            0U
#define AUTODRIVE_FISH_CMD_REJECT_DISTANCE 4U
#define AUTODRIVE_FISH_CMD_STARTED         5U
#define AUTODRIVE_FISH_CMD_INVALID         6U

#define AUTODRIVE_FISH_SAVE_NONE           0U
#define AUTODRIVE_FISH_SAVE_STORED         1U
#define AUTODRIVE_FISH_SAVE_EXISTS         2U
#define AUTODRIVE_FISH_SAVE_FULL_TEMP      3U
#define AUTODRIVE_FISH_SAVE_BUSY           4U
#define AUTODRIVE_FISH_SAVE_INVALID        5U

typedef struct
{
    u8 auto_ret_onoff;              /**< 自动返航开关状态，1=允许自动返航。 */
    AutoDrive_PointRaw_t ret_point; /**< 保存的返航目标点。 */
} AutoDrive_ReturnConfig_t;

typedef struct
{
    AutoDrive_PointRaw_t point[AUTODRIVE_FISH_POINT_COUNT]; /**< 钓点环形存储槽。 */
    u8 valid_mask;                                          /**< 钓点有效位图，bitN 对应 point[N]。 */
    u8 next_index;                                          /**< 下一次写入的钓点槽位。 */
    u8 latest_index;                                        /**< 最近一次成功写入的钓点槽位。 */
} AutoDrive_FishPointStore_t;

typedef enum
{
    AUTODRIVE_DIAG_REASON_NONE = 0,
    AUTODRIVE_DIAG_REASON_CMD_RETURN_HOME = 1,
    AUTODRIVE_DIAG_REASON_CMD_GOTO_POINT = 2,
    AUTODRIVE_DIAG_REASON_RETURN_SWITCH_SAVE = 3,
    AUTODRIVE_DIAG_REASON_LINK_TIMEOUT = 4,
    AUTODRIVE_DIAG_REASON_LOW_POWER = 5,
    AUTODRIVE_DIAG_REASON_GENERIC_TRIGGER = 6,
    AUTODRIVE_DIAG_REASON_ARRIVE = 7,
    AUTODRIVE_DIAG_REASON_OVERTIME = 8,
    AUTODRIVE_DIAG_REASON_STOP = 9
} AutoDrive_DiagReason_t;

typedef struct
{
    u8 state;                         /**< 当前自动导航状态，见 AutoDrive_State_t。 */
    u8 mode;                          /**< 当前自动导航模式，见 AutoDrive_Mode_t。 */
    u8 auto_ret_onoff;                /**< 自动返航开关快照。 */
    u8 fail_flag;                     /**< 最近一次导航失败标志。 */
    u8 last_reason;                   /**< 最近一次状态切换/失败原因，见 AutoDrive_DiagReason_t。 */
    u8 gps_ready;                     /**< GPS 是否满足导航输入条件。 */
    u8 sat_count;                     /**< 当前参与定位的卫星数量。 */
    u8 can_activate_target;           /**< 当前目标点是否允许启动导航。 */
    u16 distance_to_target_m;         /**< 到目标点的估算距离，单位 m。 */
    u16 current_heading_deg;          /**< 当前导航航向，单位 deg。 */
    u16 target_heading_deg;           /**< 指向目标点的目标航向，单位 deg。 */
    AutoDrive_PointRaw_t current_point; /**< 当前 GPS 点位快照。 */
    AutoDrive_PointRaw_t target_point;  /**< 当前导航目标点快照。 */
} AutoDrive_DebugSnapshot_t;

void AutoDrive_Init(void);
void AutoDrive_Poll(void);
void AutoDrive_Stop(void);
void AutoDrive_StopMotion(void);
void AutoDrive_TriggerReturn(void);
void AutoDrive_TriggerReturnWithReason(u8 reason);
void AutoDrive_WorkOvertimeFail(void);

void AutoDrive_SetMode(u8 mode);
u8 AutoDrive_GetMode(void);
u8 AutoDrive_InActive(void);
u8 AutoDrive_IsBusy(void);
u8 AutoDrive_IsCanActive(const AutoDrive_PointRaw_t *point);

void AutoDrive_SetReturnPositionRaw(const u8 *data_m);
u8 AutoDrive_SetFishPositionRaw(const u8 *data_m);
void AutoDrive_SetSwitchRaw(const u8 *data_m, u8 len);
void AutoDrive_GetStoredConfig(AutoDrive_ReturnConfig_t *cfg);
void AutoDrive_GetCurrentPointRaw(AutoDrive_PointRaw_t *point);
u8 AutoDrive_GetReturnPositionRaw(AutoDrive_PointRaw_t *point);
u8 AutoDrive_GetFishPositionRaw(AutoDrive_PointRaw_t *point);
u8 AutoDrive_GetFishPositionByIndexRaw(u8 index, AutoDrive_PointRaw_t *point);
u8 AutoDrive_GetLastFishCommandIndex(void);
u8 AutoDrive_GetLastFishSaveResult(void);
void AutoDrive_GetDebugSnapshot(AutoDrive_DebugSnapshot_t *snapshot);

u16 AutoDrive_GetDistanceNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData);
u16 AutoDrive_GetAngelNowToDestination(const u8 *nowpositionData,
                                       const u8 *despositionData);
u16 AutoDrive_GetNorthAngel(u8 direction, u8 angel);
u8 AutoDrive_GetDirectionNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData);

void AutoDrive_LinkAliveTick(void);
void AutoDrive_LinkAliveKick(void);

#endif
