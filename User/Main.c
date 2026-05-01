/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#include "config.h"
#include "system_init.h"
#include "Task.h"
#include "..\Code_boweny\Device\QMI8658\QMI8658.h"
#include "..\Code_boweny\Device\QMC6309\QMC6309.h"
#include "..\Code_boweny\Device\GPS\GPS.h"
#include "..\Code_boweny\Device\WIRELESS\wireless.h"
#include "..\Code_boweny\Device\WIRELESS\ship_protocol.h"
#include "..\Code_boweny\Function\AHRS\AHRS.h"
#include "..\Code_boweny\Function\Log\Log.h"

#ifndef AHRS_TEST_ONLY
#define AHRS_TEST_ONLY  0
#endif

#ifndef AHRS_LOG_DECIMATION
#define AHRS_LOG_DECIMATION  32
#endif
#define AHRS_YAW_ZERO_STABLE_CD       150
#define AHRS_YAW_ZERO_STABLE_SAMPLES  6

#if !AHRS_TEST_ONLY
#define MAG_TEST_PERIOD_MS   1000U

static void Wireless_MinimalTestUnit(void)
{
    s8 rc;

    /*
     * LT8920 datasheet does not provide a dedicated WHO_AM_I style ID register.
     * This one-shot test confirms SPI communication by reading back stable registers.
     */
    rc = Wireless_RunMinimalTest();
    if (rc != SUCCESS) {
        LOGE("WL", "minimal test fail rc=%d", rc);
    }
}

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
    static u8  timing_started = 0;
    static u32 last_mag_ms = 0;
    static u8  error_latched = 0;
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

static u8 AHRS_SignChar(int16 value)
{
    return (value < 0) ? '-' : '+';
}

static u16 AHRS_AbsWholeCd(int16 value)
{
    int32 v;

    v = (int32)value;
    if (v < 0) {
        v = -v;
    }
    return (u16)(v / 100L);
}

static u16 AHRS_AbsFracCd(int16 value)
{
    int32 v;

    v = (int32)value;
    if (v < 0) {
        v = -v;
    }
    return (u16)(v % 100L);
}

/**
 * @brief   高频 IMU/AHRS 轮询入口
 * @return  none
 *
 * @details
 * - 使用 `Task_GetTickMs()` 将 QMI8658 6 轴读取固定到 AHRS 配置周期
 * - 读取 `QMI8658_ReadAll()` 后调用 `AHRS_UpdateRaw6Axis()` 更新姿态
 * - 每 `AHRS_MAG_PERIOD_MS` 读取一次滤波地磁数据，通过 `AHRS_UpdateRawMag()` 慢修正航向
 * - Output sampled degree angles and flags for UART AHRS testing.
 */
static void IMU_HighRatePoll(void)
{
    static u8  timing_started = 0;
    static u32 last_imu_ms = 0;
    static u32 last_mag_ms = 0;
    static u16 sample_div = 0;
    static u8  error_latched = 0;
    static u8  yaw_zero_valid = 0;
    static u8  yaw_last_valid = 0;
    static u8  yaw_stable_count = 0;
    static int16 yaw_zero_cd = 0;
    static int16 yaw_last_cd = 0;
    u32 now_ms;
    u32 elapsed_ms;
    u16 dt_ms;
    int16 ax, ay, az;
    int16 gx, gy, gz;
    int16 mx, my, mz;
    int16 yaw_rel_cd;
    int16 yaw_delta_cd;
    const AHRS_State_t *att;

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

    if (elapsed_ms > AHRS_DT_MAX_MS) {
        dt_ms = AHRS_DT_MAX_MS;
    } else {
        dt_ms = (u16)elapsed_ms;
    }

    if (QMI8658_ReadAll(&ax, &ay, &az, &gx, &gy, &gz) != 0) {
        if (!error_latched) {
            LOGE("AHRS", "imu read fail");
            error_latched = 1;
        }
        return;
    }

    error_latched = 0;

    if (AHRS_UpdateRaw6Axis(ax, ay, az, gx, gy, gz, dt_ms) != 0) {
        return;
    }

    if ((now_ms - last_mag_ms) >= AHRS_MAG_PERIOD_MS) {
        last_mag_ms = now_ms;
        if (QMC6309_ReadXYZFiltered(&mx, &my, &mz) == 0) {
            AHRS_UpdateRawMag(mx, my, mz);
        }
    }

    sample_div++;
    if (sample_div >= AHRS_LOG_DECIMATION) {
        sample_div = 0;
        att = AHRS_GetState();
        if ((att->flags & AHRS_FLAG_GYRO_BIAS_READY) == 0) {
            yaw_zero_valid = 0;
            yaw_last_valid = 0;
            yaw_stable_count = 0;
            LOGI("AHRS",
                 "r=%c%u.%02u p=%c%u.%02u y=%c%u.%02u g=%c%u.%02u %c%u.%02u %c%u.%02u f=%02X",
                 AHRS_SignChar(att->roll_deg100),
                 AHRS_AbsWholeCd(att->roll_deg100),
                 AHRS_AbsFracCd(att->roll_deg100),
                 AHRS_SignChar(att->pitch_deg100),
                 AHRS_AbsWholeCd(att->pitch_deg100),
                 AHRS_AbsFracCd(att->pitch_deg100),
                 AHRS_SignChar(att->yaw_deg100),
                 AHRS_AbsWholeCd(att->yaw_deg100),
                 AHRS_AbsFracCd(att->yaw_deg100),
                 AHRS_SignChar(att->gyro_x_dps100),
                 AHRS_AbsWholeCd(att->gyro_x_dps100),
                 AHRS_AbsFracCd(att->gyro_x_dps100),
                 AHRS_SignChar(att->gyro_y_dps100),
                 AHRS_AbsWholeCd(att->gyro_y_dps100),
                 AHRS_AbsFracCd(att->gyro_y_dps100),
                 AHRS_SignChar(att->gyro_z_dps100),
                 AHRS_AbsWholeCd(att->gyro_z_dps100),
                 AHRS_AbsFracCd(att->gyro_z_dps100),
                 att->flags);
        } else {
            if (!yaw_zero_valid) {
                if (!yaw_last_valid) {
                    yaw_last_cd = att->yaw_deg100;
                    yaw_last_valid = 1;
                    yaw_stable_count = 0;
                } else {
                    yaw_delta_cd =
                        AHRS_WrapCdLocal((int32)att->yaw_deg100 - yaw_last_cd);
                    yaw_last_cd = att->yaw_deg100;
                    if (yaw_delta_cd < 0) {
                        yaw_delta_cd = (int16)(-yaw_delta_cd);
                    }
                    if (yaw_delta_cd <= AHRS_YAW_ZERO_STABLE_CD) {
                        if (yaw_stable_count < 255U) {
                            yaw_stable_count++;
                        }
                    } else {
                        yaw_stable_count = 0;
                    }
                }

                if (yaw_stable_count >= AHRS_YAW_ZERO_STABLE_SAMPLES) {
                    yaw_zero_cd = att->yaw_deg100;
                    yaw_zero_valid = 1;
                }
            }

            if (!yaw_zero_valid) {
                LOGI("AHRS",
                     "r=%c%u.%02u p=%c%u.%02u y=%c%u.%02u ys=%u g=%c%u.%02u %c%u.%02u %c%u.%02u f=%02X",
                     AHRS_SignChar(att->roll_deg100),
                     AHRS_AbsWholeCd(att->roll_deg100),
                     AHRS_AbsFracCd(att->roll_deg100),
                     AHRS_SignChar(att->pitch_deg100),
                     AHRS_AbsWholeCd(att->pitch_deg100),
                     AHRS_AbsFracCd(att->pitch_deg100),
                     AHRS_SignChar(att->yaw_deg100),
                     AHRS_AbsWholeCd(att->yaw_deg100),
                     AHRS_AbsFracCd(att->yaw_deg100),
                     (u16)yaw_stable_count,
                     AHRS_SignChar(att->gyro_x_dps100),
                     AHRS_AbsWholeCd(att->gyro_x_dps100),
                     AHRS_AbsFracCd(att->gyro_x_dps100),
                     AHRS_SignChar(att->gyro_y_dps100),
                     AHRS_AbsWholeCd(att->gyro_y_dps100),
                     AHRS_AbsFracCd(att->gyro_y_dps100),
                     AHRS_SignChar(att->gyro_z_dps100),
                     AHRS_AbsWholeCd(att->gyro_z_dps100),
                     AHRS_AbsFracCd(att->gyro_z_dps100),
                     att->flags);
                return;
            }
            yaw_rel_cd = AHRS_WrapCdLocal((int32)att->yaw_deg100 - yaw_zero_cd);

            LOGI("AHRS",
                 "r=%c%u.%02u p=%c%u.%02u y=%c%u.%02u yr=%c%u.%02u f=%02X",
                 AHRS_SignChar(att->roll_deg100),
                 AHRS_AbsWholeCd(att->roll_deg100),
                 AHRS_AbsFracCd(att->roll_deg100),
                 AHRS_SignChar(att->pitch_deg100),
                 AHRS_AbsWholeCd(att->pitch_deg100),
                 AHRS_AbsFracCd(att->pitch_deg100),
                 AHRS_SignChar(att->yaw_deg100),
                 AHRS_AbsWholeCd(att->yaw_deg100),
                 AHRS_AbsFracCd(att->yaw_deg100),
                 AHRS_SignChar(yaw_rel_cd),
                 AHRS_AbsWholeCd(yaw_rel_cd),
                 AHRS_AbsFracCd(yaw_rel_cd),
                 att->flags);
        }
    }
}

/*---------------------------------------------------------------------*/
/* 鍑芥暟: main                                                          */
/* 鎻忚堪: 绋嬪簭鍏ュ彛                                                      */
/* 鍙傛暟: 鏃?                                                            */
/* 杩斿洖: 鏃?                                                            */
/* 鐗堟湰: v1.0, 2026-04-22                                             */
/*---------------------------------------------------------------------*/
void main(void)
{
    SYS_Init();
#if !AHRS_TEST_ONLY
    Wireless_MinimalTestUnit();
#endif

    while(1)
    {
#if !AHRS_TEST_ONLY
        GPS_Poll();
        Wireless_Poll();
        ShipProtocol_Poll();
        Wireless_SearchSignalPoll();
        MAG_StandalonePoll();
#endif
        Task_Pro_Handler_Callback();
        IMU_HighRatePoll();
    }
}
