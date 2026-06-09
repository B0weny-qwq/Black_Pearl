/**
 * @file    MainLoop.c
 * @brief   当前固件主循环与统一航向输出入口。
 *
 * @details
 * 本文件负责组织 GPS、无线协议、AHRS、磁力计和控制层轮询节拍，并提供
 * 上层统一读取的航向/角速度快照。当前工程里：
 * - `MainLoop_GetRawHeadingDeg100()` 保留 AHRS 原始融合航向；
 * - `MainLoop_GetHeadingDeg100()` 在原始航向上叠加
 *   `NorthCalib_GetHeadingOffsetCd()`，形成导航统一口径；
 * - AutoDrive、手动 yaw 自稳、E 键定速巡航都必须复用该统一航向接口。
 */
#include "config.h"
#include "system_init.h"
#include "Task.h"
#include "MainLoop.h"
#include "..\Code_boweny\Device\QMC6309\QMC6309.h"
#include "..\Code_boweny\Device\QMI8658\QMI8658.h"
#include "..\Code_boweny\Device\GPS\GPS.h"
#include "..\Code_boweny\Device\Motor\Motor.h"
#include "..\Code_boweny\Device\Control\ShipControl.h"
#include "..\Code_boweny\Device\AutoDrive\NorthCalib.h"
#include "..\Code_boweny\Device\WIRELESS\wireless.h"
#include "..\Code_boweny\Device\WIRELESS\ship_protocol.h"
#include "..\Code_boweny\Function\AHRS\AHRS.h"
#include "..\Code_boweny\Function\AHRS\HeadingEstimator.h"
#include "..\Code_boweny\Function\Log\Log.h"

#ifndef MAINLOOP_WIRELESS_BOOT_SELF_TEST
#define MAINLOOP_WIRELESS_BOOT_SELF_TEST 0U
#endif

#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP && MAINLOOP_WIRELESS_BOOT_SELF_TEST
static void Wireless_MinimalTestUnit(void)
{
    s8 rc;

    LOGI("SYS", "wireless runtime enabled");
    rc = Wireless_RunMinimalTest();
    if (rc != SUCCESS) {
        LOGE("WL", "minimal test fail rc=%d", rc);
    }
}
#endif

#if ENABLE_MAG_MODULE || ENABLE_IMU_MODULE
typedef enum
{
    SENSOR_BOOT_WAIT_PAIR = 0,
    SENSOR_BOOT_I2C_PREPARE,
    SENSOR_BOOT_MAG_INIT,
    SENSOR_BOOT_IMU_START,
    SENSOR_BOOT_IMU_WAIT_READY,
    SENSOR_BOOT_READY,
    SENSOR_BOOT_FAILED
} SensorBootState_t;

static SensorBootState_t g_sensor_boot_state = SENSOR_BOOT_WAIT_PAIR;
static u8 g_sensor_boot_started = 0U;

static void MainLoop_StartSensorBootIfPaired(void);
static void MainLoop_ServiceSensorBoot(void);
static u8 MainLoop_SensorsReady(void);
#endif

#if ENABLE_MAG_MODULE
#define MAG_ANGLE_90_CD     9000L
#define MAG_ANGLE_180_CD    18000L
#define MAG_ANGLE_360_CD    36000L

#ifndef MAG_COMPASS_READY_COUNT
#define MAG_COMPASS_READY_COUNT        5U
#endif
#ifndef MAG_COMPASS_STATIC_SETTLE_MS
#define MAG_COMPASS_STATIC_SETTLE_MS   3000UL
#endif
#ifndef MAG_COMPASS_IIR_DIV
#define MAG_COMPASS_IIR_DIV            8L
#endif
#ifndef MAG_COMPASS_JUMP_GATE_CD
#define MAG_COMPASS_JUMP_GATE_CD       3000L
#endif
#ifndef MAG_COMPASS_NORM_TRACK_PCT
#define MAG_COMPASS_NORM_TRACK_PCT     25U
#endif
#ifndef MAG_COMPASS_NORM_REJECT_PCT
#define MAG_COMPASS_NORM_REJECT_PCT    80U
#endif
#ifndef MAG_COMPASS_HORIZ_MIN_SUM
#define MAG_COMPASS_HORIZ_MIN_SUM      40UL
#endif
#ifndef MAG_COMPASS_RAW_OFFSET_CD
#define MAG_COMPASS_RAW_OFFSET_CD      0L
#endif
#ifndef MAG_COMPASS_DIRECTION_SIGN
/* 当前板载安装方向会让原始罗盘角与手机罗盘方向相反。 */
#define MAG_COMPASS_DIRECTION_SIGN     (-1L)
#endif
#ifndef MAG_COMPASS_INSTALL_OFFSET_CD
/* 船头指向真北时，原始罗盘读数约为 219.3 度。 */
#define MAG_COMPASS_INSTALL_OFFSET_CD  21930L
#endif
#ifndef MAG_COMPASS_DECLINATION_CD
#define MAG_COMPASS_DECLINATION_CD     0L
#endif

static u8 g_mag_heading_ready_snapshot = 0U;
static u16 g_mag_heading_deg100_snapshot = 0U;
static u8 g_mag_filter_started = 0U;
static u8 g_mag_filter_stable_count = 0U;
static u32 g_mag_norm_base = 0UL;
static int32 g_mag_heading_iir_cd = 0L;

#if ENABLE_MAG_STANDALONE_POLL
#define MAG_TEST_PERIOD_MS  SHIP_MAG_LOG_PERIOD_MS
#endif

static u32 MAG_Abs16ToU32(int16 value)
{
    int32 v;

    v = (int32)value;
    if (v < 0) {
        return (u32)(-v);
    }

    return (u32)v;
}

static u32 MAG_Abs32ToU32(int32 value)
{
    if (value < 0L) {
        return (u32)(-value);
    }
    return (u32)value;
}

static u16 MAG_WrapDeg100(int32 angle_cd)
{
    while (angle_cd >= MAG_ANGLE_360_CD) {
        angle_cd -= MAG_ANGLE_360_CD;
    }
    while (angle_cd < 0L) {
        angle_cd += MAG_ANGLE_360_CD;
    }
    return (u16)angle_cd;
}

static u16 MAG_Atan01Deg100(u16 z_q10)
{
    int32 z;
    int32 curve;
    int32 angle;

    if (z_q10 > 1024U) {
        z_q10 = 1024U;
    }

    z = (int32)z_q10;
    curve = 4500L + ((1564L * (1024L - z)) / 1024L);
    angle = (z * curve) / 1024L;
    return (u16)angle;
}

static u16 MAG_Atan2Deg100(int32 y, int32 x)
{
    u32 ax;
    u32 ay;
    u16 z_q10;
    int32 base;
    int32 angle;

    ax = (x < 0L) ? (u32)(-x) : (u32)x;
    ay = (y < 0L) ? (u32)(-y) : (u32)y;

    if ((ax == 0UL) && (ay == 0UL)) {
        return 0U;
    }

    if (ax >= ay) {
        z_q10 = (u16)((ay * 1024UL) / ax);
        base = (int32)MAG_Atan01Deg100(z_q10);
        angle = (x >= 0L) ? base : (MAG_ANGLE_180_CD - base);
    } else {
        z_q10 = (u16)((ax * 1024UL) / ay);
        base = (int32)MAG_Atan01Deg100(z_q10);
        angle = (x >= 0L) ? (MAG_ANGLE_90_CD - base) : (MAG_ANGLE_90_CD + base);
    }

    if (y < 0L) {
        angle = -angle;
    }

    return MAG_WrapDeg100(angle);
}

static int32 MAG_WrapDiffDeg100(int32 diff_cd)
{
    while (diff_cd >= MAG_ANGLE_180_CD) {
        diff_cd -= MAG_ANGLE_360_CD;
    }
    while (diff_cd < -MAG_ANGLE_180_CD) {
        diff_cd += MAG_ANGLE_360_CD;
    }
    return diff_cd;
}

static u8 MAG_CompassHeadingDeg100(int16 raw_x, int16 raw_y, int16 raw_z,
                                   u16 *heading_out,
                                   u32 *norm1_out,
                                   u32 *horiz_sum_out)
{
    int16 body_x;
    int16 body_y;
    int16 body_z;
    int32 compass_cd;
    u32 norm1;
    u32 horiz_sum;

    if ((heading_out == 0) || (norm1_out == 0) || (horiz_sum_out == 0)) {
        return 0U;
    }

    body_x = 0;
    body_y = 0;
    body_z = 0;
    AHRS_MapRawMagToBody(raw_x, raw_y, raw_z, &body_x, &body_y, &body_z);
    norm1 = MAG_Abs16ToU32(body_x) + MAG_Abs16ToU32(body_y) + MAG_Abs16ToU32(body_z);
    horiz_sum = MAG_Abs16ToU32(body_x) + MAG_Abs16ToU32(body_y);
    *norm1_out = norm1;
    *horiz_sum_out = horiz_sum;
    if ((norm1 == 0UL) || (horiz_sum < (u32)MAG_COMPASS_HORIZ_MIN_SUM)) {
        return 0U;
    }

    compass_cd = (int32)MAG_Atan2Deg100((int32)(-body_y), (int32)body_x);
    compass_cd += (int32)MAG_COMPASS_RAW_OFFSET_CD;
    compass_cd *= (int32)MAG_COMPASS_DIRECTION_SIGN;
    compass_cd += (int32)MAG_COMPASS_INSTALL_OFFSET_CD;
    compass_cd += (int32)MAG_COMPASS_DECLINATION_CD;
    *heading_out = MAG_WrapDeg100(compass_cd);
    return 1U;
}

static u8 MAG_UpdateCompassFilter(int16 raw_x, int16 raw_y, int16 raw_z,
                                  u16 *heading_out)
{
    u16 raw_heading_cd;
    u32 norm1;
    u32 horiz_sum;
    u32 norm_diff;
    int32 heading_diff_cd;

    if (heading_out == 0) {
        return 0U;
    }
    if (MAG_CompassHeadingDeg100(raw_x, raw_y, raw_z,
                                 &raw_heading_cd,
                                 &norm1,
                                 &horiz_sum) == 0U) {
        return 0U;
    }

    if (g_mag_norm_base == 0UL) {
        g_mag_norm_base = norm1;
    }
    if (norm1 > g_mag_norm_base) {
        norm_diff = norm1 - g_mag_norm_base;
    } else {
        norm_diff = g_mag_norm_base - norm1;
    }
    if ((g_mag_norm_base > 0UL) &&
        ((norm_diff * 100UL) > (g_mag_norm_base * (u32)MAG_COMPASS_NORM_REJECT_PCT))) {
        if (g_mag_filter_stable_count > 0U) {
            g_mag_filter_stable_count--;
        }
        return 0U;
    }
    if ((g_mag_norm_base > 0UL) &&
        ((norm_diff * 100UL) <= (g_mag_norm_base * (u32)MAG_COMPASS_NORM_TRACK_PCT))) {
        g_mag_norm_base += ((int32)norm1 - (int32)g_mag_norm_base) / 8L;
    }

    if (g_mag_filter_started == 0U) {
        g_mag_heading_iir_cd = (int32)raw_heading_cd;
        g_mag_heading_deg100_snapshot = raw_heading_cd;
        g_mag_filter_started = 1U;
        g_mag_filter_stable_count = 1U;
        return 0U;
    }

    heading_diff_cd = MAG_WrapDiffDeg100((int32)raw_heading_cd - g_mag_heading_iir_cd);
    if (MAG_Abs32ToU32(heading_diff_cd) > (u32)MAG_COMPASS_JUMP_GATE_CD) {
        g_mag_filter_stable_count = 0U;
        return 0U;
    }

    if (MAG_COMPASS_IIR_DIV > 1L) {
        g_mag_heading_iir_cd += heading_diff_cd / (int32)MAG_COMPASS_IIR_DIV;
    } else {
        g_mag_heading_iir_cd = (int32)raw_heading_cd;
    }
    g_mag_heading_iir_cd = (int32)MAG_WrapDeg100(g_mag_heading_iir_cd);

    if (g_mag_filter_stable_count < 255U) {
        g_mag_filter_stable_count++;
    }
    if (g_mag_filter_stable_count >= (u8)MAG_COMPASS_READY_COUNT) {
        g_mag_heading_ready_snapshot = 1U;
        g_mag_heading_deg100_snapshot = (u16)g_mag_heading_iir_cd;
        *heading_out = g_mag_heading_deg100_snapshot;
        return 1U;
    }

    return 0U;
}

static void MAG_ResetCompassFilter(void)
{
    g_mag_heading_ready_snapshot = 0U;
    g_mag_filter_started = 0U;
    g_mag_filter_stable_count = 0U;
    g_mag_norm_base = 0UL;
    g_mag_heading_iir_cd = 0L;
}

#if ENABLE_MAG_STANDALONE_POLL
static void MAG_StandalonePoll(void)
{
    static u8 timing_started = 0;
    static u32 last_mag_ms = 0;
    static u8 error_latched = 0;
    u32 now_ms;
    u32 norm1;
    u32 horiz_sum;
    u16 compass_cd;
    int16 mx, my, mz;

    now_ms = Task_GetTickMs();
    if (!timing_started) {
        timing_started = 1;
        last_mag_ms = now_ms;
    } else if ((now_ms - last_mag_ms) < MAG_TEST_PERIOD_MS) {
        return;
    } else {
        last_mag_ms = now_ms;
    }

    if (QMC6309_ReadXYZ(&mx, &my, &mz) != 0) {
        if (!error_latched) {
            LOGW("MAG", "test read fail id=0x%02X", QMC6309_ReadID());
            error_latched = 1;
        }
        return;
    }

    error_latched = 0;
    if (MAG_CompassHeadingDeg100(mx, my, mz, &compass_cd, &norm1, &horiz_sum) != 0U) {
        LOGI("MAG", "test raw=%d %d %d norm1=%lu compass=%u.%02u stable=%u",
             mx,
             my,
             mz,
             norm1,
             (u16)(compass_cd / 100U),
             (u16)(compass_cd % 100U),
             (u16)g_mag_heading_ready_snapshot);
    } else {
        norm1 = MAG_Abs16ToU32(mx) + MAG_Abs16ToU32(my) + MAG_Abs16ToU32(mz);
        LOGI("MAG", "test raw=%d %d %d norm1=%lu stable=%u",
             mx,
             my,
             mz,
             norm1,
             (u16)g_mag_heading_ready_snapshot);
    }
}
#endif
#endif

#if ENABLE_IMU_MODULE
#define IMU_BASIC_LOG_PERIOD_MS        SHIP_IMU_LOG_PERIOD_MS
#ifndef AHRS_LOG_DECIMATION
#define AHRS_LOG_DECIMATION            32U
#endif

#if ENABLE_IMU_AHRS_POLL
static HeadingEstimator_t xdata g_heading;
static int16 g_heading_rel_cd_snapshot = 0;
static int16 g_gyro_z_dps100_snapshot = 0;
static u8 g_heading_ready_snapshot = 0U;
static char xdata g_roll_buf[10];
static char xdata g_pitch_buf[10];
static char xdata g_yaw_buf[10];
static char xdata g_heading_buf[10];
static char xdata g_yaw_rel_buf[10];
static char xdata g_yaw_gyro_buf[10];
static char xdata g_yaw_mag_buf[10];
static char xdata g_gx_buf[12];
static char xdata g_gy_buf[12];
static char xdata g_gz_buf[12];
static char xdata g_bias_buf[12];
static char xdata g_err_buf[10];
static char xdata g_hp_buf[10];
static char xdata g_hd_buf[10];
static char xdata g_hm_buf[10];
static char xdata g_ahrs_log_buf[192];
#endif

static u32 AHRS_Abs32Local(int32 value);
static void AHRS_LogSensorWarn(const AHRS_State_t *att,
                               u32 now_ms,
                               int16 mag_x,
                               int16 mag_y,
                               int16 mag_z,
                               u8 mag_valid,
                               u8 mag_ready,
                               u8 mag_settled,
                               u8 mag_used,
                               u8 heading_seeded,
                               u8 heading_ready,
                               u8 heading_static)
{
#if ENABLE_IMU_AHRS_POLL
    static u32 last_gyro_log_ms = 0UL;
    static u32 last_mag_log_ms = 0UL;
    u32 mag_norm;

    if (att == 0) {
        return;
    }

    if ((SHIP_IMU_LOG_PERIOD_MS == 0U) ||
        ((now_ms - last_gyro_log_ms) >= SHIP_IMU_LOG_PERIOD_MS)) {
        last_gyro_log_ms = now_ms;
        LOGW("GYRO", "g=%d %d %d f=%02X hd=%u st=%u seed=%u",
             att->gyro_x_dps100,
             att->gyro_y_dps100,
             att->gyro_z_dps100,
             (u16)att->flags,
             (u16)heading_ready,
             (u16)heading_static,
             (u16)heading_seeded);
    }

    if ((SHIP_MAG_LOG_PERIOD_MS == 0U) ||
        ((now_ms - last_mag_log_ms) >= SHIP_MAG_LOG_PERIOD_MS)) {
        last_mag_log_ms = now_ms;
        mag_norm = (u32)(AHRS_Abs32Local((int32)(mag_valid ? mag_x : 0)) +
                         AHRS_Abs32Local((int32)(mag_valid ? mag_y : 0)) +
                         AHRS_Abs32Local((int32)(mag_valid ? mag_z : 0)));
        LOGW("MAG", "raw=%d %d %d n=%lu mv=%u mr=%u ms=%u mu=%u",
             mag_valid ? mag_x : 0,
             mag_valid ? mag_y : 0,
             mag_valid ? mag_z : 0,
             mag_norm,
             (u16)mag_valid,
             (u16)mag_ready,
             (u16)mag_settled,
             (u16)mag_used);
    }
#else
    (void)att;
    (void)now_ms;
    (void)mag_x;
    (void)mag_y;
    (void)mag_z;
    (void)mag_valid;
    (void)mag_ready;
    (void)mag_settled;
    (void)mag_used;
    (void)heading_seeded;
    (void)heading_ready;
    (void)heading_static;
#endif
}

static u8 IMU_ServicePoll(void)
{
    static u8 service_error_latched = 0;
    s8 service_rc;

    service_rc = QMI8658_Service();
    if (service_rc != 0) {
        g_qmi8658_ready = 0;
        if (!service_error_latched) {
            LOGE("IMU", "service fail state=%u err=%s",
                 (u16)QMI8658_GetState(),
                 QMI8658_GetLastI2cErrorName());
            service_error_latched = 1;
        }
        return 0U;
    }

    service_error_latched = 0;
    g_qmi8658_ready = QMI8658_IsReady();
    return g_qmi8658_ready;
}

#if ENABLE_IMU_BASIC_POLL
static void IMU_BasicPoll(void)
{
    static u8 timing_started = 0;
    static u32 last_log_ms = 0;
    static u8 read_error_latched = 0;
    u32 now_ms;
    int16 ax, ay, az;
    int16 gx, gy, gz;
    int16 temp;

    now_ms = Task_GetTickMs();
    if (!timing_started) {
        timing_started = 1;
        last_log_ms = now_ms;
        return;
    }
    if ((now_ms - last_log_ms) < IMU_BASIC_LOG_PERIOD_MS) {
        return;
    }
    last_log_ms = now_ms;

    if (QMI8658_ReadAll(&ax, &ay, &az, &gx, &gy, &gz) != 0) {
        if (!read_error_latched) {
            LOGW("IMU", "basic read fail state=%u err=%s",
                 (u16)QMI8658_GetState(),
                 QMI8658_GetLastI2cErrorName());
            read_error_latched = 1;
        }
        return;
    }

    read_error_latched = 0;
    if (QMI8658_ReadTemp(&temp) != 0) {
        temp = 0;
    }

    LOGI("IMU",
         "basic raw addr=0x%02X temp=%d acc=%d %d %d gyro=%d %d %d",
         QMI8658_I2C_Addr, temp, ax, ay, az, gx, gy, gz);
}
#endif

#if ENABLE_IMU_AHRS_POLL
static int16 AHRS_WrapCdLocal(int32 angle_cd)
{
    while (angle_cd >= 18000L) {
        angle_cd -= 36000L;
    }
    while (angle_cd < -18000L) {
        angle_cd += 36000L;
    }
    return (int16)angle_cd;
}

static u8 AHRS_SignChar(int32 value)
{
    return (value < 0) ? '-' : '+';
}

static u16 AHRS_AbsWholeCd(int32 value)
{
    int32 v;

    v = (int32)value;
    if (v < 0) {
        v = -v;
    }
    return (u16)(v / 100L);
}

static u16 AHRS_AbsFracCd(int32 value)
{
    int32 v;

    v = (int32)value;
    if (v < 0) {
        v = -v;
    }
    return (u16)(v % 100L);
}

static u32 AHRS_Abs32Local(int32 value)
{
    if (value < 0) {
        return (u32)(-value);
    }
    return (u32)value;
}

static int32 AHRS_FloatToCdLocal(float value)
{
    float scaled;

    scaled = value * 100.0f;
    if (scaled >= 0.0f) {
        return (int32)(scaled + 0.5f);
    }
    return (int32)(scaled - 0.5f);
}

static char *AHRS_MagStatusSuffix(u8 flags)
{
#if AHRS_MAG_ENABLE
    if ((flags & AHRS_FLAG_MAG_VALID) == 0U) {
        return " mv=0";
    }

    return "";
#else
    (void)flags;
    return " me=0";
#endif
}

static void AHRS_FormatSignedCd(int32 value, char *buf)
{
    (void)sprintf(buf, "%c%u.%02u",
                  AHRS_SignChar(value),
                  AHRS_AbsWholeCd(value),
                  AHRS_AbsFracCd(value));
}

static u8 AHRS_IsHeadingStatic(const AHRS_State_t *att)
{
    int16 left_speed;
    int16 right_speed;

    if (att == 0) {
        return 0U;
    }

    if (ShipControl_GetMode() != SHIP_CONTROL_MODE_STOP) {
        return 0U;
    }
    left_speed = Motor_GetSpeed(MOTOR_LEFT);
    right_speed = Motor_GetSpeed(MOTOR_RIGHT);
    if ((left_speed != 0) || (right_speed != 0)) {
        return 0U;
    }

    if ((att->flags & AHRS_FLAG_GYRO_BIAS_READY) == 0U) {
        return 0U;
    }
    if ((att->flags & AHRS_FLAG_ACC_VALID) == 0U) {
        return 0U;
    }
    if (AHRS_Abs32Local((int32)att->gyro_x_dps100) > AHRS_GYRO_STILL_DPS100) {
        return 0U;
    }
    if (AHRS_Abs32Local((int32)att->gyro_y_dps100) > AHRS_GYRO_STILL_DPS100) {
        return 0U;
    }
    if (AHRS_Abs32Local((int32)att->gyro_z_dps100) > AHRS_GYRO_STILL_DPS100) {
        return 0U;
    }

    return 1U;
}

static u8 AHRS_HasSelfStabilize(const AHRS_State_t *att)
{
    if (att == 0) {
        return 0U;
    }

    if ((att->flags & AHRS_FLAG_GYRO_BIAS_READY) == 0U) {
        return 0U;
    }
    if ((att->flags & AHRS_FLAG_MAG_VALID) == 0U) {
        return 0U;
    }

    return AHRS_IsHeadingStatic(att);
}

static void IMU_AhrsPoll(void)
{
    static u8 timing_started = 0;
    static u32 last_imu_ms = 0;
    static u32 last_mag_ms = 0;
    static u16 sample_div = 0;
    static u8 read_error_latched = 0;
    static u8 yaw_zero_valid = 0;
    static u8 heading_seeded = 0;
    static u8 last_heading_static_flag = 0U;
    static u32 heading_static_start_ms = 0UL;
    static int32 yaw_gyro_zero_cd = 0;
    static int32 yaw_mag_zero_cd = 0;
    static int16 last_mag_x = 0;
    static int16 last_mag_y = 0;
    static int16 last_mag_z = 0;
    static u8 last_mag_valid = 0U;
    u32 now_ms;
    u32 elapsed_ms;
    u16 dt_ms;
    int16 ax, ay, az;
    int16 gx, gy, gz;
    int16 mx, my, mz;
    u16 stable_mag_heading_cd;
    int16 yaw_rel_cd;
    int16 yaw_gyro_rel_cd;
    int16 yaw_mag_rel_cd;
    int16 heading_abs_cd;
    int16 heading_gyro_cd;
    int16 heading_mag_cd;
    int32 heading_gyro_abs_cd;
    int32 heading_mag_abs_cd;
    int32 heading_bias_cd;
    int32 heading_err_cd;
    int32 heading_pred_cd;
    int32 heading_fused_cd;
    int32 heading_mag_dbg_cd;
    u8 stable_mag_valid;
    u8 heading_static_flag;
    u8 heading_mag_settled;
    u8 self_stabilize_flag;
    float heading_seed_deg;
    float heading_dt_s;
    char *mag_suffix;
    const AHRS_State_t *att;
    u8 ahrs_log_len;

    heading_static_flag = 0U;
    heading_mag_settled = 0U;
    self_stabilize_flag = 0U;

    if (!g_qmi8658_ready) {
        return;
    }

    now_ms = Task_GetTickMs();
    if (!timing_started) {
        last_imu_ms = now_ms;
        last_mag_ms = now_ms;
        timing_started = 1;
        return;
    }

    elapsed_ms = now_ms - last_imu_ms;
    if (elapsed_ms < AHRS_IMU_PERIOD_MS) {
        return;
    }
    last_imu_ms = now_ms;
    dt_ms = (elapsed_ms > AHRS_DT_MAX_MS) ? AHRS_DT_MAX_MS : (u16)elapsed_ms;

    if (QMI8658_ReadAll(&ax, &ay, &az, &gx, &gy, &gz) != 0) {
        if (!read_error_latched) {
            LOGW("AHRS", "imu read fail state=%u err=%s",
                 (u16)QMI8658_GetState(),
                 QMI8658_GetLastI2cErrorName());
            read_error_latched = 1;
        }
        return;
    }

    read_error_latched = 0;

    if (AHRS_UpdateRaw6Axis(ax, ay, az, gx, gy, gz, dt_ms) != 0) {
        return;
    }

    att = AHRS_GetState();
    if ((att->flags & AHRS_FLAG_READY) != 0U) {
        heading_static_flag = AHRS_IsHeadingStatic(att);
        self_stabilize_flag = AHRS_HasSelfStabilize(att);
        if ((heading_static_flag != 0U) && (last_heading_static_flag == 0U)) {
            heading_static_start_ms = now_ms;
            MAG_ResetCompassFilter();
        }
        if (heading_static_flag == 0U) {
            heading_static_start_ms = 0UL;
        } else if ((now_ms - heading_static_start_ms) >= MAG_COMPASS_STATIC_SETTLE_MS) {
            heading_mag_settled = 1U;
        }
        last_heading_static_flag = heading_static_flag;
    } else {
        last_heading_static_flag = 0U;
        heading_static_start_ms = 0UL;
    }

    stable_mag_heading_cd = g_mag_heading_deg100_snapshot;
    stable_mag_valid = 0U;
    if ((now_ms - last_mag_ms) >= AHRS_MAG_PERIOD_MS) {
        last_mag_ms = now_ms;
        if (QMC6309_ReadXYZFiltered(&mx, &my, &mz) == 0) {
            if ((heading_mag_settled != 0U) &&
                (MAG_UpdateCompassFilter(mx, my, mz, &stable_mag_heading_cd) != 0U)) {
                stable_mag_valid = 1U;
            }
            last_mag_x = mx;
            last_mag_y = my;
            last_mag_z = mz;
            last_mag_valid = 1U;
        }
    }

    if ((att->flags & AHRS_FLAG_READY) == 0U) {
        yaw_zero_valid = 0;
        yaw_gyro_zero_cd = 0L;
        yaw_mag_zero_cd = 0L;
        heading_seeded = 0U;
        g_heading_rel_cd_snapshot = 0;
        g_heading_ready_snapshot = 0U;
        Heading_Init(&g_heading);
    } else {
        heading_dt_s = (float)dt_ms * 0.001f;

        if (!heading_seeded) {
            if ((g_mag_heading_ready_snapshot == 0U) ||
                (heading_mag_settled == 0U)) {
                yaw_zero_valid = 0U;
                g_heading_rel_cd_snapshot = 0;
                g_heading_ready_snapshot = 0U;
                AHRS_LogSensorWarn(att,
                                   now_ms,
                                   last_mag_x,
                                   last_mag_y,
                                   last_mag_z,
                                   last_mag_valid,
                                   g_mag_heading_ready_snapshot,
                                   heading_mag_settled,
                                   0U,
                                   heading_seeded,
                                   g_heading_ready_snapshot,
                                   heading_static_flag);
                return;
            }
            heading_seed_deg = (float)g_mag_heading_deg100_snapshot * 0.01f;
            Heading_SetHeadingDeg(&g_heading, heading_seed_deg);
            Heading_ResetZero(&g_heading);
            heading_seeded = 1U;
            yaw_zero_valid = 1U;
            yaw_gyro_zero_cd = Heading_GetGyroDeg100(&g_heading);
            yaw_mag_zero_cd = Heading_GetMagDeg100(&g_heading);
            g_heading_rel_cd_snapshot = 0;
            g_heading_ready_snapshot = 1U;
        }

        Heading_Update(&g_heading,
                       (float)att->gyro_z_dps100 * 0.01f,
                       (float)stable_mag_heading_cd * 0.01f,
                       stable_mag_valid,
                       heading_static_flag,
                       heading_dt_s);
        g_gyro_z_dps100_snapshot = att->gyro_z_dps100;
    }

    if (yaw_zero_valid != 0U) {
        g_heading_rel_cd_snapshot = AHRS_WrapCdLocal(Heading_GetRelativeDeg100(&g_heading));
        g_heading_ready_snapshot = 1U;
    } else {
        g_heading_rel_cd_snapshot = 0;
        g_heading_ready_snapshot = 0U;
    }

    AHRS_LogSensorWarn(att,
                       now_ms,
                       last_mag_x,
                       last_mag_y,
                       last_mag_z,
                       last_mag_valid,
                       g_mag_heading_ready_snapshot,
                       heading_mag_settled,
                       g_heading.mag_used,
                       heading_seeded,
                       g_heading_ready_snapshot,
                       heading_static_flag);

    sample_div++;
    if (sample_div < AHRS_LOG_DECIMATION) {
        return;
    }
    sample_div = 0;

    mag_suffix = AHRS_MagStatusSuffix(att->flags);
    AHRS_FormatSignedCd(att->roll_deg100, g_roll_buf);
    AHRS_FormatSignedCd(att->pitch_deg100, g_pitch_buf);
    AHRS_FormatSignedCd(att->yaw_deg100, g_yaw_buf);

    if ((att->flags & AHRS_FLAG_GYRO_BIAS_READY) == 0U) {
        AHRS_FormatSignedCd(att->gyro_x_dps100, g_gx_buf);
        AHRS_FormatSignedCd(att->gyro_y_dps100, g_gy_buf);
        AHRS_FormatSignedCd(att->gyro_z_dps100, g_gz_buf);
        ahrs_log_len = (u8)sprintf(g_ahrs_log_buf,
                                   "r=%s p=%s y=%s",
                                   g_roll_buf,
                                   g_pitch_buf,
                                   g_yaw_buf);
        ahrs_log_len += (u8)sprintf(g_ahrs_log_buf + ahrs_log_len,
                                    " g=%s %s %s f=%02X%s",
                                    g_gx_buf,
                                    g_gy_buf,
                                    g_gz_buf,
                                    att->flags,
                                    mag_suffix);
        LOGI("AHRS", "%s", g_ahrs_log_buf);
        return;
    }

    heading_abs_cd = AHRS_WrapCdLocal(Heading_GetDeg100(&g_heading));
    heading_gyro_abs_cd = Heading_GetGyroDeg100(&g_heading);
    heading_mag_abs_cd = Heading_GetMagDeg100(&g_heading);
    heading_gyro_cd = AHRS_WrapCdLocal(heading_gyro_abs_cd);
    heading_mag_cd = AHRS_WrapCdLocal(heading_mag_abs_cd);
    heading_bias_cd = Heading_GetBiasDps100(&g_heading);
    heading_err_cd = AHRS_FloatToCdLocal(g_heading.heading_err_deg);
    heading_pred_cd = AHRS_WrapCdLocal(AHRS_FloatToCdLocal(g_heading.heading_pred_deg));
    heading_fused_cd = heading_abs_cd;
    heading_mag_dbg_cd = heading_mag_cd;

    if (!yaw_zero_valid) {
        AHRS_FormatSignedCd(att->gyro_x_dps100, g_gx_buf);
        AHRS_FormatSignedCd(att->gyro_y_dps100, g_gy_buf);
        AHRS_FormatSignedCd(att->gyro_z_dps100, g_gz_buf);
        ahrs_log_len = (u8)sprintf(g_ahrs_log_buf,
                                   "r=%s p=%s y=%s ys=%u",
                                   g_roll_buf,
                                   g_pitch_buf,
                                   g_yaw_buf,
                                   1U);
        ahrs_log_len += (u8)sprintf(g_ahrs_log_buf + ahrs_log_len,
                                    " g=%s %s %s f=%02X%s",
                                    g_gx_buf,
                                    g_gy_buf,
                                    g_gz_buf,
                                    att->flags,
                                    mag_suffix);
        LOGI("AHRS", "%s", g_ahrs_log_buf);

        AHRS_FormatSignedCd(heading_abs_cd, g_heading_buf);
        AHRS_FormatSignedCd(heading_gyro_cd, g_yaw_gyro_buf);
        AHRS_FormatSignedCd(heading_mag_cd, g_yaw_mag_buf);
        AHRS_FormatSignedCd(heading_bias_cd, g_bias_buf);
        AHRS_FormatSignedCd(heading_err_cd, g_err_buf);
        AHRS_FormatSignedCd(heading_pred_cd, g_hp_buf);
        AHRS_FormatSignedCd(heading_fused_cd, g_hd_buf);
        AHRS_FormatSignedCd(heading_mag_dbg_cd, g_hm_buf);
        ahrs_log_len = (u8)sprintf(g_ahrs_log_buf,
                                   "y=%s yg=%s ym=%s ys=%u",
                                   g_heading_buf,
                                   g_yaw_gyro_buf,
                                   g_yaw_mag_buf,
                                   1U);
        ahrs_log_len += (u8)sprintf(g_ahrs_log_buf + ahrs_log_len,
                                    " gz=%s bz=%s mv=%u mu=%u sf=%u err=%s hp=%s hd=%s hm=%s f=%02X",
                                    g_gz_buf,
                                    g_bias_buf,
                                    (u16)g_heading.raw_mag_valid,
                                    (u16)g_heading.mag_used,
                                    (u16)g_heading.static_flag,
                                    g_err_buf,
                                    g_hp_buf,
                                    g_hd_buf,
                                    g_hm_buf,
                                    att->flags);
        LOGI("HDG", "%s", g_ahrs_log_buf);
        LOGI("MAG", "raw=%d %d %d norm=%lu yaw=%s self=%u",
             last_mag_valid ? last_mag_x : 0,
             last_mag_valid ? last_mag_y : 0,
             last_mag_valid ? last_mag_z : 0,
             (u32)(AHRS_Abs32Local((int32)(last_mag_valid ? last_mag_x : 0)) +
                   AHRS_Abs32Local((int32)(last_mag_valid ? last_mag_y : 0)) +
                   AHRS_Abs32Local((int32)(last_mag_valid ? last_mag_z : 0))),
             g_yaw_mag_buf,
             (u16)self_stabilize_flag);
        return;
    }

    yaw_rel_cd = AHRS_WrapCdLocal(Heading_GetRelativeDeg100(&g_heading));
    yaw_gyro_rel_cd = AHRS_WrapCdLocal(heading_gyro_abs_cd - yaw_gyro_zero_cd);
    yaw_mag_rel_cd = AHRS_WrapCdLocal(heading_mag_abs_cd - yaw_mag_zero_cd);
    AHRS_FormatSignedCd(yaw_rel_cd, g_yaw_rel_buf);
    ahrs_log_len = (u8)sprintf(g_ahrs_log_buf,
                               "r=%s p=%s y=%s yr=%s f=%02X%s",
                               g_roll_buf,
                               g_pitch_buf,
                               g_yaw_buf,
                               g_yaw_rel_buf,
                               att->flags,
                               mag_suffix);
    LOGI("AHRS", "%s", g_ahrs_log_buf);

    AHRS_FormatSignedCd(yaw_gyro_rel_cd, g_yaw_gyro_buf);
    AHRS_FormatSignedCd(yaw_mag_rel_cd, g_yaw_mag_buf);
    AHRS_FormatSignedCd(att->gyro_z_dps100, g_gz_buf);
    AHRS_FormatSignedCd(heading_bias_cd, g_bias_buf);
    AHRS_FormatSignedCd(heading_err_cd, g_err_buf);
    AHRS_FormatSignedCd(heading_pred_cd, g_hp_buf);
    AHRS_FormatSignedCd(heading_fused_cd, g_hd_buf);
    AHRS_FormatSignedCd(heading_mag_dbg_cd, g_hm_buf);
    ahrs_log_len = (u8)sprintf(g_ahrs_log_buf,
                               "yr=%s yg=%s ym=%s",
                               g_yaw_rel_buf,
                               g_yaw_gyro_buf,
                               g_yaw_mag_buf);
    ahrs_log_len += (u8)sprintf(g_ahrs_log_buf + ahrs_log_len,
                                " gz=%s bz=%s mv=%u mu=%u sf=%u err=%s hp=%s hd=%s hm=%s f=%02X",
                                g_gz_buf,
                                g_bias_buf,
                                (u16)g_heading.raw_mag_valid,
                                (u16)g_heading.mag_used,
                                (u16)g_heading.static_flag,
                                g_err_buf,
                                g_hp_buf,
                                g_hd_buf,
                                g_hm_buf,
                                att->flags);
    LOGI("HDG", "%s", g_ahrs_log_buf);
    LOGI("MAG", "raw=%d %d %d norm=%lu yaw=%s self=%u",
         last_mag_valid ? last_mag_x : 0,
         last_mag_valid ? last_mag_y : 0,
         last_mag_valid ? last_mag_z : 0,
         (u32)(AHRS_Abs32Local((int32)(last_mag_valid ? last_mag_x : 0)) +
               AHRS_Abs32Local((int32)(last_mag_valid ? last_mag_y : 0)) +
               AHRS_Abs32Local((int32)(last_mag_valid ? last_mag_z : 0))),
         g_yaw_mag_buf,
         (u16)self_stabilize_flag);
}
#endif
#endif

void MainLoop_Bootstrap(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    Heading_Init(&g_heading);
    g_heading_rel_cd_snapshot = 0;
    g_heading_ready_snapshot = 0U;
#endif
    NorthCalib_Init();

#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP && MAINLOOP_WIRELESS_BOOT_SELF_TEST
    Wireless_MinimalTestUnit();
#endif
}

#if ENABLE_MAG_MODULE || ENABLE_IMU_MODULE
static void MainLoop_StartSensorBootIfPaired(void)
{
#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP && ENABLE_SHIP_PROTOCOL_SCHED
    if (ShipProtocol_IsPaired() == 0U) {
        return;
    }
#endif

    if (g_sensor_boot_started != 0U) {
        return;
    }

    g_sensor_boot_started = 1U;
    g_sensor_boot_state = SENSOR_BOOT_I2C_PREPARE;
    LOGI("SYS", "sensor boot start after rc pair");
}

static u8 MainLoop_SensorsReady(void)
{
    return ((g_sensor_boot_started != 0U) &&
            (g_sensor_boot_state == SENSOR_BOOT_READY)) ? 1U : 0U;
}

static void MainLoop_ServiceSensorBoot(void)
{
    if (g_sensor_boot_started == 0U) {
        return;
    }

    switch (g_sensor_boot_state) {
    case SENSOR_BOOT_I2C_PREPARE:
        Sensor_I2C_prepare();
        g_qmi8658_ready = 0U;
        AHRS_Reset();
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
        Heading_Init(&g_heading);
        g_heading_rel_cd_snapshot = 0;
        g_heading_ready_snapshot = 0U;
#endif
        g_sensor_boot_state = SENSOR_BOOT_MAG_INIT;
        return;

    case SENSOR_BOOT_MAG_INIT:
#if ENABLE_MAG_MODULE
        if (QMC6309_Init() == 0) {
            LOGI("SYS", "mag init ready");
        } else {
            LOGE("SYS", "mag init failed");
            g_sensor_boot_state = SENSOR_BOOT_FAILED;
            return;
        }
#endif
        g_sensor_boot_state = SENSOR_BOOT_IMU_START;
        return;

    case SENSOR_BOOT_IMU_START:
#if ENABLE_IMU_MODULE
#if QMI8658_INIT_NONBLOCKING
        QMI8658_RequestReinit();
        g_qmi8658_ready = 0U;
        g_sensor_boot_state = SENSOR_BOOT_IMU_WAIT_READY;
#else
        g_qmi8658_ready = (QMI8658_Init() == 0) ? 1U : 0U;
        if (g_qmi8658_ready != 0U) {
            LOGI("SYS", "imu init ready");
            g_sensor_boot_state = SENSOR_BOOT_READY;
        } else {
            LOGE("SYS", "imu init failed");
            g_sensor_boot_state = SENSOR_BOOT_FAILED;
        }
#endif
#else
        g_qmi8658_ready = 1U;
        g_sensor_boot_state = SENSOR_BOOT_READY;
#endif
        return;

    case SENSOR_BOOT_IMU_WAIT_READY:
#if ENABLE_IMU_MODULE
        if (IMU_ServicePoll() != 0U) {
            LOGI("SYS", "imu init ready");
            g_sensor_boot_state = SENSOR_BOOT_READY;
        } else if (QMI8658_GetState() == QMI8658_STATE_FAILED) {
            g_sensor_boot_state = SENSOR_BOOT_FAILED;
            LOGE("SYS", "imu init failed");
        }
#else
        g_sensor_boot_state = SENSOR_BOOT_READY;
#endif
        return;

    case SENSOR_BOOT_READY:
        return;

    case SENSOR_BOOT_FAILED:
        return;

    case SENSOR_BOOT_WAIT_PAIR:
    default:
        return;
    }
}
#endif

u8 MainLoop_IsHeadingReady(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    if (g_heading_ready_snapshot != 0U) {
        return 1U;
    }
#endif
    return 0U;
}

/** @brief 返回 HeadingEstimator 输出的原始融合绝对航向。 */
u16 MainLoop_GetRawHeadingDeg100(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    int32 heading_cd;

    if (g_heading_ready_snapshot != 0U) {
        heading_cd = Heading_GetDeg100(&g_heading);
        while (heading_cd >= 36000L) {
            heading_cd -= 36000L;
        }
        while (heading_cd < 0L) {
            heading_cd += 36000L;
        }
        return (u16)heading_cd;
    }
#endif
    return 0U;
}

/** @brief 返回叠加当前北向偏移后的导航航向。 */
u16 MainLoop_GetHeadingDeg100(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    int32 heading_cd;

    if (g_heading_ready_snapshot != 0U) {
        heading_cd = (int32)MainLoop_GetRawHeadingDeg100() +
                     (int32)NorthCalib_GetHeadingOffsetCd();
        while (heading_cd >= 36000L) {
            heading_cd -= 36000L;
        }
        while (heading_cd < 0L) {
            heading_cd += 36000L;
        }
        return (u16)heading_cd;
    }
#endif
    return 0U;
}

u8 MainLoop_IsMagHeadingFallback(void)
{
#if ENABLE_MAG_MODULE && ENABLE_MAG_STANDALONE_POLL
    if (g_mag_heading_ready_snapshot != 0U) {
        return 1U;
    }
#else
    return 0U;
#endif
    return 0U;
}

int16 MainLoop_GetHeadingRelativeDeg100(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    return g_heading_rel_cd_snapshot;
#else
    return 0;
#endif
}

int16 MainLoop_GetGyroZDps100(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    return g_gyro_z_dps100_snapshot;
#else
    return 0;
#endif
}

void MainLoop_RunOnce(void)
{
#if ENABLE_GPS_MODULE
    GPS_Poll();
#endif

#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP
    Wireless_Poll();
#if ENABLE_SHIP_PROTOCOL_SCHED && SHIP_PROTOCOL_POLL_ENABLE
    ShipProtocol_RunScheduler();
#endif
#if SHIP_PROTOCOL_COMPAT_ENABLE
    ShipProtocol_Poll();
#endif
    Wireless_SearchSignalPoll();
#endif

#if ENABLE_MAG_MODULE || ENABLE_IMU_MODULE
    MainLoop_StartSensorBootIfPaired();
    MainLoop_ServiceSensorBoot();
#endif

#if ENABLE_MAG_MODULE && ENABLE_MAG_STANDALONE_POLL
    if (MainLoop_SensorsReady() != 0U) {
        MAG_StandalonePoll();
    }
#endif

#if ENABLE_IMU_MODULE
    if ((MainLoop_SensorsReady() != 0U) && (IMU_ServicePoll() != 0U)) {
#if ENABLE_IMU_AHRS_POLL
        IMU_AhrsPoll();
#elif ENABLE_IMU_BASIC_POLL
        IMU_BasicPoll();
#endif
    }
#endif

    Motor_Service();
    Task_Pro_Handler_Callback();
}
