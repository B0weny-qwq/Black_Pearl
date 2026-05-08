#include "config.h"
#include "system_init.h"
#include "Task.h"
#include "MainLoop.h"
#include "..\Code_boweny\Device\QMC6309\QMC6309.h"
#include "..\Code_boweny\Device\GPS\GPS.h"
#include "..\Code_boweny\Device\WIRELESS\wireless.h"
#include "..\Code_boweny\Device\WIRELESS\ship_protocol.h"
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
#define MAG_TEST_PERIOD_MS   3000U

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

void MainLoop_Bootstrap(void)
{
#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP
    Wireless_MinimalTestUnit();
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

    Task_Pro_Handler_Callback();
}
