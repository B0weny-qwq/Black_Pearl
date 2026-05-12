#include "config.h"
#include "system_init.h"
#include "Task.h"
#include "MainLoop.h"
#include "..\Code_boweny\Device\QMC6309\QMC6309.h"
#include "..\Code_boweny\Device\QMI8658\QMI8658.h"
#include "..\Code_boweny\Device\GPS\GPS.h"
#include "..\Code_boweny\Device\WIRELESS\wireless.h"
#include "..\Code_boweny\Device\WIRELESS\ship_protocol.h"
#include "..\Code_boweny\Function\AHRS\AHRS.h"
#include "..\Code_boweny\Function\AHRS\HeadingEstimator.h"
#include "..\Code_boweny\Function\Log\Log.h"

#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP
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

#if ENABLE_MAG_MODULE && ENABLE_MAG_STANDALONE_POLL
#define MAG_TEST_PERIOD_MS  1000U

static u32 MAG_Abs16ToU32(int16 value)
{
    int32 v;

    v = (int32)value;
    if (v < 0) {
        return (u32)(-v);
    }

    return (u32)v;
}

static void MAG_StandalonePoll(void)
{
    static u8 timing_started = 0;
    static u32 last_mag_ms = 0;
    static u8 error_latched = 0;
    u32 now_ms;
    u32 norm1;
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
    norm1 = MAG_Abs16ToU32(mx) + MAG_Abs16ToU32(my) + MAG_Abs16ToU32(mz);
    LOGI("MAG", "test raw=%d %d %d norm1=%lu", mx, my, mz, norm1);
}
#endif

#if ENABLE_IMU_MODULE
#define IMU_BASIC_LOG_PERIOD_MS        500U
#ifndef AHRS_LOG_DECIMATION
#define AHRS_LOG_DECIMATION            32U
#endif

#if ENABLE_IMU_AHRS_POLL
static HeadingEstimator_t xdata g_heading;
static int16 g_heading_rel_cd_snapshot = 0;
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
    if (att == 0) {
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

static void IMU_AhrsPoll(void)
{
    static u8 timing_started = 0;
    static u32 last_imu_ms = 0;
    static u32 last_mag_ms = 0;
    static u16 sample_div = 0;
    static u8 read_error_latched = 0;
    static u8 yaw_zero_valid = 0;
    static u8 heading_seeded = 0;
    static int32 yaw_gyro_zero_cd = 0;
    static int32 yaw_mag_zero_cd = 0;
    u32 now_ms;
    u32 elapsed_ms;
    u16 dt_ms;
    int16 ax, ay, az;
    int16 gx, gy, gz;
    int16 mx, my, mz;
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
    u8 raw_mag_valid;
    u8 heading_static_flag;
    float heading_seed_deg;
    float heading_dt_s;
    char *mag_suffix;
    const AHRS_State_t *att;
    u8 ahrs_log_len;

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

    if ((now_ms - last_mag_ms) >= AHRS_MAG_PERIOD_MS) {
        last_mag_ms = now_ms;
        if (QMC6309_ReadXYZFiltered(&mx, &my, &mz) == 0) {
            (void)AHRS_UpdateRawMag(mx, my, mz);
        }
    }

    att = AHRS_GetState();
    if ((att->flags & AHRS_FLAG_GYRO_BIAS_READY) == 0U) {
        yaw_zero_valid = 0;
        yaw_gyro_zero_cd = 0L;
        yaw_mag_zero_cd = 0L;
        heading_seeded = 0U;
        g_heading_rel_cd_snapshot = 0;
        g_heading_ready_snapshot = 0U;
        Heading_Init(&g_heading);
    } else {
        raw_mag_valid = ((att->flags & AHRS_FLAG_MAG_VALID) != 0U) ? 1U : 0U;
        heading_static_flag = AHRS_IsHeadingStatic(att);
        heading_dt_s = (float)dt_ms * 0.001f;

        if (!heading_seeded) {
            if (raw_mag_valid) {
                heading_seed_deg = (float)att->yaw_mag_deg100 * 0.01f;
            } else {
                heading_seed_deg = (float)att->yaw_deg100 * 0.01f;
            }
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
                       (float)att->yaw_mag_deg100 * 0.01f,
                       raw_mag_valid,
                       heading_static_flag,
                       heading_dt_s);
    }

    if (yaw_zero_valid != 0U) {
        g_heading_rel_cd_snapshot = AHRS_WrapCdLocal(Heading_GetRelativeDeg100(&g_heading));
        g_heading_ready_snapshot = 1U;
    } else {
        g_heading_rel_cd_snapshot = 0;
        g_heading_ready_snapshot = 0U;
    }

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

#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP
    Wireless_MinimalTestUnit();
#endif
}

u8 MainLoop_IsHeadingReady(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    return g_heading_ready_snapshot;
#else
    return 0U;
#endif
}

int16 MainLoop_GetHeadingRelativeDeg100(void)
{
#if ENABLE_IMU_MODULE && ENABLE_IMU_AHRS_POLL
    return g_heading_rel_cd_snapshot;
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

#if ENABLE_MAG_MODULE && ENABLE_MAG_STANDALONE_POLL
    MAG_StandalonePoll();
#endif

#if ENABLE_IMU_MODULE
    if (IMU_ServicePoll() != 0U) {
#if ENABLE_IMU_AHRS_POLL
        IMU_AhrsPoll();
#elif ENABLE_IMU_BASIC_POLL
        IMU_BasicPoll();
#endif
    }
#endif

    Task_Pro_Handler_Callback();
}
