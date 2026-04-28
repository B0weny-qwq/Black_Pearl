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

#define AHRS_LOG_DECIMATION  64

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

/**
 * @brief   高频 IMU/AHRS 轮询入口
 * @return  none
 *
 * @details
 * - 使用 `Task_GetTickMs()` 将 QMI8658 6 轴读取固定到 AHRS 配置周期
 * - 读取 `QMI8658_ReadAll()` 后调用 `AHRS_UpdateRaw6Axis()` 更新姿态
 * - 每 `AHRS_MAG_PERIOD_MS` 读取一次滤波地磁数据，通过 `AHRS_UpdateRawMag()` 慢修正航向
 * - 按抽样比例输出 `rpy_cd` 与 `gyro_dps100`，便于串口调试
 */
static void IMU_HighRatePoll(void)
{
    static u8  timing_started = 0;
    static u32 last_imu_ms = 0;
    static u32 last_mag_ms = 0;
    static u16 sample_div = 0;
    static u8  error_latched = 0;
    u32 now_ms;
    u32 elapsed_ms;
    u16 dt_ms;
    int16 ax, ay, az;
    int16 gx, gy, gz;
    int16 mx, my, mz;
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
        LOGI("AHRS", "rpy_cd=%d %d %d gyro_dps100=%d %d %d flags=0x%02X",
             att->roll_deg100,
             att->pitch_deg100,
             att->yaw_deg100,
             att->gyro_x_dps100,
             att->gyro_y_dps100,
             att->gyro_z_dps100,
             att->flags);
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
    Wireless_MinimalTestUnit();

    while(1)
    {
        GPS_Poll();
        Wireless_Poll();
        ShipProtocol_Poll();
        Wireless_SearchSignalPoll();
        Task_Pro_Handler_Callback();
        IMU_HighRatePoll();
    }
}
