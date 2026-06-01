/**
 * @file    autodrive.c
 * @brief   返航、钓点巡航与目标航向规划。
 *
 * @details
 * AutoDrive 当前只负责：
 * - 解析/缓存 `0x13/0x14/0x15` 旧协议点位与自动返航配置；
 * - 在 GPS 新点到来时计算目标距离与 `target_heading_cd`；
 * - 根据距离切换对准、巡航、减速和到点判定；
 * - 把目标航向和基础速度提交给 `ShipControl_RequestGpsNav()`。
 *
 * AutoDrive 不再自行实现第二套 yaw PID，也不直接决定左右电机极性。
 * 真实控制输出统一由 `ShipControl` 与 `MainLoop_GetHeadingDeg100()` 所代表的
 * 公共航向链路完成。
 */
/**
 * @note 当前职责边界：
 * - AutoDrive 负责目标点、返航条件、距离判断和目标航向规划。
 * - 最终左右电机目标仍由 ShipControl 统一决策。
 * - 统一导航航向入口来自 MainLoop_GetHeadingDeg100()。
 */
#include "autodrive.h"

#include "autodrive_cfg.h"
#include "..\GPS\GPS.h"
#include "..\Control\ShipControl.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\..\User\MainLoop.h"
#include "..\..\..\User\Task.h"

/* ==================== 任务超时与人工接管 ==================== */
/* 自动巡航最长工作时间：10min，AutoDrive_Poll 按 10ms 周期计数。 */
#define AUTODRIVE_WORK_OVERTIME            (10U * 60U * 100U)
/* 遥控器人工接管保持时间：30s，超时后允许自动任务继续判断。 */
#define AUTODRIVE_MANUAL_TIMEOUT_TICKS     (30U * 100U)
/* 人工关闭确认时间：约 3s，用于避免瞬时丢包误关自动模式。 */
#define AUTODRIVE_MANUAL_CLOSE_TICKS       300U

/* ==================== 目标距离判定 ==================== */
/* 小于该距离的目标不启动自动巡航，避免近点反复调头。 */
#define AUTODRIVE_MIN_ACTIVE_DISTANCE_M    10U
/* 超过该距离的目标视为异常点，防止错误坐标导致长时间乱跑。 */
#define AUTODRIVE_MAX_ACTIVE_DISTANCE_M    800U
/* 到达判定距离，进入该范围后停止自动巡航。 */
#define AUTODRIVE_ARRIVE_DISTANCE_M        3U

/* ==================== 前进速度策略 ==================== */
/* 远距离正常巡航基础速度。 */
#define AUTODRIVE_CRUISE_BASE_SPEED        850
/* 进入 20m 内开始降速靠近目标。 */
#define AUTODRIVE_APPROACH_DISTANCE_M      20U
/* 进入 8m 内切换为低速爬行靠近。 */
#define AUTODRIVE_CRAWL_DISTANCE_M         8U
/* 8m~20m 区间的靠近速度上限。 */
#define AUTODRIVE_APPROACH_BASE_SPEED      700
/* 3m~8m 区间的低速爬行速度。 */
#define AUTODRIVE_CRAWL_BASE_SPEED         500

/* ==================== GPS 坐标与角度换算 ==================== */
/* 遥控协议里的经纬度分钟值放大倍数。 */
#define AUTODRIVE_MINUTE_SCALE             10000UL
/* 1 度 = 60 分。 */
#define AUTODRIVE_MINUTES_PER_DEG          60UL
/* 1 经纬度分约等于 1850m。 */
#define AUTODRIVE_METERS_PER_MINUTE        1850UL
/* 1 纬度约等于 111130m。 */
#define AUTODRIVE_METERS_PER_DEG           111130UL
/* atan 近似计算使用的 Q10 缩放。 */
#define AUTODRIVE_ATAN_Q10                 1024UL

/* ==================== 启航前原地对准放行条件 ==================== */
/* 对准放行容差，单位 0.01 度；500 表示目标航向 +/-5.00 度。 */
#define AUTODRIVE_ALIGN_EXIT_ERROR_CD      500

static u8 g_autoDrive_switch = 0U;
static u8 g_autoDrive_state = AUTO_DRIVE_IDLE;
static u8 g_autoDrive_mode = AUTO_DRIVE_CLOSE;
static u16 g_autodrive_work_overtime = 0U;
static u8 g_autoDrive_fail_flag = 0U;

static AutoDrive_PointRaw_t g_idle_position;
static AutoDrive_PointRaw_t g_now_position;
static AutoDrive_PointRaw_t g_last_position;
static AutoDrive_PointRaw_t g_return_position;
static AutoDrive_PointRaw_t g_fish_position;
/* 本次上电会话 RAM 表：保存钓点直到复位或重新上电触发 AutoDrive_Init()。 */
static AutoDrive_FishPointStore_t g_fish_points;
static u8 g_last_fish_cmd_index = 0U;
static u8 g_last_fish_save_result = AUTODRIVE_FISH_SAVE_NONE;
static AutoDrive_ReturnConfig_t g_autodrv_cfg;

static u8 g_destination_direction = POSITION_EAST;
static u8 g_nowrun_direction = POSITION_EAST;
static u16 g_nowrun_angle = 0U;
static u16 g_destination_angle = 0U;
static u16 g_autodrive_target_heading_cd = 0U;
static u8 g_autodrive_target_heading_valid = 0U;
static u16 g_autodrive_base_speed = AUTODRIVE_CRUISE_BASE_SPEED;

static u16 g_link_alive_ticks = 0U;
static u16 g_link_close_ticks = 0U;
static u32 g_last_run_update_seq = 0UL;
static u32 g_last_poll_tick_ms = 0UL;
static u32 g_last_link_tick_ms = 0UL;
static u8 g_last_diag_reason = AUTODRIVE_DIAG_REASON_NONE;

static u16 AutoDrive_GetStartHeadingDeg(void);
static u16 AutoDrive_ReadU16Wire(const u8 *data_m)
{
    /* 遥控器协议中的 0x13/0x14/0x15 点位字段按大端字节序发送。 */
    return (u16)(((u16)data_m[0] << 8) | data_m[1]);
}

static int32 AutoDrive_PointLonMinute1e4(const AutoDrive_PointRaw_t *point)
{
    int32 value;

    if (point == 0) {
        return 0L;
    }
    value = (int32)((u32)(point->lon_whole / 100U) *
                    (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE));
    value += (int32)((u32)(point->lon_whole % 100U) * AUTODRIVE_MINUTE_SCALE);
    value += (int32)point->lon_frac;
    if (point->lon_ew == 'W') {
        value = -value;
    }
    return value;
}

static int32 AutoDrive_PointLatMinute1e4(const AutoDrive_PointRaw_t *point)
{
    int32 value;

    if (point == 0) {
        return 0L;
    }
    value = (int32)((u32)(point->lat_whole / 100U) *
                    (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE));
    value += (int32)((u32)(point->lat_whole % 100U) * AUTODRIVE_MINUTE_SCALE);
    value += (int32)point->lat_frac;
    if (point->lat_ns == 'S') {
        value = -value;
    }
    return value;
}

static u16 AutoDrive_LonScaleQ10FromLatMinute1e4(int32 lat_min1e4)
{
    u32 abs_min;
    u32 deg;

    abs_min = (lat_min1e4 < 0L) ? (u32)(-lat_min1e4) : (u32)lat_min1e4;
    deg = abs_min / (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE);
    if (deg < 10UL) {
        return 1008U;
    }
    if (deg < 20UL) {
        return 962U;
    }
    if (deg < 30UL) {
        return 887U;
    }
    if (deg < 40UL) {
        return 784U;
    }
    if (deg < 50UL) {
        return 658U;
    }
    if (deg < 60UL) {
        return 512U;
    }
    return 350U;
}

static u32 AutoDrive_ScaledLonDiffMinute1e4(const AutoDrive_PointRaw_t *now,
                                            const AutoDrive_PointRaw_t *des)
{
    int32 lon_now;
    int32 lon_des;
    int32 lat_now;
    int32 diff;
    u16 scale_q10;

    lon_now = AutoDrive_PointLonMinute1e4(now);
    lon_des = AutoDrive_PointLonMinute1e4(des);
    lat_now = AutoDrive_PointLatMinute1e4(now);
    diff = lon_des - lon_now;
    if (diff < 0L) {
        diff = -diff;
    }
    scale_q10 = AutoDrive_LonScaleQ10FromLatMinute1e4(lat_now);
    return (((u32)diff * (u32)scale_q10) + (AUTODRIVE_ATAN_Q10 >> 1)) >> 10;
}

static u32 AutoDrive_LatDiffMinute1e4(const AutoDrive_PointRaw_t *now,
                                      const AutoDrive_PointRaw_t *des)
{
    int32 lat_now;
    int32 lat_des;
    int32 diff;

    lat_now = AutoDrive_PointLatMinute1e4(now);
    lat_des = AutoDrive_PointLatMinute1e4(des);
    diff = lat_des - lat_now;
    if (diff < 0L) {
        diff = -diff;
    }
    return (u32)diff;
}

static u32 AutoDrive_Minute1e4ToMeters(u32 minute1e4)
{
    return (minute1e4 * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE;
}

static void AutoDrive_PointFromLegacyWire(AutoDrive_PointRaw_t *point,
                                          const u8 *data_m)
{
    if ((point == 0) || (data_m == 0)) {
        return;
    }

    point->lon_ew = data_m[0];
    point->lon_whole = AutoDrive_ReadU16Wire(&data_m[1]);
    point->lon_frac = AutoDrive_ReadU16Wire(&data_m[3]);
    point->lat_ns = data_m[5];
    point->lat_whole = AutoDrive_ReadU16Wire(&data_m[6]);
    point->lat_frac = AutoDrive_ReadU16Wire(&data_m[8]);
}

static u32 AutoDrive_Abs32Diff(u32 lhs, u32 rhs)
{
    if (lhs >= rhs) {
        return (lhs - rhs);
    }
    return (rhs - lhs);
}

static u16 AutoDrive_Atan01Deg(u16 z_q10)
{
    u32 z;
    u32 curve;
    u32 angle_deg100;

    z = z_q10;
    if (z > AUTODRIVE_ATAN_Q10) {
        z = AUTODRIVE_ATAN_Q10;
    }

    curve = 4500UL + ((1564UL * (AUTODRIVE_ATAN_Q10 - z)) / AUTODRIVE_ATAN_Q10);
    angle_deg100 = (z * curve) / AUTODRIVE_ATAN_Q10;
    return (u16)((angle_deg100 + 50UL) / 100UL);
}

static u8 AutoDrive_PointRawValid(const AutoDrive_PointRaw_t *point)
{
    if (point == 0) {
        return 0U;
    }
    if ((point->lon_ew != 'E') && (point->lon_ew != 'W')) {
        return 0U;
    }
    if (point->lon_whole == 0U) {
        return 0U;
    }
    return 1U;
}

static void AutoDrive_CopyPoint(AutoDrive_PointRaw_t *dst, const AutoDrive_PointRaw_t *src)
{
    if ((dst == 0) || (src == 0)) {
        return;
    }
    *dst = *src;
}

static void AutoDrive_ClearPoint(AutoDrive_PointRaw_t *point)
{
    if (point == 0) {
        return;
    }

    point->lon_ew = 0U;
    point->lon_whole = 0U;
    point->lon_frac = 0U;
    point->lat_ns = 0U;
    point->lat_whole = 0U;
    point->lat_frac = 0U;
}

static void AutoDrive_ClearFishPoints(void)
{
    u8 i;

    /* 到达/停止路径不要调用本函数；已保存钓点在整个上电会话内都应可复用。 */
    for (i = 0U; i < AUTODRIVE_FISH_POINT_COUNT; i++) {
        AutoDrive_ClearPoint(&g_fish_points.point[i]);
    }
    g_fish_points.valid_mask = 0U;
    g_fish_points.next_index = 0U;
    g_fish_points.latest_index = 0xFFU;
}

static u8 AutoDrive_FishPointsReady(void)
{
    return ((g_fish_points.valid_mask & ((1U << AUTODRIVE_FISH_POINT_COUNT) - 1U)) ==
            ((1U << AUTODRIVE_FISH_POINT_COUNT) - 1U)) ? 1U : 0U;
}

static u8 AutoDrive_PointRawEqual(const AutoDrive_PointRaw_t *lhs,
                                  const AutoDrive_PointRaw_t *rhs)
{
    if ((lhs == 0) || (rhs == 0)) {
        return 0U;
    }

    return ((lhs->lon_ew == rhs->lon_ew) &&
            (lhs->lon_whole == rhs->lon_whole) &&
            (lhs->lon_frac == rhs->lon_frac) &&
            (lhs->lat_ns == rhs->lat_ns) &&
            (lhs->lat_whole == rhs->lat_whole) &&
            (lhs->lat_frac == rhs->lat_frac)) ? 1U : 0U;
}

static u8 AutoDrive_FindFishPointIndex(const AutoDrive_PointRaw_t *point)
{
    u8 i;

    if ((point == 0) || (AutoDrive_PointRawValid(point) == 0U)) {
        return 0U;
    }

    for (i = 0U; i < AUTODRIVE_FISH_POINT_COUNT; i++) {
        if ((g_fish_points.valid_mask & (u8)(1U << i)) == 0U) {
            continue;
        }
        if (AutoDrive_PointRawEqual(point, &g_fish_points.point[i]) != 0U) {
            return (u8)(i + 1U);
        }
    }
    return 0U;
}

static u8 AutoDrive_StoreFishPoint(const AutoDrive_PointRaw_t *point)
{
    u8 index;

    if ((point == 0) || (AutoDrive_PointRawValid(point) == 0U)) {
        return 0xFFU;
    }
    if (AutoDrive_FishPointsReady() != 0U) {
        return 0xFFU;
    }

    index = g_fish_points.next_index;
    if (index >= AUTODRIVE_FISH_POINT_COUNT) {
        return 0xFFU;
    }

    AutoDrive_CopyPoint(&g_fish_points.point[index], point);
    g_fish_points.valid_mask |= (u8)(1U << index);
    g_fish_points.latest_index = index;
    index++;
    g_fish_points.next_index = index;
    return g_fish_points.latest_index;
}

static void AutoDrive_PointFromGps(AutoDrive_PointRaw_t *point, const GPS_State_t *gps)
{
    u32 lat_abs;
    u32 lon_abs;
    u32 lat_deg;
    u32 lon_deg;
    u32 lat_min_x1e4;
    u32 lon_min_x1e4;

    if ((point == 0) || (gps == 0)) {
        return;
    }

    lat_abs = (u32)((gps->lat_deg1e7 < 0L) ? -gps->lat_deg1e7 : gps->lat_deg1e7);
    lon_abs = (u32)((gps->lon_deg1e7 < 0L) ? -gps->lon_deg1e7 : gps->lon_deg1e7);

    lat_deg = lat_abs / 10000000UL;
    lon_deg = lon_abs / 10000000UL;

    lat_min_x1e4 = ((lat_abs % 10000000UL) * 6UL + 50UL) / 100UL;
    lon_min_x1e4 = ((lon_abs % 10000000UL) * 6UL + 50UL) / 100UL;

    point->lon_ew = (gps->lon_deg1e7 < 0L) ? 'W' : 'E';
    point->lon_whole = (u16)(lon_deg * 100UL + (lon_min_x1e4 / 10000UL));
    point->lon_frac = (u16)(lon_min_x1e4 % 10000UL);
    point->lat_ns = (gps->lat_deg1e7 < 0L) ? 'S' : 'N';
    point->lat_whole = (u16)(lat_deg * 100UL + (lat_min_x1e4 / 10000UL));
    point->lat_frac = (u16)(lat_min_x1e4 % 10000UL);
}

void AutoDrive_GetCurrentPointRaw(AutoDrive_PointRaw_t *point)
{
    const GPS_State_t *gps;

    gps = GPS_GetState();
    if ((point == 0) || (gps == 0)) {
        return;
    }
    AutoDrive_PointFromGps(point, gps);
}

static u8 AutoDrive_GpsReady(void)
{
    const GPS_State_t *gps;
    u8 sat_count;

    gps = GPS_GetState();
    if (gps == 0) {
        return 0U;
    }
    sat_count = (gps->satellites_used_gsa > 0U) ? gps->satellites_used_gsa : gps->satellites_used;
    if (sat_count < 7U) {
        return 0U;
    }
    if ((gps->lat_deg1e7 == 0L) || (gps->lon_deg1e7 == 0L)) {
        return 0U;
    }
    return 1U;
}

static u8 AutoDrive_GetSatCount(void)
{
    const GPS_State_t *gps;

    gps = GPS_GetState();
    if (gps == 0) {
        return 0U;
    }
    if (gps->satellites_used_gsa > 0U) {
        return gps->satellites_used_gsa;
    }
    return gps->satellites_used;
}

static u8 AutoDrive_GetReadyCurrentPoint(AutoDrive_PointRaw_t *point)
{
    const GPS_State_t *gps;

    if (point == 0) {
        return 0U;
    }
    if (AutoDrive_GpsReady() == 0U) {
        return 0U;
    }

    gps = GPS_GetState();
    if (gps == 0) {
        return 0U;
    }

    AutoDrive_PointFromGps(point, gps);
    return AutoDrive_PointRawValid(point);
}

static void AutoDrive_SetDiagReason(u8 reason)
{
    g_last_diag_reason = reason;
}

static void AutoDrive_ResetApproachTracker(void)
{
    g_autodrive_base_speed = AUTODRIVE_CRUISE_BASE_SPEED;
}

static int16 AutoDrive_Abs16(int16 value)
{
    return (value >= 0) ? value : (int16)(-value);
}

static int16 AutoDrive_WrapSignedCd(int32 angle_cd)
{
    while (angle_cd >= 18000L) {
        angle_cd -= 36000L;
    }
    while (angle_cd < -18000L) {
        angle_cd += 36000L;
    }
    return (int16)angle_cd;
}

static u8 AutoDrive_GetHeadingErrorCd(int16 *error_cd)
{
    if (error_cd == 0) {
        return 0U;
    }
    if ((g_autodrive_target_heading_valid == 0U) ||
        (MainLoop_IsHeadingReady() == 0U)) {
        return 0U;
    }

    *error_cd =
        AutoDrive_WrapSignedCd((int32)g_autodrive_target_heading_cd -
                               (int32)MainLoop_GetHeadingDeg100());
    return 1U;
}

static u8 AutoDrive_AlignTargetReached(void)
{
    int16 heading_error_cd;

    if (AutoDrive_GetHeadingErrorCd(&heading_error_cd) == 0U) {
        return 0U;
    }

    if (AutoDrive_Abs16(heading_error_cd) <= (int16)AUTODRIVE_ALIGN_EXIT_ERROR_CD) {
        return 1U;
    }
    return 0U;
}

static u8 AutoDrive_TickWorkOvertime(void)
{
    if (g_autodrive_work_overtime > 0U) {
        g_autodrive_work_overtime--;
        return 1U;
    }

    AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
    AutoDrive_WorkOvertimeFail();
    return 0U;
}

static u16 AutoDrive_InterpolateSpeed(u16 distance_m,
                                      u16 near_distance_m,
                                      u16 far_distance_m,
                                      u16 near_speed,
                                      u16 far_speed)
{
    u32 distance_span;
    u32 speed_span;
    u32 distance_offset;

    if (distance_m <= near_distance_m) {
        return near_speed;
    }
    if (distance_m >= far_distance_m) {
        return far_speed;
    }
    if (far_distance_m <= near_distance_m) {
        return near_speed;
    }

    distance_span = (u32)far_distance_m - (u32)near_distance_m;
    speed_span = (u32)far_speed - (u32)near_speed;
    distance_offset = (u32)distance_m - (u32)near_distance_m;
    return (u16)((u32)near_speed +
                 ((distance_offset * speed_span) / distance_span));
}

static u16 AutoDrive_CalcBaseSpeed(u16 distance_m)
{
    if (distance_m >= AUTODRIVE_APPROACH_DISTANCE_M) {
        return AUTODRIVE_CRUISE_BASE_SPEED;
    }

    if (distance_m >= AUTODRIVE_CRAWL_DISTANCE_M) {
        return AutoDrive_InterpolateSpeed(distance_m,
                                          AUTODRIVE_CRAWL_DISTANCE_M,
                                          AUTODRIVE_APPROACH_DISTANCE_M,
                                          AUTODRIVE_APPROACH_BASE_SPEED,
                                          AUTODRIVE_CRUISE_BASE_SPEED);
    }

    return AutoDrive_InterpolateSpeed(distance_m,
                                      AUTODRIVE_ARRIVE_DISTANCE_M,
                                      AUTODRIVE_CRAWL_DISTANCE_M,
                                      AUTODRIVE_CRAWL_BASE_SPEED,
                                      AUTODRIVE_APPROACH_BASE_SPEED);
}

static void AutoDrive_UpdateApproachSpeed(u16 distance_m)
{
    g_autodrive_base_speed = AutoDrive_CalcBaseSpeed(distance_m);
}

static u8 AutoDrive_GetTargetPoint(const AutoDrive_PointRaw_t **target)
{
    if (target == 0) {
        return 0U;
    }

    if (g_autoDrive_mode == AUTO_DRIVE_GO_FISISH_POSITION) {
        *target = &g_fish_position;
        return 1U;
    }
    if (g_autoDrive_mode == AUTO_DRIVE_GO_HOME_POSITION) {
        *target = &g_return_position;
        return 1U;
    }

    *target = 0;
    return 0U;
}

static u8 AutoDrive_UpdateTargetHeading(const AutoDrive_PointRaw_t *current_point,
                                        const AutoDrive_PointRaw_t *target_point)
{
    u16 target_angle;
    u8 target_direction;

    if ((current_point == 0) || (target_point == 0)) {
        return 0U;
    }

    target_angle = AutoDrive_GetAngelNowToDestination((const u8 *)current_point,
                                                      (const u8 *)target_point);
    if (target_angle == 65535U) {
        return 0U;
    }

    target_direction =
        AutoDrive_GetDirectionNowToDestination((const u8 *)current_point,
                                               (const u8 *)target_point);
    g_destination_direction = target_direction;
    g_destination_angle = AutoDrive_GetNorthAngel(target_direction, (u8)target_angle);
    if (g_destination_angle >= 360U) {
        g_destination_angle = (u16)(g_destination_angle % 360U);
    }
    g_autodrive_target_heading_cd = (u16)(g_destination_angle * 100U);
    g_autodrive_target_heading_valid = 1U;
    log_info((u8 *)"AD",
             (u8 *)"bearing dir=%u quad=%u tgt=%u cur=%u",
             (u16)target_direction,
             (u16)target_angle,
             (u16)g_destination_angle,
             (u16)AutoDrive_GetStartHeadingDeg());
    return 1U;
}

static u8 AutoDrive_ApplyHeadingHold(u16 base_speed)
{
    if (g_autodrive_target_heading_valid == 0U) {
        ShipControl_StopGpsNav();
        return 0U;
    }
    if (MainLoop_IsHeadingReady() == 0U) {
        ShipControl_StopGpsNav();
        return 0U;
    }

    ShipControl_RequestGpsNav(g_autodrive_target_heading_cd,
                              (int16)base_speed);
    return 1U;
}

static u8 AutoDrive_ApplyAlignHeadingHold(void)
{
    if (g_autodrive_target_heading_valid == 0U) {
        ShipControl_StopGpsNav();
        return 0U;
    }
    if (MainLoop_IsHeadingReady() == 0U) {
        ShipControl_StopGpsNav();
        return 0U;
    }

    ShipControl_RequestGpsAlign(g_autodrive_target_heading_cd);
    return 1U;
}

void AutoDrive_StopMotion(void)
{
    g_autodrive_target_heading_valid = 0U;
    AutoDrive_ResetApproachTracker();
    ShipControl_StopGpsNav();
}

void AutoDrive_Stop(void)
{
    g_autoDrive_state = AUTO_DRIVE_IDLE;
    AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_STOP);
    AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
}

void AutoDrive_SetMode(u8 mode)
{
    if ((g_autoDrive_mode != AUTO_DRIVE_CLOSE) && (mode == AUTO_DRIVE_CLOSE)) {
        AutoDrive_StopMotion();
    }

    g_autoDrive_mode = mode;
    if (g_autoDrive_mode == AUTO_DRIVE_CLOSE) {
        g_autoDrive_state = AUTO_DRIVE_IDLE;
    }
}

u8 AutoDrive_GetMode(void)
{
    return g_autoDrive_mode;
}

u8 AutoDrive_InActive(void)
{
    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        if (g_autoDrive_mode == AUTO_DRIVE_GO_HOME_POSITION) {
            return 1U;
        }
        if (g_autoDrive_mode == AUTO_DRIVE_GO_FISISH_POSITION) {
            return 2U;
        }
    }
    return 0U;
}

u8 AutoDrive_IsBusy(void)
{
    return (g_autoDrive_state != AUTO_DRIVE_IDLE) ? 1U : 0U;
}

u8 AutoDrive_IsCanActive(const AutoDrive_PointRaw_t *point)
{
    AutoDrive_PointRaw_t current_point;
    u16 distance;

    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        return 0U;
    }
    if (AutoDrive_PointRawValid(point) == 0U) {
        return 0U;
    }
    if (MainLoop_IsHeadingReady() == 0U) {
        return 0U;
    }
    if (AutoDrive_GetReadyCurrentPoint(&current_point) == 0U) {
        return 0U;
    }

    distance = AutoDrive_GetDistanceNowToDestination((const u8 *)point,
                                                     (const u8 *)&current_point);
    if ((distance > AUTODRIVE_MIN_ACTIVE_DISTANCE_M) &&
        (distance < AUTODRIVE_MAX_ACTIVE_DISTANCE_M)) {
        AutoDrive_CopyPoint(&g_idle_position, &current_point);
        return 1U;
    }
    return 0U;
}

void AutoDrive_SetReturnPositionRaw(const u8 *data_m)
{
    AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_CMD_RETURN_HOME);
    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        return;
    }

    AutoDrive_PointFromLegacyWire(&g_return_position, data_m);
    AutoDrive_CopyPoint(&g_autodrv_cfg.ret_point, &g_return_position);
    (void)AutoDriveCfg_Save(&g_autodrv_cfg);
    if (AutoDrive_IsCanActive(&g_return_position) == 0U) {
        return;
    }

    AutoDrive_SetMode(AUTO_DRIVE_GO_HOME_POSITION);
    g_autoDrive_state = AUTO_DRIVE_START;
    g_autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;
    g_autoDrive_fail_flag = 0U;
}

static u8 AutoDrive_StartFishPoint(const AutoDrive_PointRaw_t *point)
{
    AutoDrive_CopyPoint(&g_fish_position, point);
    if (AutoDrive_IsCanActive(&g_fish_position) == 0U) {
        return AUTODRIVE_FISH_CMD_REJECT_DISTANCE;
    }

    AutoDrive_SetMode(AUTO_DRIVE_GO_FISISH_POSITION);
    g_autoDrive_state = AUTO_DRIVE_START;
    g_autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;
    g_autoDrive_fail_flag = 0U;
    return AUTODRIVE_FISH_CMD_STARTED;
}

u8 AutoDrive_SetFishPositionRaw(const u8 *data_m)
{
    AutoDrive_PointRaw_t rx_point;
    u8 matched_index;
    u8 stored_index;

    AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_CMD_GOTO_POINT);
    g_last_fish_cmd_index = 0U;
    g_last_fish_save_result = AUTODRIVE_FISH_SAVE_NONE;
    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        g_last_fish_save_result = AUTODRIVE_FISH_SAVE_BUSY;
        return AUTODRIVE_FISH_CMD_BUSY;
    }

    AutoDrive_PointFromLegacyWire(&rx_point, data_m);
    if (AutoDrive_PointRawValid(&rx_point) == 0U) {
        g_last_fish_save_result = AUTODRIVE_FISH_SAVE_INVALID;
        return AUTODRIVE_FISH_CMD_INVALID;
    }

    matched_index = AutoDrive_FindFishPointIndex(&rx_point);
    if (matched_index != 0U) {
        g_last_fish_save_result = AUTODRIVE_FISH_SAVE_EXISTS;
    } else {
        if (AutoDrive_FishPointsReady() == 0U) {
            stored_index = AutoDrive_StoreFishPoint(&rx_point);
            if (stored_index != 0xFFU) {
                matched_index = (u8)(stored_index + 1U);
                g_last_fish_save_result = AUTODRIVE_FISH_SAVE_STORED;
            } else {
                g_last_fish_save_result = AUTODRIVE_FISH_SAVE_FULL_TEMP;
            }
        } else {
            g_last_fish_save_result = AUTODRIVE_FISH_SAVE_FULL_TEMP;
        }
    }

    g_last_fish_cmd_index = matched_index;
    return AutoDrive_StartFishPoint(&rx_point);
}

void AutoDrive_TriggerReturnWithReason(u8 reason)
{
    AutoDrive_SetDiagReason(reason);
    if (g_autodrv_cfg.auto_ret_onoff != 0x30U) {
        if (g_autoDrive_fail_flag != 0U) {
            return;
        }
        if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
            return;
        }
        if (AutoDrive_IsCanActive(&g_autodrv_cfg.ret_point) != 0U) {
            AutoDrive_CopyPoint(&g_return_position, &g_autodrv_cfg.ret_point);
            AutoDrive_SetMode(AUTO_DRIVE_GO_HOME_POSITION);
            g_autoDrive_state = AUTO_DRIVE_START;
            g_autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;
        }
    }
}

void AutoDrive_TriggerReturn(void)
{
    AutoDrive_TriggerReturnWithReason(AUTODRIVE_DIAG_REASON_GENERIC_TRIGGER);
}

void AutoDrive_WorkOvertimeFail(void)
{
    g_autoDrive_fail_flag = 1U;
    AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_OVERTIME);
}

void AutoDrive_SetSwitchRaw(const u8 *data_m, u8 len)
{
    if ((data_m == 0) || (len == 0U)) {
        return;
    }

    g_autoDrive_switch = data_m[0];
    g_autodrv_cfg.auto_ret_onoff = data_m[0];
    if (len >= (u8)(1U + AUTODRIVE_LEGACY_POINT_WIRE_LEN)) {
        AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_RETURN_SWITCH_SAVE);
        AutoDrive_PointFromLegacyWire(&g_autodrv_cfg.ret_point, &data_m[1]);
        if (g_autoDrive_state == AUTO_DRIVE_IDLE) {
            AutoDrive_CopyPoint(&g_return_position, &g_autodrv_cfg.ret_point);
        }
    }
    (void)AutoDriveCfg_Save(&g_autodrv_cfg);
    if (g_autodrv_cfg.auto_ret_onoff != 0x30U) {
        AutoDrive_TriggerReturnWithReason(AUTODRIVE_DIAG_REASON_RETURN_SWITCH_SAVE);
    }
}

void AutoDrive_GetStoredConfig(AutoDrive_ReturnConfig_t *cfg)
{
    if (cfg != 0) {
        *cfg = g_autodrv_cfg;
    }
}

u8 AutoDrive_GetReturnPositionRaw(AutoDrive_PointRaw_t *point)
{
    if (point != 0) {
        AutoDrive_CopyPoint(point, &g_return_position);
    }
    return AutoDrive_PointRawValid(&g_return_position);
}

u8 AutoDrive_GetFishPositionRaw(AutoDrive_PointRaw_t *point)
{
    if (point != 0) {
        AutoDrive_CopyPoint(point, &g_fish_position);
    }
    return AutoDrive_PointRawValid(&g_fish_position);
}

u8 AutoDrive_GetFishPositionByIndexRaw(u8 index, AutoDrive_PointRaw_t *point)
{
    u8 slot;

    if ((index == 0U) || (index > AUTODRIVE_FISH_POINT_COUNT)) {
        return 0U;
    }

    slot = (u8)(index - 1U);
    if ((g_fish_points.valid_mask & (u8)(1U << slot)) == 0U) {
        return 0U;
    }

    if (point != 0) {
        AutoDrive_CopyPoint(point, &g_fish_points.point[slot]);
    }
    return AutoDrive_PointRawValid(&g_fish_points.point[slot]);
}

u8 AutoDrive_GetLastFishCommandIndex(void)
{
    return g_last_fish_cmd_index;
}

u8 AutoDrive_GetLastFishSaveResult(void)
{
    return g_last_fish_save_result;
}

u8 AutoDrive_GetDirectionNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData)
{
    const AutoDrive_PointRaw_t *nowposition;
    const AutoDrive_PointRaw_t *desposition;
    int32 lon_diff;
    int32 lat_diff;

    nowposition = (const AutoDrive_PointRaw_t *)nowpositionData;
    desposition = (const AutoDrive_PointRaw_t *)despositionData;

    lon_diff = AutoDrive_PointLonMinute1e4(desposition) -
               AutoDrive_PointLonMinute1e4(nowposition);
    lat_diff = AutoDrive_PointLatMinute1e4(desposition) -
               AutoDrive_PointLatMinute1e4(nowposition);

    if (lon_diff > 0L) {
        if (lat_diff > 0L) {
            return POSITION_EAST_NORTH;
        }
        if (lat_diff < 0L) {
            return POSITION_EAST_SOUTH;
        }
        return POSITION_EAST;
    }
    if (lon_diff < 0L) {
        if (lat_diff > 0L) {
            return POSITION_WEST_NORTH;
        }
        if (lat_diff < 0L) {
            return POSITION_WEST_SOUTH;
        }
        return POSITION_WEST;
    }

    if (lat_diff < 0L) {
        return POSITION_SOUTH;
    }
    return POSITION_NORTH;
}

static u32 AutoDrive_CalDistanceLon(const AutoDrive_PointRaw_t *now,
                                    const AutoDrive_PointRaw_t *des)
{
    int32 lon_now;
    int32 lon_des;
    int32 diff;

    lon_now = AutoDrive_PointLonMinute1e4(now);
    lon_des = AutoDrive_PointLonMinute1e4(des);
    diff = lon_des - lon_now;
    if (diff < 0L) {
        diff = -diff;
    }
    return AutoDrive_Minute1e4ToMeters((u32)diff);
}

static u32 AutoDrive_CalDistanceLonScaled(const AutoDrive_PointRaw_t *now,
                                          const AutoDrive_PointRaw_t *des)
{
    u32 lon_m;
    u16 scale_q10;

    lon_m = AutoDrive_CalDistanceLon(now, des);
    scale_q10 = AutoDrive_LonScaleQ10FromLatMinute1e4(AutoDrive_PointLatMinute1e4(now));
    return ((lon_m * (u32)scale_q10) + (AUTODRIVE_ATAN_Q10 >> 1)) >> 10;
}

static u32 AutoDrive_CalDistanceLat(const AutoDrive_PointRaw_t *now,
                                    const AutoDrive_PointRaw_t *des)
{
    return AutoDrive_Minute1e4ToMeters(AutoDrive_LatDiffMinute1e4(now, des));
}

u16 AutoDrive_GetAngelNowToDestination(const u8 *nowpositionData,
                                       const u8 *despositionData)
{
    const AutoDrive_PointRaw_t *nowposition;
    const AutoDrive_PointRaw_t *desposition;
    u32 distance_width;
    u32 distance_height;
    u32 z_q10;
    u16 base;

    nowposition = (const AutoDrive_PointRaw_t *)nowpositionData;
    desposition = (const AutoDrive_PointRaw_t *)despositionData;

    distance_width = AutoDrive_ScaledLonDiffMinute1e4(nowposition, desposition);
    distance_height = AutoDrive_LatDiffMinute1e4(nowposition, desposition);

    if (distance_width == 0UL) {
        if (distance_height == 0UL) {
            return 65535U;
        }
        return 90U;
    }

    if (distance_height <= distance_width) {
        z_q10 = ((distance_height << 10) + (distance_width >> 1)) / distance_width;
        return AutoDrive_Atan01Deg((u16)z_q10);
    }

    z_q10 = ((distance_width << 10) + (distance_height >> 1)) / distance_height;
    base = AutoDrive_Atan01Deg((u16)z_q10);
    return (u16)(90U - base);
}

u16 AutoDrive_GetDistanceNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData)
{
    const AutoDrive_PointRaw_t *nowposition;
    const AutoDrive_PointRaw_t *desposition;
    u32 distance_width;
    u32 distance_height;
    u32 max_distance;
    u32 min_distance;
    u32 distance;

    nowposition = (const AutoDrive_PointRaw_t *)nowpositionData;
    desposition = (const AutoDrive_PointRaw_t *)despositionData;

    distance_width = AutoDrive_CalDistanceLonScaled(nowposition, desposition);
    distance_height = AutoDrive_CalDistanceLat(nowposition, desposition);

    if (distance_width >= distance_height) {
        max_distance = distance_width;
        min_distance = distance_height;
    } else {
        max_distance = distance_height;
        min_distance = distance_width;
    }
    distance = max_distance + ((min_distance * 3UL) >> 3);
    if (distance > 65535UL) {
        return 65535U;
    }

    return (u16)distance;
}

u16 AutoDrive_GetNorthAngel(u8 direction, u8 angel)
{
    switch (direction) {
    case POSITION_NORTH:
        return 0U;
    case POSITION_EAST_NORTH:
        return (u16)(90U - angel);
    case POSITION_EAST:
        return 90U;
    case POSITION_EAST_SOUTH:
        return (u16)(90U + angel);
    case POSITION_SOUTH:
        return 180U;
    case POSITION_WEST_SOUTH:
        return (u16)(270U - angel);
    case POSITION_WEST:
        return 270U;
    case POSITION_WEST_NORTH:
        return (u16)(270U + angel);
    default:
        return 0U;
    }
}

static u16 AutoDrive_GetStartHeadingDeg(void)
{
    u16 heading_cd;
    u16 heading;

    if (MainLoop_IsHeadingReady() != 0U) {
        heading_cd = (u16)(MainLoop_GetHeadingDeg100() % 36000U);
        heading = (u16)((heading_cd + 50U) / 100U);
        if (heading >= 360U) {
            heading = 0U;
        }
        return heading;
    }

    return 0U;
}

static u16 AutoDrive_GetRunHeadingDeg(void)
{
    u16 heading_cd;
    u16 heading;

    if (MainLoop_IsHeadingReady() != 0U) {
        heading_cd = (u16)(MainLoop_GetHeadingDeg100() % 36000U);
        heading = (u16)((heading_cd + 50U) / 100U);
        if (heading >= 360U) {
            heading = 0U;
        }
        return heading;
    }

    return 65535U;
}

static void AutoDrive_UpdateIdlePosition(void)
{
    if (AutoDrive_GpsReady() == 0U) {
        return;
    }

    AutoDrive_PointFromGps(&g_idle_position, GPS_GetState());
}

static void AutoDrive_UpdateGpsStepPoints(void)
{
    const GPS_State_t *gps;

    gps = GPS_GetState();
    if (gps == 0) {
        return;
    }

    AutoDrive_PointFromGps(&g_now_position, gps);
}

static void AutoDrive_GetSnapshotTargetPoint(AutoDrive_PointRaw_t *point)
{
    if (point == 0) {
        return;
    }

    point->lon_ew = 0U;
    point->lon_whole = 0U;
    point->lon_frac = 0U;
    point->lat_ns = 0U;
    point->lat_whole = 0U;
    point->lat_frac = 0U;

    if ((g_autoDrive_mode == AUTO_DRIVE_GO_HOME_POSITION) ||
        (g_last_diag_reason == AUTODRIVE_DIAG_REASON_CMD_RETURN_HOME)) {
        AutoDrive_CopyPoint(point, &g_return_position);
    } else if ((g_autoDrive_mode == AUTO_DRIVE_GO_FISISH_POSITION) ||
               (g_last_diag_reason == AUTODRIVE_DIAG_REASON_CMD_GOTO_POINT)) {
        AutoDrive_CopyPoint(point, &g_fish_position);
    } else {
        AutoDrive_CopyPoint(point, &g_autodrv_cfg.ret_point);
    }
}

static u8 AutoDrive_CanActivateTargetPoint(const AutoDrive_PointRaw_t *point,
                                           const AutoDrive_PointRaw_t *current_point)
{
    u16 distance;

    if (AutoDrive_PointRawValid(point) == 0U) {
        return 0U;
    }
    if (AutoDrive_PointRawValid(current_point) == 0U) {
        return 0U;
    }
    if (AutoDrive_GpsReady() == 0U) {
        return 0U;
    }

    distance = AutoDrive_GetDistanceNowToDestination((const u8 *)point,
                                                     (const u8 *)current_point);
    if ((distance > AUTODRIVE_MIN_ACTIVE_DISTANCE_M) &&
        (distance < AUTODRIVE_MAX_ACTIVE_DISTANCE_M)) {
        return 1U;
    }
    return 0U;
}

void AutoDrive_Init(void)
{
    AutoDriveCfg_Init();
    AutoDriveCfg_Load(&g_autodrv_cfg);
    if (g_autodrv_cfg.auto_ret_onoff == 0xFFU) {
        g_autodrv_cfg.auto_ret_onoff = 0x30U;
    }
    AutoDrive_CopyPoint(&g_return_position, &g_autodrv_cfg.ret_point);
    AutoDrive_ClearPoint(&g_fish_position);
    AutoDrive_ClearFishPoints();
    g_last_fish_cmd_index = 0U;
    g_last_fish_save_result = AUTODRIVE_FISH_SAVE_NONE;

    g_autoDrive_switch = g_autodrv_cfg.auto_ret_onoff;
    g_autoDrive_state = AUTO_DRIVE_IDLE;
    g_autoDrive_mode = AUTO_DRIVE_CLOSE;
    g_autodrive_work_overtime = 0U;
    g_autoDrive_fail_flag = 0U;
    g_link_alive_ticks = 0U;
    g_link_close_ticks = 0U;
    g_last_run_update_seq = 0UL;
    g_last_poll_tick_ms = Task_GetTickMs();
    g_last_link_tick_ms = g_last_poll_tick_ms;
    g_last_diag_reason = AUTODRIVE_DIAG_REASON_NONE;
    g_autodrive_target_heading_cd = 0U;
    g_autodrive_target_heading_valid = 0U;
    AutoDrive_ResetApproachTracker();

    AutoDrive_StopMotion();
}

void AutoDrive_LinkAliveKick(void)
{
    g_link_alive_ticks = 0U;
    g_link_close_ticks = AUTODRIVE_MANUAL_CLOSE_TICKS;
}

void AutoDrive_LinkAliveTick(void)
{
    u32 now_ms;

    now_ms = Task_GetTickMs();
    if ((now_ms - g_last_link_tick_ms) < 10U) {
        return;
    }
    g_last_link_tick_ms = now_ms;

    if (g_link_alive_ticks < AUTODRIVE_MANUAL_TIMEOUT_TICKS) {
        g_link_alive_ticks++;
    } else {
        AutoDrive_TriggerReturnWithReason(AUTODRIVE_DIAG_REASON_LINK_TIMEOUT);
        g_link_alive_ticks = 0U;
    }

    if (g_link_close_ticks > 0U) {
        g_link_close_ticks--;
        if (g_link_close_ticks == 0U) {
            if (AutoDrive_GetMode() == AUTO_DRIVE_CLOSE) {
                AutoDrive_StopMotion();
            }
        }
    }
}

void AutoDrive_Poll(void)
{
    const GPS_State_t *gps;
    const AutoDrive_PointRaw_t *target_point;
    u16 destination_distance;
    u32 now_ms;

    now_ms = Task_GetTickMs();
    if ((now_ms - g_last_poll_tick_ms) < 10U) {
        return;
    }
    g_last_poll_tick_ms = now_ms;

    gps = GPS_GetState();
    if ((gps != 0) && (g_autoDrive_state == AUTO_DRIVE_IDLE)) {
        AutoDrive_UpdateIdlePosition();
    }

    switch (g_autoDrive_state) {
    case AUTO_DRIVE_IDLE:
        break;

    case AUTO_DRIVE_START:
        if (AutoDrive_TickWorkOvertime() == 0U) {
            break;
        }
        if (AutoDrive_GetTargetPoint(&target_point) == 0U) {
            AutoDrive_Stop();
            break;
        }
        if ((gps == 0) || (AutoDrive_GpsReady() == 0U) ||
            (MainLoop_IsHeadingReady() == 0U)) {
            ShipControl_StopGpsNav();
            break;
        }

        AutoDrive_PointFromGps(&g_last_position, gps);
        AutoDrive_CopyPoint(&g_idle_position, &g_last_position);
        if (AutoDrive_UpdateTargetHeading(&g_last_position, target_point) == 0U) {
            AutoDrive_Stop();
            break;
        }

        AutoDrive_ResetApproachTracker();
        destination_distance =
            AutoDrive_GetDistanceNowToDestination((const u8 *)&g_last_position,
                                                  (const u8 *)target_point);
        AutoDrive_UpdateApproachSpeed(destination_distance);
        if (destination_distance <= AUTODRIVE_ARRIVE_DISTANCE_M) {
            AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_ARRIVE);
            g_autoDrive_state = AUTO_DRIVE_IDLE;
            AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
            AutoDrive_StopMotion();
            break;
        }

        ShipControl_ResetYawHoldController();
        g_last_run_update_seq = gps->update_sequence;
        g_autoDrive_state = AUTO_DRIVE_GET_DIRECTION;
        (void)AutoDrive_ApplyAlignHeadingHold();
        break;

    case AUTO_DRIVE_GET_DIRECTION:
        if (AutoDrive_TickWorkOvertime() == 0U) {
            break;
        }
        if (AutoDrive_GetTargetPoint(&target_point) == 0U) {
            AutoDrive_Stop();
            break;
        }

        if ((gps != 0) && (AutoDrive_GpsReady() != 0U)) {
            AutoDrive_PointFromGps(&g_now_position, gps);
            destination_distance =
                AutoDrive_GetDistanceNowToDestination((const u8 *)&g_now_position,
                                                      (const u8 *)target_point);
            AutoDrive_UpdateApproachSpeed(destination_distance);
            if (destination_distance <= AUTODRIVE_ARRIVE_DISTANCE_M) {
                AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_ARRIVE);
                g_autoDrive_state = AUTO_DRIVE_IDLE;
                AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
                AutoDrive_StopMotion();
                break;
            }
            if (AutoDrive_UpdateTargetHeading(&g_now_position, target_point) != 0U) {
                AutoDrive_CopyPoint(&g_last_position, &g_now_position);
                g_last_run_update_seq = gps->update_sequence;
            }
        }

        if (AutoDrive_ApplyAlignHeadingHold() == 0U) {
            break;
        }
        if (AutoDrive_AlignTargetReached() != 0U) {
            ShipControl_ResetYawHoldController();
            g_autoDrive_state = AUTO_DRIVE_RUNING;
            (void)AutoDrive_ApplyHeadingHold(g_autodrive_base_speed);
        }
        break;

    case AUTO_DRIVE_RUNING:
        if (AutoDrive_TickWorkOvertime() == 0U) {
            break;
        }

        if ((gps != 0) && (AutoDrive_GpsReady() != 0U) &&
            (gps->update_sequence != g_last_run_update_seq)) {
            if (AutoDrive_GetTargetPoint(&target_point) == 0U) {
                AutoDrive_Stop();
                break;
            }

            AutoDrive_UpdateGpsStepPoints();
            destination_distance =
                AutoDrive_GetDistanceNowToDestination((const u8 *)&g_now_position,
                                                      (const u8 *)target_point);
            AutoDrive_UpdateApproachSpeed(destination_distance);
            if (destination_distance <= AUTODRIVE_ARRIVE_DISTANCE_M) {
                AutoDrive_SetDiagReason(AUTODRIVE_DIAG_REASON_ARRIVE);
                g_autoDrive_state = AUTO_DRIVE_IDLE;
                AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
                AutoDrive_StopMotion();
                break;
            }

            g_nowrun_direction =
                AutoDrive_GetDirectionNowToDestination((const u8 *)&g_last_position,
                                                       (const u8 *)&g_now_position);
            if (AutoDrive_UpdateTargetHeading(&g_now_position, target_point) == 0U) {
                break;
            }

            AutoDrive_CopyPoint(&g_last_position, &g_now_position);
            g_last_run_update_seq = gps->update_sequence;
        }

        (void)AutoDrive_ApplyHeadingHold(g_autodrive_base_speed);
        break;

    default:
        g_autodrive_work_overtime = 0U;
        AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
        break;
    }
}

void AutoDrive_GetDebugSnapshot(AutoDrive_DebugSnapshot_t *snapshot)
{
    u16 heading;

    if (snapshot == 0) {
        return;
    }

    snapshot->state = g_autoDrive_state;
    snapshot->mode = g_autoDrive_mode;
    snapshot->auto_ret_onoff = g_autodrv_cfg.auto_ret_onoff;
    snapshot->fail_flag = g_autoDrive_fail_flag;
    snapshot->last_reason = g_last_diag_reason;
    snapshot->gps_ready = AutoDrive_GpsReady();
    snapshot->sat_count = AutoDrive_GetSatCount();
    snapshot->current_point.lon_ew = 0U;
    snapshot->current_point.lon_whole = 0U;
    snapshot->current_point.lon_frac = 0U;
    snapshot->current_point.lat_ns = 0U;
    snapshot->current_point.lat_whole = 0U;
    snapshot->current_point.lat_frac = 0U;

    AutoDrive_GetCurrentPointRaw(&snapshot->current_point);
    AutoDrive_GetSnapshotTargetPoint(&snapshot->target_point);

    snapshot->can_activate_target =
        AutoDrive_CanActivateTargetPoint(&snapshot->target_point,
                                         &snapshot->current_point);

    if ((AutoDrive_PointRawValid(&snapshot->current_point) != 0U) &&
        (AutoDrive_PointRawValid(&snapshot->target_point) != 0U)) {
        snapshot->distance_to_target_m =
            AutoDrive_GetDistanceNowToDestination((const u8 *)&snapshot->current_point,
                                                  (const u8 *)&snapshot->target_point);
    } else {
        snapshot->distance_to_target_m = 0U;
    }

    heading = AutoDrive_GetRunHeadingDeg();
    if ((heading == 65535U) || (heading > 360U)) {
        heading = AutoDrive_GetStartHeadingDeg();
    }
    snapshot->current_heading_deg = heading;
    snapshot->target_heading_deg = g_destination_angle;
}
