#include "autodrive.h"

#include "autodrive_cfg.h"
#include "..\GPS\GPS.h"
#include "..\Motor\Motor.h"
#include "..\..\Function\AHRS\AHRS.h"
#include "..\..\..\User\Task.h"

#define AUTODRIVE_WORK_OVERTIME            (10U * 60U * 100U)
#define AUTODRIVE_MIN_ACTIVE_DISTANCE_M    10U
#define AUTODRIVE_MAX_ACTIVE_DISTANCE_M    800U
#define AUTODRIVE_ARRIVE_DISTANCE_M        3U
#define AUTODRIVE_STRAIGHT_DISTANCE_X10_M  15U
#define AUTODRIVE_MANUAL_TIMEOUT_TICKS     (30U * 100U)
#define AUTODRIVE_MANUAL_CLOSE_TICKS       300U
#define AUTODRIVE_DRIVE_PWM                900
#define AUTODRIVE_DRIVE_FAST_PWM           1000
#define AUTODRIVE_TURN_PWM                 850
#define AUTODRIVE_MINUTE_SCALE             10000UL
#define AUTODRIVE_MINUTES_PER_DEG          60UL
#define AUTODRIVE_METERS_PER_MINUTE        1850UL
#define AUTODRIVE_METERS_PER_DEG           111130UL
#define AUTODRIVE_ATAN_Q10                 1024UL

static u8 g_autoDrive_switch = 0U;
static u8 g_autoDrive_state = AUTO_DRIVE_IDLE;
static u8 g_autoDrive_mode = AUTO_DRIVE_CLOSE;
static u16 g_autoDrive_turn_times = 0U;
static u16 g_autodrive_work_overtime = 0U;
static u8 g_autoDrive_fail_flag = 0U;
static u8 g_motor_ready = 0U;

static AutoDrive_PointRaw_t g_idle_position;
static AutoDrive_PointRaw_t g_now_position;
static AutoDrive_PointRaw_t g_last_position;
static AutoDrive_PointRaw_t g_return_position;
static AutoDrive_PointRaw_t g_fish_position;
static AutoDrive_ReturnConfig_t g_autodrv_cfg;

static u8 g_destination_direction = POSITION_EAST;
static u8 g_nowrun_direction = POSITION_EAST;
static u16 g_nowrun_angle = 0U;
static u16 g_destination_angle = 0U;

static u16 g_link_alive_ticks = 0U;
static u16 g_link_close_ticks = 0U;
static u32 g_last_run_update_seq = 0UL;
static u32 g_last_poll_tick_ms = 0UL;
static u32 g_last_link_tick_ms = 0UL;

static u16 AutoDrive_Abs16(int16 value)
{
    if (value < 0) {
        return (u16)(-value);
    }
    return (u16)value;
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
    if ((point->lat_ns != 'N') && (point->lat_ns != 'S')) {
        return 0U;
    }
    if ((point->lon_whole == 0U) || (point->lat_whole == 0U)) {
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

    lat_min_x1e4 = ((lat_abs % 10000000UL) * 6UL + 500UL) / 1000UL;
    lon_min_x1e4 = ((lon_abs % 10000000UL) * 6UL + 500UL) / 1000UL;

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
    if (gps->fix_valid == 0U) {
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

static void AutoDrive_SetMotorForward(u16 pwm)
{
    Motor_SetBothSpeed((int16)pwm, (int16)pwm);
}

static void AutoDrive_SetMotorLeft(u16 pwm)
{
    Motor_SetBothSpeed(-(int16)pwm, (int16)pwm);
}

static void AutoDrive_SetMotorRight(u16 pwm)
{
    Motor_SetBothSpeed((int16)pwm, -(int16)pwm);
}

void AutoDrive_StopMotion(void)
{
    Motor_StopAll();
}

void AutoDrive_Stop(void)
{
    g_autoDrive_state = AUTO_DRIVE_IDLE;
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
    u16 distance;

    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        return 0U;
    }
    if (AutoDrive_PointRawValid(point) == 0U) {
        return 0U;
    }
    if (AutoDrive_PointRawValid(&g_idle_position) == 0U) {
        return 0U;
    }
    if (AutoDrive_GpsReady() == 0U) {
        return 0U;
    }

    distance = AutoDrive_GetDistanceNowToDestination((const u8 *)point,
                                                     (const u8 *)&g_idle_position);
    if ((distance > AUTODRIVE_MIN_ACTIVE_DISTANCE_M) &&
        (distance < AUTODRIVE_MAX_ACTIVE_DISTANCE_M)) {
        return 1U;
    }
    return 0U;
}

void AutoDrive_SetReturnPositionRaw(const u8 *data_m)
{
    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        return;
    }

    AutoDrive_CopyPoint(&g_return_position, (const AutoDrive_PointRaw_t *)data_m);
    if (AutoDrive_IsCanActive(&g_return_position) == 0U) {
        return;
    }

    AutoDrive_SetMode(AUTO_DRIVE_GO_HOME_POSITION);
    g_autoDrive_state = AUTO_DRIVE_START;
    g_autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;
    g_autoDrive_fail_flag = 0U;
}

void AutoDrive_SetFishPositionRaw(const u8 *data_m)
{
    if (g_autoDrive_state != AUTO_DRIVE_IDLE) {
        return;
    }

    AutoDrive_CopyPoint(&g_fish_position, (const AutoDrive_PointRaw_t *)data_m);
    if (AutoDrive_IsCanActive(&g_fish_position) == 0U) {
        return;
    }

    AutoDrive_SetMode(AUTO_DRIVE_GO_FISISH_POSITION);
    g_autoDrive_state = AUTO_DRIVE_START;
    g_autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;
    g_autoDrive_fail_flag = 0U;
}

void AutoDrive_TriggerReturn(void)
{
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

void AutoDrive_WorkOvertimeFail(void)
{
    g_autoDrive_fail_flag = 1U;
}

void AutoDrive_SetSwitchRaw(const u8 *data_m, u8 len)
{
    if ((data_m == 0) || (len == 0U)) {
        return;
    }

    g_autoDrive_switch = data_m[0];
    g_autodrv_cfg.auto_ret_onoff = data_m[0];
    if (len >= (u8)(1U + sizeof(AutoDrive_PointRaw_t))) {
        AutoDrive_CopyPoint(&g_autodrv_cfg.ret_point,
                            (const AutoDrive_PointRaw_t *)&data_m[1]);
    }
    (void)AutoDriveCfg_Save(&g_autodrv_cfg);
}

void AutoDrive_GetStoredConfig(AutoDrive_ReturnConfig_t *cfg)
{
    if (cfg != 0) {
        *cfg = g_autodrv_cfg;
    }
}

static void AutoDrive_SetTurnTimes(u16 times)
{
    if (times > 200U) {
        times = 200U;
    }
    g_autoDrive_turn_times = times;
}

static void AutoDrive_StartRunLine(u16 speed)
{
    AutoDrive_SetMotorForward(speed);
}

static void AutoDrive_TurnHandle(void)
{
    if (g_autoDrive_turn_times > 0U) {
        g_autoDrive_turn_times--;
    } else {
        AutoDrive_StartRunLine(AUTODRIVE_DRIVE_FAST_PWM);
    }
}

u8 AutoDrive_GetDirectionNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData)
{
    const AutoDrive_PointRaw_t *nowposition;
    const AutoDrive_PointRaw_t *desposition;
    u8 direction;

    nowposition = (const AutoDrive_PointRaw_t *)nowpositionData;
    desposition = (const AutoDrive_PointRaw_t *)despositionData;
    direction = 0U;

    if ((nowposition->lon_whole > desposition->lon_whole) ||
        ((nowposition->lon_whole == desposition->lon_whole) &&
         (nowposition->lon_frac > desposition->lon_frac))) {
        direction = POSITION_WEST;
        if ((nowposition->lat_whole > desposition->lat_whole) ||
            ((nowposition->lat_whole == desposition->lat_whole) &&
             (nowposition->lat_frac > desposition->lat_frac))) {
            direction = POSITION_WEST_SOUTH;
        } else if ((nowposition->lat_whole < desposition->lat_whole) ||
                   ((nowposition->lat_whole == desposition->lat_whole) &&
                    (nowposition->lat_frac < desposition->lat_frac))) {
            direction = POSITION_WEST_NORTH;
        }
    }

    if ((nowposition->lon_whole < desposition->lon_whole) ||
        ((nowposition->lon_whole == desposition->lon_whole) &&
         (nowposition->lon_frac < desposition->lon_frac))) {
        direction = POSITION_EAST;
        if ((nowposition->lat_whole > desposition->lat_whole) ||
            ((nowposition->lat_whole == desposition->lat_whole) &&
             (nowposition->lat_frac > desposition->lat_frac))) {
            direction = POSITION_EAST_SOUTH;
        } else if ((nowposition->lat_whole < desposition->lat_whole) ||
                   ((nowposition->lat_whole == desposition->lat_whole) &&
                    (nowposition->lat_frac < desposition->lat_frac))) {
            direction = POSITION_EAST_NORTH;
        }
    }

    if (direction == 0U) {
        if ((nowposition->lat_whole > desposition->lat_whole) ||
            ((nowposition->lat_whole == desposition->lat_whole) &&
             (nowposition->lat_frac > desposition->lat_frac))) {
            direction = POSITION_SOUTH;
        } else {
            direction = POSITION_NORTH;
        }
    }

    return direction;
}

static u32 AutoDrive_CalDistanceLon(const AutoDrive_PointRaw_t *now,
                                    const AutoDrive_PointRaw_t *des)
{
    u16 dd1;
    u16 dd2;
    u32 sec1;
    u32 sec2;
    u16 ddiff;
    u32 sec_diff;

    dd1 = now->lon_whole / 100U;
    dd2 = des->lon_whole / 100U;
    sec1 = ((u32)(now->lon_whole % 100U) * AUTODRIVE_MINUTE_SCALE) + (u32)now->lon_frac;
    sec2 = ((u32)(des->lon_whole % 100U) * AUTODRIVE_MINUTE_SCALE) + (u32)des->lon_frac;

    if (dd1 == dd2) {
        sec_diff = AutoDrive_Abs32Diff(sec1, sec2);
        return (sec_diff * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE;
    }

    if (dd1 > dd2) {
        ddiff = (u16)(dd1 - dd2);
        if (sec1 > sec2) {
            sec_diff = sec1 - sec2;
        } else {
            ddiff -= 1U;
            sec1 += (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE);
            sec_diff = sec1 - sec2;
        }
    } else {
        ddiff = (u16)(dd2 - dd1);
        if (sec2 > sec1) {
            sec_diff = sec2 - sec1;
        } else {
            ddiff -= 1U;
            sec2 += (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE);
            sec_diff = sec2 - sec1;
        }
    }

    if (ddiff != 0U) {
        return ((u32)ddiff * AUTODRIVE_METERS_PER_DEG) +
               ((sec_diff * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE);
    }
    return (sec_diff * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE;
}

static u32 AutoDrive_CalDistanceLat(const AutoDrive_PointRaw_t *now,
                                    const AutoDrive_PointRaw_t *des)
{
    u16 dd1;
    u16 dd2;
    u32 sec1;
    u32 sec2;
    u16 ddiff;
    u32 sec_diff;

    dd1 = now->lat_whole / 100U;
    dd2 = des->lat_whole / 100U;
    sec1 = ((u32)(now->lat_whole % 100U) * AUTODRIVE_MINUTE_SCALE) + (u32)now->lat_frac;
    sec2 = ((u32)(des->lat_whole % 100U) * AUTODRIVE_MINUTE_SCALE) + (u32)des->lat_frac;

    if (dd1 == dd2) {
        sec_diff = AutoDrive_Abs32Diff(sec1, sec2);
        return (sec_diff * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE;
    }

    if (dd1 > dd2) {
        ddiff = (u16)(dd1 - dd2);
        if (sec1 > sec2) {
            sec_diff = sec1 - sec2;
        } else {
            ddiff -= 1U;
            sec1 += (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE);
            sec_diff = sec1 - sec2;
        }
    } else {
        ddiff = (u16)(dd2 - dd1);
        if (sec2 > sec1) {
            sec_diff = sec2 - sec1;
        } else {
            ddiff -= 1U;
            sec2 += (AUTODRIVE_MINUTES_PER_DEG * AUTODRIVE_MINUTE_SCALE);
            sec_diff = sec2 - sec1;
        }
    }

    if (ddiff != 0U) {
        return ((u32)ddiff * AUTODRIVE_METERS_PER_DEG) +
               ((sec_diff * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE);
    }
    return (sec_diff * AUTODRIVE_METERS_PER_MINUTE) / AUTODRIVE_MINUTE_SCALE;
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

    distance_width = AutoDrive_CalDistanceLon(nowposition, desposition);
    distance_height = AutoDrive_CalDistanceLat(nowposition, desposition);

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

    distance_width = AutoDrive_CalDistanceLon(nowposition, desposition);
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

static u16 AutoDrive_GetCurrentHeadingDeg(void)
{
    const GPS_State_t *gps;

    gps = GPS_GetState();
    if ((gps != 0) && (gps->course_deg_x100 <= 36000U) &&
        (gps->speed_kmh_x100 >= 80UL)) {
        return (u16)(gps->course_deg_x100 / 100U);
    }

    if (AHRS_IsReady()) {
        const AHRS_State_t *ahrs;
        int16 yaw;

        ahrs = AHRS_GetState();
        yaw = ahrs->yaw_deg100;
        while (yaw < 0) {
            yaw = (int16)(yaw + 36000);
        }
        while (yaw >= 36000) {
            yaw = (int16)(yaw - 36000);
        }
        return (u16)(yaw / 100U);
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

void AutoDrive_Init(void)
{
    AutoDriveCfg_Init();
    AutoDriveCfg_Load(&g_autodrv_cfg);
    if (g_autodrv_cfg.auto_ret_onoff == 0xFFU) {
        g_autodrv_cfg.auto_ret_onoff = 0x30U;
    }

    g_autoDrive_switch = g_autodrv_cfg.auto_ret_onoff;
    g_autoDrive_state = AUTO_DRIVE_IDLE;
    g_autoDrive_mode = AUTO_DRIVE_CLOSE;
    g_autoDrive_turn_times = 0U;
    g_autodrive_work_overtime = 0U;
    g_autoDrive_fail_flag = 0U;
    g_link_alive_ticks = 0U;
    g_link_close_ticks = 0U;
    g_last_run_update_seq = 0UL;
    g_last_poll_tick_ms = Task_GetTickMs();
    g_last_link_tick_ms = g_last_poll_tick_ms;

    if (g_motor_ready == 0U) {
        Motor_Init();
        g_motor_ready = 1U;
    }
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
        AutoDrive_TriggerReturn();
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
    static u8 running_wait_times = 0U;
    const GPS_State_t *gps;
    u16 destination_distance;
    u16 moved_distance;
    u16 turn_angle;
    u16 current_heading;
    u8 turn_dir;
    int16 direction_diff;
    u32 now_ms;

    now_ms = Task_GetTickMs();
    if ((now_ms - g_last_poll_tick_ms) < 10U) {
        return;
    }
    g_last_poll_tick_ms = now_ms;

    gps = GPS_GetState();
    if (gps != 0) {
        AutoDrive_UpdateIdlePosition();
    }

    switch (g_autoDrive_state) {
    case AUTO_DRIVE_IDLE:
        break;

    case AUTO_DRIVE_START:
        if (g_autoDrive_mode == AUTO_DRIVE_GO_FISISH_POSITION) {
            g_destination_direction =
                AutoDrive_GetDirectionNowToDestination((const u8 *)&g_idle_position,
                                                       (const u8 *)&g_fish_position);
            AutoDrive_StartRunLine(AUTODRIVE_DRIVE_PWM);
            AutoDrive_SetTurnTimes(0U);
            g_autoDrive_state = AUTO_DRIVE_GET_DIRECTION;
        } else if (g_autoDrive_mode == AUTO_DRIVE_GO_HOME_POSITION) {
            g_destination_direction =
                AutoDrive_GetDirectionNowToDestination((const u8 *)&g_idle_position,
                                                       (const u8 *)&g_return_position);

            current_heading = AutoDrive_GetCurrentHeadingDeg();
            if (current_heading == 65535U) {
                current_heading = 0U;
            }

            switch (g_destination_direction) {
            case POSITION_NORTH:
                direction_diff = (int16)(0 - (int16)current_heading);
                break;
            case POSITION_EAST_NORTH:
                direction_diff = (int16)(315 - (int16)current_heading);
                break;
            case POSITION_EAST:
                direction_diff = (int16)(270 - (int16)current_heading);
                break;
            case POSITION_EAST_SOUTH:
                direction_diff = (int16)(225 - (int16)current_heading);
                break;
            case POSITION_SOUTH:
                direction_diff = (int16)(180 - (int16)current_heading);
                break;
            case POSITION_WEST_SOUTH:
                direction_diff = (int16)(135 - (int16)current_heading);
                break;
            case POSITION_WEST:
                direction_diff = (int16)(90 - (int16)current_heading);
                break;
            case POSITION_WEST_NORTH:
                direction_diff = (int16)(45 - (int16)current_heading);
                break;
            default:
                direction_diff = 0;
                break;
            }

            if (direction_diff < -180) {
                direction_diff = (int16)(direction_diff + 360);
            } else if (direction_diff > 180) {
                direction_diff = (int16)(direction_diff - 360);
            }

            if (direction_diff > 5) {
                AutoDrive_SetMotorLeft(AUTODRIVE_TURN_PWM);
            } else if (direction_diff < -5) {
                AutoDrive_SetMotorRight(AUTODRIVE_TURN_PWM);
            } else {
                AutoDrive_SetMotorForward(AUTODRIVE_DRIVE_PWM);
            }

            AutoDrive_SetTurnTimes((u16)(AutoDrive_Abs16(direction_diff) >> 3));
            g_autoDrive_state = AUTO_DRIVE_GET_DIRECTION;
        } else {
            AutoDrive_Stop();
            break;
        }
        AutoDrive_TurnHandle();
        break;

    case AUTO_DRIVE_GET_DIRECTION:
        AutoDrive_TurnHandle();
        if (g_autoDrive_turn_times == 0U) {
            if (AutoDrive_GpsReady() == 0U) {
                AutoDrive_Stop();
                break;
            }
            AutoDrive_PointFromGps(&g_last_position, gps);
            g_last_run_update_seq = gps->update_sequence;
            g_autoDrive_state = AUTO_DRIVE_RUNING;
            AutoDrive_StartRunLine(AUTODRIVE_DRIVE_PWM);
            AutoDrive_SetTurnTimes(100U);
        }
        break;

    case AUTO_DRIVE_RUNING:
        AutoDrive_TurnHandle();

        if (g_autodrive_work_overtime > 0U) {
            g_autodrive_work_overtime--;
        } else {
            AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
            AutoDrive_WorkOvertimeFail();
            break;
        }

        if (AutoDrive_GpsReady() == 0U) {
            break;
        }
        if (g_autoDrive_turn_times != 0U) {
            break;
        }
        if ((gps == 0) || (gps->update_sequence == g_last_run_update_seq)) {
            break;
        }

        if (running_wait_times >= 2U) {
            running_wait_times = 0U;
        } else {
            running_wait_times++;
            break;
        }

        AutoDrive_UpdateGpsStepPoints();
        g_nowrun_direction =
            AutoDrive_GetDirectionNowToDestination((const u8 *)&g_last_position,
                                                   (const u8 *)&g_now_position);

        if (g_autoDrive_mode == AUTO_DRIVE_GO_FISISH_POSITION) {
            g_destination_angle =
                AutoDrive_GetAngelNowToDestination((const u8 *)&g_now_position,
                                                   (const u8 *)&g_fish_position);
            g_destination_direction =
                AutoDrive_GetDirectionNowToDestination((const u8 *)&g_now_position,
                                                       (const u8 *)&g_fish_position);
            if (g_destination_angle == 65535U) {
                break;
            }
            destination_distance =
                AutoDrive_GetDistanceNowToDestination((const u8 *)&g_now_position,
                                                      (const u8 *)&g_fish_position);
        } else if (g_autoDrive_mode == AUTO_DRIVE_GO_HOME_POSITION) {
            g_destination_angle =
                AutoDrive_GetAngelNowToDestination((const u8 *)&g_now_position,
                                                   (const u8 *)&g_return_position);
            g_destination_direction =
                AutoDrive_GetDirectionNowToDestination((const u8 *)&g_now_position,
                                                       (const u8 *)&g_return_position);
            if (g_destination_angle == 65535U) {
                break;
            }
            destination_distance =
                AutoDrive_GetDistanceNowToDestination((const u8 *)&g_now_position,
                                                      (const u8 *)&g_return_position);
        } else {
            AutoDrive_Stop();
            break;
        }

        if (destination_distance < AUTODRIVE_ARRIVE_DISTANCE_M) {
            g_autoDrive_state = AUTO_DRIVE_IDLE;
            AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
            AutoDrive_StopMotion();
            break;
        }

        g_destination_angle =
            AutoDrive_GetNorthAngel(g_destination_direction, (u8)g_destination_angle);

        moved_distance =
            AutoDrive_GetDistanceNowToDestination((const u8 *)&g_now_position,
                                                  (const u8 *)&g_last_position);
        if (moved_distance < AUTODRIVE_STRAIGHT_DISTANCE_X10_M) {
            AutoDrive_SetTurnTimes(60U);
            AutoDrive_StartRunLine(AUTODRIVE_DRIVE_FAST_PWM);
            break;
        }

        g_nowrun_angle = AutoDrive_GetCurrentHeadingDeg();
        if ((g_nowrun_angle == 65535U) || (g_nowrun_angle > 360U)) {
            g_nowrun_angle =
                AutoDrive_GetAngelNowToDestination((const u8 *)&g_last_position,
                                                   (const u8 *)&g_now_position);
            if ((g_nowrun_angle == 65535U) || (g_nowrun_angle > 360U)) {
                AutoDrive_StartRunLine(AUTODRIVE_DRIVE_FAST_PWM);
                break;
            }
            g_nowrun_angle =
                AutoDrive_GetNorthAngel(g_nowrun_direction, (u8)g_nowrun_angle);
        }

        direction_diff = (int16)((g_destination_angle - g_nowrun_angle + 360U) % 360U);
        if (direction_diff > 180) {
            turn_angle = (u16)(360 - direction_diff);
            turn_dir = 1U;
        } else {
            turn_angle = (u16)direction_diff;
            turn_dir = 2U;
        }

        if (turn_angle < 10U) {
            AutoDrive_SetMotorForward(AUTODRIVE_DRIVE_FAST_PWM);
        } else {
            if (turn_dir == 1U) {
                AutoDrive_SetMotorLeft(AUTODRIVE_TURN_PWM);
            } else {
                AutoDrive_SetMotorRight(AUTODRIVE_TURN_PWM);
            }
            AutoDrive_SetTurnTimes(turn_angle);
        }

        AutoDrive_CopyPoint(&g_last_position, &g_now_position);
        g_last_run_update_seq = gps->update_sequence;
        break;

    default:
        g_autodrive_work_overtime = 0U;
        AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
        break;
    }
}
