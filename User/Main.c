/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#include "config.h"
#include "system_init.h"
#include "Task.h"
#include "..\Code_boweny\Device\QMI8658\QMI8658.h"
#include "..\Code_boweny\Device\GPS\GPS.h"
#include "..\Code_boweny\Device\WIRELESS\wireless.h"
#include "..\Code_boweny\Device\WIRELESS\ship_protocol.h"
#include "..\Code_boweny\Function\Log\Log.h"

#define IMU_ACC_LOG_DECIMATION  128

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

static void IMU_HighRatePoll(void)
{
    static u16 sample_div = 0;
    static u8  error_latched = 0;
    int16 ax, ay, az;

    if (!g_qmi8658_ready) {
        return;
    }

    if (QMI8658_ReadAcc(&ax, &ay, &az) != 0) {
        if (!error_latched) {
            LOGE("IMU", "high-rate acc read fail");
            error_latched = 1;
        }
        return;
    }

    error_latched = 0;
    sample_div++;
    if (sample_div >= IMU_ACC_LOG_DECIMATION) {
        sample_div = 0;
        LOGI("IMU", "acc=%d %d %d", ax, ay, az);
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
