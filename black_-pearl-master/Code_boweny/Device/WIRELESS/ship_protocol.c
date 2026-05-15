#include "ship_protocol.h"

#include <string.h>

#include "wireless.h"
#include "..\AutoDrive\autodrive.h"
#include "..\Motor\Motor.h"
#include "..\Power\power_Adc.h"
#include "..\..\..\User\Task.h"
#include "..\..\Function\Log\Log.h"
#include "..\GPS\GPS.h"

#define SHIP_TAG "SHIP"
#define SHIP_LOWPOWER_CHECK_TICKS 600U

static u8 g_now_pwm_accelerator = 0U;
static u16 g_lowpower_check_times = 0U;
static u32 g_last_lowpower_tick_ms = 0UL;

static u8 ShipProtocol_Xor(const u8 *buf, u8 len)
{
    u8 i;
    u8 val;

    val = 0U;
    for (i = 0U; i < len; i++) {
        val ^= buf[i];
    }
    return val;
}

static void ShipProtocol_WriteU16LE(u8 *buf, u16 value)
{
    buf[0] = (u8)(value & 0xFFU);
    buf[1] = (u8)(value >> 8);
}

static void ShipProtocol_MotorControl(u8 leftRight, u8 frontBack)
{
    u8 abs_leftRight;
    u8 abs_frontBack;
    int16 pwm;

    abs_leftRight = (u8)abs((int16)leftRight - 100);
    abs_frontBack = (u8)(abs((int16)frontBack - 100) + 5);

    if ((abs_frontBack > abs_leftRight) &&
        ((abs_leftRight > 10U) || (abs_frontBack > 20U))) {
        if (frontBack > 110U) {
            g_now_pwm_accelerator = (u8)(frontBack - 100U);
            pwm = (int16)(frontBack - 100U) * 10;
            Motor_SetBothSpeed(pwm, pwm);
        } else if (frontBack < 90U) {
            g_now_pwm_accelerator = 0U;
            pwm = (int16)(100U - frontBack) * 10;
            Motor_SetBothSpeed((int16)-pwm, (int16)-pwm);
        } else {
            g_now_pwm_accelerator = 0U;
        }
    } else if ((abs_frontBack < abs_leftRight) &&
               ((abs_leftRight > 10U) || (abs_frontBack > 20U))) {
        g_now_pwm_accelerator = 0U;
        if (leftRight <= 90U) {
            pwm = (int16)(100U - leftRight) * 10;
            Motor_SetBothSpeed((int16)-pwm, pwm);
        } else if (leftRight > 110U) {
            pwm = (int16)(leftRight - 100U) * 10;
            Motor_SetBothSpeed(pwm, (int16)-pwm);
        }
    } else {
        g_now_pwm_accelerator = 0U;
        AutoDrive_StopMotion();
    }
}

static void ShipProtocol_KeyControl(u8 accelerator_frontBack, u8 keyCode)
{
    static u8 lastKeyCode = 0U;

    if (lastKeyCode == keyCode) {
        return;
    }
    lastKeyCode = keyCode;

    switch (lastKeyCode) {
    case KEY_E_FLAG:
        if (accelerator_frontBack < 110U) {
            g_now_pwm_accelerator = 0U;
            AutoDrive_StopMotion();
        }
        AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
        break;
    default:
        break;
    }
}

static void ShipProtocol_HandleThrottle(const u8 *payload, u8 payload_len)
{
    if (payload_len < 3U) {
        return;
    }

    if (!AutoDrive_IsBusy()) {
        ShipProtocol_MotorControl(payload[0], payload[1]);
    }
    ShipProtocol_KeyControl(payload[1], payload[2]);
    AutoDrive_LinkAliveKick();
}

static void ShipProtocol_LowPowerCheck(void)
{
    u32 now_ms;

    now_ms = Task_GetTickMs();
    if ((now_ms - g_last_lowpower_tick_ms) < 10U) {
        return;
    }
    g_last_lowpower_tick_ms = now_ms;

    Power_Check_Handle();

    g_lowpower_check_times++;
    if (g_lowpower_check_times > SHIP_LOWPOWER_CHECK_TICKS) {
        g_lowpower_check_times = 0U;
        if (Power_ADC_Get_Level() == POWER_LEVEL_0) {
            if ((AutoDrive_GetMode() == AUTO_DRIVE_CLOSE) &&
                (g_now_pwm_accelerator < 10U)) {
                AutoDrive_TriggerReturn();
            }
        }
    } else {
        if (Power_ADC_Get_Level() > POWER_LEVEL_0) {
            g_lowpower_check_times = 0U;
        }
    }
}

static void ShipProtocol_SendGpsReport(void)
{
    const GPS_State_t *gps;
    AutoDrive_PointRaw_t point;
    u8 payload[22];
    u8 index;
    u8 frame[32];
    u8 frame_len;

    gps = GPS_GetState();
    if (gps == 0) {
        return;
    }

    index = 0U;
    payload[index++] = gps->satellites_used;

    ShipProtocol_WriteU16LE(&payload[index], (u16)gps->course_deg_x100);
    index += 2U;

    AutoDrive_GetCurrentPointRaw(&point);
    payload[index++] = point.lon_ew;
    ShipProtocol_WriteU16LE(&payload[index], point.lon_whole);
    index += 2U;
    ShipProtocol_WriteU16LE(&payload[index], point.lon_frac);
    index += 2U;
    payload[index++] = point.lat_ns;
    ShipProtocol_WriteU16LE(&payload[index], point.lat_whole);
    index += 2U;
    ShipProtocol_WriteU16LE(&payload[index], point.lat_frac);
    index += 2U;

    payload[index++] = Power_ADC_Get_Level();
    payload[index++] = AutoDrive_InActive();

    frame_len = 0U;
    frame[frame_len++] = SHIP_PROTO_HEAD;
    frame[frame_len++] = (u8)(2U + index);
    frame[frame_len++] = SHIP_CMD_GPS_REPORT;
    memcpy(&frame[frame_len], payload, index);
    frame_len = (u8)(frame_len + index);
    frame[frame_len++] = ShipProtocol_Xor(&frame[1], (u8)(2U + index));
    frame[frame_len++] = SHIP_PROTO_TAIL;
    (void)Wireless_Send(frame, frame_len);
}

static void ShipProtocol_Dispatch(u8 cmd, const u8 *payload, u8 payload_len)
{
    switch (cmd) {
    case SHIP_CMD_PAIR:
        break;
    case SHIP_CMD_THROTTLE:
        ShipProtocol_HandleThrottle(payload, payload_len);
        break;
    case SHIP_CMD_RETURN_HOME:
        if (payload_len >= sizeof(AutoDrive_PointRaw_t)) {
            AutoDrive_SetReturnPositionRaw(payload);
        }
        break;
    case SHIP_CMD_GOTO_POINT:
        if (payload_len >= sizeof(AutoDrive_PointRaw_t)) {
            AutoDrive_SetFishPositionRaw(payload);
        }
        break;
    case SHIP_CMD_RETURN_SWITCH:
        if (payload_len >= 1U) {
            AutoDrive_SetSwitchRaw(payload, payload_len);
        }
        break;
    default:
        LOGW(SHIP_TAG, "unknown cmd=0x%02X len=%u", (u16)cmd, (u16)payload_len);
        break;
    }
}

s8 ShipProtocol_ParseFrame(const u8 *frame, u8 frame_len)
{
    u8 body_len;
    u8 cmd;
    u8 data_len;
    u8 xor_calc;
    u8 xor_recv;

    if ((frame == 0) || (frame_len < 5U)) {
        return WIRELESS_ERR_PARAM;
    }
    if ((frame[0] != SHIP_PROTO_HEAD) || (frame[frame_len - 1U] != SHIP_PROTO_TAIL)) {
        return WIRELESS_ERR_VERIFY;
    }

    body_len = frame[1];
    if ((body_len < 2U) || ((u8)(body_len + 3U) != frame_len)) {
        return WIRELESS_ERR_VERIFY;
    }

    xor_recv = frame[frame_len - 2U];
    xor_calc = ShipProtocol_Xor(&frame[1], body_len);
    if (xor_recv != xor_calc) {
        return WIRELESS_ERR_VERIFY;
    }

    cmd = frame[2];
    data_len = (u8)(body_len - 2U);
    ShipProtocol_Dispatch(cmd, &frame[3], data_len);
    ShipProtocol_SendGpsReport();
    return SUCCESS;
}

void ShipProtocol_Poll(void)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 frame_len;
    s8 rc;

    AutoDrive_LinkAliveTick();
    ShipProtocol_LowPowerCheck();

    do {
        frame_len = 0U;
        rc = Wireless_Receive(frame, SHIP_PROTO_MAX_FRAME_LEN, &frame_len);
        if (rc == SUCCESS) {
            (void)ShipProtocol_ParseFrame(frame, frame_len);
        }
    } while (rc == SUCCESS);
}
