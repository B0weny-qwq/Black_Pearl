/**
 * @file    ship_protocol.c
 * @brief   船端旧遥控器无线业务协议移植实现。
 * @author  boweny
 * @date    2026-05-07
 * @version v1.2
 *
 * @details
 * 本文件保持旧版 `Wireless/wirelessProtocal.c` 的配对、收包解析、
 * 固定 `0x12` 回传和 `0x11` 手动链路语义。今晚联调目标仅保留
 * 手动开环控船、GPS 状态回传和磁力计可观测；IMU/AHRS/自动驾驶
 * 全部不参与控制。
 */
#include "ship_protocol.h"
#include "wireless.h"
#include "..\..\Device\GPS\GPS.h"
#include "..\..\Device\Motor\Motor.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\..\User\Task.h"
#include "..\..\..\Driver\inc\STC32G_ADC.h"

#if defined(SHIP_PAIR_SEED_USE_CHIPID) && (SHIP_PAIR_SEED_USE_CHIPID != 0)
#include "..\..\..\User\STC32G.h"
#endif

#define SHIP_TAG "SHIP"
#define SHIP_PAIR_FIX_REV "pairbiz-r4"

#define SHIP_LEGACY_PROTO_MAX_LEN      30U
#define SHIP_AXIS_CENTER               100U
#define SHIP_LR_DEAD_LOW               90U
#define SHIP_LR_DEAD_HIGH              110U
#define SHIP_FB_DEAD_LOW               90U
#define SHIP_FB_DEAD_HIGH              110U
#define SHIP_TURN_COMPARE_BIAS         5U
#define SHIP_PULSE_DURATION_MS         150U
#define SHIP_PULSE_SPEED               700
#define SHIP_THROTTLE_RECOVER_MS       3000UL
#define SHIP_REPEAT_LOG_MS             500UL

#define SHIP_KEY_E_RESERVED            0xA1U
#define SHIP_KEY_A_TOGGLE_LIGHT        0xA3U
#define SHIP_KEY_B_UNUSED              0xA5U
#define SHIP_KEY_C_PULSE_FORWARD       0xA7U
#define SHIP_KEY_D_PULSE_BACKWARD      0xA9U
#define SHIP_KEY_NULL                  0xA0U

typedef enum
{
    SHIP_STATE_BOOT_WAIT = 0,
    SHIP_STATE_PAIR_SEND,
    SHIP_STATE_WORK_RX
} ShipState_t;

typedef enum
{
    SHIP_MOTION_STOP = 0,
    SHIP_MOTION_FORWARD,
    SHIP_MOTION_BACKWARD,
    SHIP_MOTION_LEFT,
    SHIP_MOTION_RIGHT
} ShipMotion_t;

typedef struct
{
    u8 lr;
    u8 ud;
    u8 key;
    u8 last_key;
    u8 valid;
    u8 paired;
    u8 work_rx_configured;
    u8 work_state_logged;
    u8 light_toggle_pending;
    ShipState_t state;
    u8 rf_channel[3];
    u8 rf_send_key[2];
    u16 pair_wait_rsp_time;
    u16 wait_ticks;
    u16 pair_left;
    u16 pair_retry_count;
    u16 work_rx_reopen_ticks;
    u16 work_rx_reopen_total;
    u32 pair_wait_start_ms;
    u32 last_proto_rx_ms;
    u32 last_throttle_rx_ms;
    u8 pair_rsp_timeout_logged;
    u8 rx_idle_warned;
    u8 throttle_online;
    u8 throttle_recover_done;
    u8 motor_initialized;
    ShipMotion_t motion;
    ShipMotion_t pulse_motion;
    u32 pulse_expire_ms;
    u8 pulse_active;
} ShipRuntime_t;

typedef struct
{
    u16 raw;
    u16 adc_mv;
    u32 bat_mv;
    u8 report;
    u8 valid;
} ShipPowerSample_t;

static ShipRuntime_t g_ship_rt;

static s8 ShipProtocol_ApplyWorkSyncIdle(u8 log_rxdbg);
static s8 ShipProtocol_ApplyWorkRx(u8 log_rxdbg);
static void ShipProtocol_ReopenWorkRx(const char *reason, u8 log_rxdbg, u8 log_ok);
static const char *ShipProtocol_CmdName(u8 cmd);
static const char *ShipProtocol_KeyName(u8 key);
static const char *ShipProtocol_MotionName(ShipMotion_t motion);
static u8 ShipProtocol_ShouldLogManualSample(u8 left_right, u8 front_back, u8 key, u32 now_ms);
static void ShipProtocol_LogManualDecision(u8 left_right, u8 front_back, u8 key, ShipMotion_t target_motion, int16 speed, u8 force_log);
static void ShipProtocol_LogRxDebug(const u8 *stage);
static void ShipProtocol_LogPayloadBrief(const u8 *stage, u8 cmd, const u8 *payload, u8 payload_len);
static void ShipProtocol_LogFrameBrief(const u8 *stage, u8 channel, const u8 *frame, u8 frame_len);

static u8 ShipProtocol_AbsDelta(u8 value)
{
    if (value >= SHIP_AXIS_CENTER) {
        return (u8)(value - SHIP_AXIS_CENTER);
    }
    return (u8)(SHIP_AXIS_CENTER - value);
}

static void ShipProtocol_EnsureMotorInit(void)
{
#if SHIP_THROTTLE_PWM_ENABLE
    if (g_ship_rt.motor_initialized == 0U) {
        Motor_Init();
        g_ship_rt.motor_initialized = 1U;
        LOGI(SHIP_TAG, "motor pwm init");
    }
#else
    if (g_ship_rt.motor_initialized == 0U) {
        g_ship_rt.motor_initialized = 1U;
        LOGI(SHIP_TAG, "pwm disabled by SHIP_THROTTLE_PWM_ENABLE=0");
    }
#endif
}

static int16 ShipProtocol_LimitSpeed(int16 speed)
{
    if (speed > MOTOR_SPEED_MAX) {
        return MOTOR_SPEED_MAX;
    }
    if (speed < -MOTOR_SPEED_MAX) {
        return -MOTOR_SPEED_MAX;
    }
    return speed;
}

static int16 ShipProtocol_ClampAxisSpeed(u8 value)
{
    int16 speed;

    if (value > SHIP_AXIS_CENTER) {
        speed = (int16)(value - SHIP_AXIS_CENTER) * 10;
    } else {
        speed = (int16)(SHIP_AXIS_CENTER - value) * 10;
    }
    return ShipProtocol_LimitSpeed(speed);
}

static const char *ShipProtocol_CmdName(u8 cmd)
{
    switch (cmd) {
    case SHIP_CMD_PAIR_RSP:
        return "pair-rsp";
    case SHIP_CMD_PAIR:
        return "pair-req";
    case SHIP_CMD_THROTTLE:
        return "manual-ctrl";
    case SHIP_CMD_GPS_REPORT:
        return "gps-report";
    case SHIP_CMD_RETURN_HOME:
        return "return-home";
    case SHIP_CMD_GOTO_POINT:
        return "goto-point";
    case SHIP_CMD_RETURN_SWITCH:
        return "return-switch";
    default:
        return "unknown";
    }
}

static const char *ShipProtocol_KeyName(u8 key)
{
    switch (key) {
    case SHIP_KEY_E_RESERVED:
        return "E";
    case SHIP_KEY_A_TOGGLE_LIGHT:
        return "A";
    case SHIP_KEY_B_UNUSED:
        return "B";
    case SHIP_KEY_C_PULSE_FORWARD:
        return "C";
    case SHIP_KEY_D_PULSE_BACKWARD:
        return "D";
    case SHIP_KEY_NULL:
        return "NONE";
    default:
        return "UNKNOWN";
    }
}

static const char *ShipProtocol_MotionName(ShipMotion_t motion)
{
    switch (motion) {
    case SHIP_MOTION_FORWARD:
        return "forward";
    case SHIP_MOTION_BACKWARD:
        return "backward";
    case SHIP_MOTION_LEFT:
        return "left";
    case SHIP_MOTION_RIGHT:
        return "right";
    default:
        return "stop";
    }
}

static u8 ShipProtocol_ShouldLogManualSample(u8 left_right, u8 front_back, u8 key, u32 now_ms)
{
    static u8 last_left_right = SHIP_AXIS_CENTER;
    static u8 last_front_back = SHIP_AXIS_CENTER;
    static u8 last_key = SHIP_KEY_NULL;
    static u32 last_log_ms = 0UL;

    if ((left_right != last_left_right) ||
        (front_back != last_front_back) ||
        (key != last_key) ||
        ((now_ms - last_log_ms) >= SHIP_REPEAT_LOG_MS)) {
        last_left_right = left_right;
        last_front_back = front_back;
        last_key = key;
        last_log_ms = now_ms;
        return 1U;
    }

    return 0U;
}

static void ShipProtocol_LogManualDecision(u8 left_right, u8 front_back, u8 key, ShipMotion_t target_motion, int16 speed, u8 force_log)
{
    if (force_log == 0U) {
        return;
    }

    LOGI(SHIP_TAG,
         "manual parse cmd=0x11 throttle_raw=%u steering_raw=%u throttle_val=%d steering_val=%d key=0x%02X(%s) target=%s speed=%d",
         (u16)front_back,
         (u16)left_right,
         (int16)front_back - (int16)SHIP_AXIS_CENTER,
         (int16)left_right - (int16)SHIP_AXIS_CENTER,
         (u16)key,
         ShipProtocol_KeyName(key),
         ShipProtocol_MotionName(target_motion),
         speed);
}

static void ShipProtocol_LogMotion(ShipMotion_t motion, int16 left_speed, int16 right_speed)
{
    LOGI(SHIP_TAG, "manual motion=%s left=%d right=%d",
         ShipProtocol_MotionName(motion),
         left_speed,
         right_speed);
}

static void ShipProtocol_ApplyMotion(ShipMotion_t motion, int16 speed, u8 force_log)
{
    int16 left_speed;
    int16 right_speed;
    ShipMotion_t prev_motion;

    ShipProtocol_EnsureMotorInit();
    speed = ShipProtocol_LimitSpeed(speed);
    left_speed = 0;
    right_speed = 0;
    prev_motion = g_ship_rt.motion;

    switch (motion) {
    case SHIP_MOTION_FORWARD:
        left_speed = speed;
        right_speed = -speed;
        break;
    case SHIP_MOTION_BACKWARD:
        left_speed = -speed;
        right_speed = speed;
        break;
    case SHIP_MOTION_LEFT:
        left_speed = -speed;
        right_speed = -speed;
        break;
    case SHIP_MOTION_RIGHT:
        left_speed = speed;
        right_speed = speed;
        break;
    default:
        break;
    }

#if SHIP_THROTTLE_PWM_ENABLE
    if (motion == SHIP_MOTION_STOP) {
        Motor_StopAll();
    } else {
        Motor_SetBothSpeed(left_speed, right_speed);
    }
#endif

    g_ship_rt.motion = motion;
    if ((prev_motion != motion) || (force_log != 0U)) {
        ShipProtocol_LogMotion(motion, left_speed, right_speed);
    }
}

static void ShipProtocol_StopMotion(const u8 *reason, u8 force_log)
{
    ShipMotion_t prev_motion;

    ShipProtocol_EnsureMotorInit();
    prev_motion = g_ship_rt.motion;
#if SHIP_THROTTLE_PWM_ENABLE
    Motor_StopAll();
#endif
    g_ship_rt.motion = SHIP_MOTION_STOP;
    g_ship_rt.pulse_active = 0U;
    g_ship_rt.pulse_motion = SHIP_MOTION_STOP;
    g_ship_rt.pulse_expire_ms = 0UL;
    if ((prev_motion != SHIP_MOTION_STOP) || (force_log != 0U)) {
        LOGI(SHIP_TAG, "manual motion=stop reason=%s", reason);
    }
}

static void ShipProtocol_StartPulse(ShipMotion_t motion)
{
    g_ship_rt.pulse_active = 1U;
    g_ship_rt.pulse_motion = motion;
    g_ship_rt.pulse_expire_ms = Task_GetTickMs() + SHIP_PULSE_DURATION_MS;
    ShipProtocol_ApplyMotion(motion, SHIP_PULSE_SPEED, 1U);
    LOGI(SHIP_TAG, "key pulse motion=%u dur=%ums", (u16)motion, (u16)SHIP_PULSE_DURATION_MS);
}

static void ShipProtocol_ServicePulse(u32 now_ms)
{
    if ((g_ship_rt.pulse_active != 0U) && ((int32)(now_ms - g_ship_rt.pulse_expire_ms) >= 0)) {
        g_ship_rt.pulse_active = 0U;
        g_ship_rt.pulse_motion = SHIP_MOTION_STOP;
        g_ship_rt.pulse_expire_ms = 0UL;
        ShipProtocol_StopMotion("pulse done", 1U);
    }
}

static void ShipProtocol_LogLightPending(void)
{
    if (g_ship_rt.light_toggle_pending == 0U) {
        g_ship_rt.light_toggle_pending = 1U;
        LOGW(SHIP_TAG, "key A received but ship light pin is not confirmed on v1.1");
    } else {
        LOGI(SHIP_TAG, "key A received again, still waiting for ship light pin confirmation");
    }
}

static void ShipProtocol_ApplyManualControl(u8 left_right, u8 front_back, u8 log_this_sample)
{
    u8 abs_left_right;
    u8 abs_front_back;
    int16 speed;
    ShipMotion_t target_motion;

    abs_left_right = ShipProtocol_AbsDelta(left_right);
    abs_front_back = (u8)(ShipProtocol_AbsDelta(front_back) + SHIP_TURN_COMPARE_BIAS);
    target_motion = SHIP_MOTION_STOP;
    speed = 0;

    if ((abs_front_back > abs_left_right) &&
        ((abs_left_right > 10U) || (abs_front_back > 20U))) {
        speed = ShipProtocol_ClampAxisSpeed(front_back);
        if (front_back > SHIP_FB_DEAD_HIGH) {
            target_motion = SHIP_MOTION_FORWARD;
            ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, target_motion, speed, log_this_sample);
            ShipProtocol_ApplyMotion(target_motion, speed, log_this_sample);
        } else if (front_back < SHIP_FB_DEAD_LOW) {
            target_motion = SHIP_MOTION_BACKWARD;
            ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, target_motion, speed, log_this_sample);
            ShipProtocol_ApplyMotion(target_motion, speed, log_this_sample);
        } else {
            ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, SHIP_MOTION_STOP, 0, log_this_sample);
            ShipProtocol_StopMotion("manual center", log_this_sample);
        }
        return;
    }

    if ((abs_front_back < abs_left_right) &&
        ((abs_left_right > 10U) || (abs_front_back > 20U))) {
        speed = ShipProtocol_ClampAxisSpeed(left_right);
        if (left_right <= SHIP_LR_DEAD_LOW) {
            target_motion = SHIP_MOTION_LEFT;
            ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, target_motion, speed, log_this_sample);
            ShipProtocol_ApplyMotion(target_motion, speed, log_this_sample);
        } else if (left_right > SHIP_LR_DEAD_HIGH) {
            target_motion = SHIP_MOTION_RIGHT;
            ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, target_motion, speed, log_this_sample);
            ShipProtocol_ApplyMotion(target_motion, speed, log_this_sample);
        } else {
            ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, SHIP_MOTION_STOP, 0, log_this_sample);
            ShipProtocol_StopMotion("manual center", log_this_sample);
        }
        return;
    }

    ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key, SHIP_MOTION_STOP, 0, log_this_sample);
    ShipProtocol_StopMotion("manual center", log_this_sample);
}

static void ShipProtocol_HandleKey(u8 front_back, u8 key)
{
    if (key == g_ship_rt.last_key) {
        return;
    }
    g_ship_rt.last_key = key;

    switch (key) {
    case SHIP_KEY_A_TOGGLE_LIGHT:
        (void)front_back;
        ShipProtocol_LogLightPending();
        break;
    case SHIP_KEY_B_UNUSED:
        LOGI(SHIP_TAG, "key B ignored to keep legacy no-op behavior");
        break;
    case SHIP_KEY_C_PULSE_FORWARD:
        ShipProtocol_StartPulse(SHIP_MOTION_FORWARD);
        break;
    case SHIP_KEY_D_PULSE_BACKWARD:
        ShipProtocol_StartPulse(SHIP_MOTION_BACKWARD);
        break;
    case SHIP_KEY_E_RESERVED:
        LOGI(SHIP_TAG, "key E reserved, autodrive side effects disabled tonight");
        break;
    case SHIP_KEY_NULL:
    default:
        break;
    }
}

static u16 ShipProtocol_AdcRawToMv(u16 adc_raw)
{
    return (u16)(((u32)adc_raw * (u32)SHIP_ADC_REF_MV) / 4095UL);
}

static u32 ShipProtocol_AdcMvToBatteryMv(u16 adc_mv)
{
    if (SHIP_BAT_DIV_DEN == 0UL) {
        return (u32)adc_mv;
    }
    return (((u32)adc_mv * (u32)SHIP_BAT_DIV_NUM) / (u32)SHIP_BAT_DIV_DEN);
}

static void ShipProtocol_ReadPowerSample(ShipPowerSample_t *sample)
{
    u16 adc_raw;

    if (sample == 0) {
        return;
    }

    adc_raw = Get_ADCResult(ADC_CH8);
    if (adc_raw > 4095U) {
        sample->raw = adc_raw;
        sample->adc_mv = 0U;
        sample->bat_mv = 0UL;
        sample->report = 0xFFU;
        sample->valid = 0U;
        return;
    }

    sample->raw = adc_raw;
    sample->adc_mv = ShipProtocol_AdcRawToMv(adc_raw);
    sample->bat_mv = ShipProtocol_AdcMvToBatteryMv(sample->adc_mv);
    sample->report = (u8)(adc_raw >> 4);
    sample->valid = 1U;
}

static void ShipProtocol_LogPowerSample(const ShipPowerSample_t *sample, u8 force_log)
{
#if SHIP_ADC_LOG_ENABLE
    if (force_log == 0U) {
        return;
    }
    if (sample == 0) {
        return;
    }
    if (sample->valid == 0U) {
        LOGW(SHIP_TAG, "adc p0.0 read fail raw=%u power=0x%02X",
             (u16)sample->raw,
             (u16)sample->report);
        return;
    }

    LOGI(SHIP_TAG, "adc p0.0 raw=%u adc_mv=%u bat_mv=%lu power=0x%02X",
         (u16)sample->raw,
         (u16)sample->adc_mv,
         (u32)sample->bat_mv,
         (u16)sample->report);
#else
    (void)force_log;
    (void)sample;
#endif
}

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

static void ShipProtocol_GetPairSeed(u8 *seed)
{
    if (seed == 0) {
        return;
    }

#if defined(SHIP_PAIR_SEED_USE_CHIPID) && (SHIP_PAIR_SEED_USE_CHIPID != 0)
    seed[0] = CHIPID20;
    seed[1] = CHIPID21;
    seed[2] = CHIPID22;
    seed[3] = CHIPID23;
#else
    seed[0] = SHIP_PAIR_SEED0;
    seed[1] = SHIP_PAIR_SEED1;
    seed[2] = SHIP_PAIR_SEED2;
    seed[3] = SHIP_PAIR_SEED3;
#endif
}

static u16 ShipProtocol_ReadU16BE(const u8 *buf)
{
    return (u16)(((u16)buf[0] << 8) | buf[1]);
}

static u16 ShipProtocol_ToCoord1(u32 abs_deg1e7)
{
    return (u16)(abs_deg1e7 / 10000000UL);
}

static u16 ShipProtocol_ToCoord2(u32 abs_deg1e7)
{
    return (u16)(((abs_deg1e7 % 10000000UL) * 10000UL) / 10000000UL);
}

static void ShipProtocol_WriteU16LE(u8 *dst, u16 value)
{
    dst[0] = (u8)(value & 0xFFU);
    dst[1] = (u8)(value >> 8);
}

static void ShipProtocol_LogCoordBE(const u8 *buf, u8 len)
{
    if (len < 10U) {
        LOGW(SHIP_TAG, "coord short len=%u", (u16)len);
        return;
    }

    LOGI(SHIP_TAG,
         "coord lonEW=0x%02X lon=%u.%u latNS=0x%02X lat=%u.%u",
         (u16)buf[0],
         ShipProtocol_ReadU16BE(&buf[1]),
         ShipProtocol_ReadU16BE(&buf[3]),
         (u16)buf[5],
         ShipProtocol_ReadU16BE(&buf[6]),
         ShipProtocol_ReadU16BE(&buf[8]));
}

static s8 ShipProtocol_SendFrame(u8 channel, u8 cmd, const u8 *payload, u8 payload_len, u8 log_frame)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 idx;
    u8 body_len;
    u8 i;

    if (payload_len > (SHIP_PROTO_MAX_FRAME_LEN - 5U)) {
        return WIRELESS_ERR_PARAM;
    }

    body_len = (u8)(2U + payload_len);
    idx = 0U;
    frame[idx++] = SHIP_PROTO_HEAD;
    frame[idx++] = body_len;
    frame[idx++] = cmd;

    for (i = 0U; i < payload_len; i++) {
        frame[idx++] = payload[i];
    }

    frame[idx++] = ShipProtocol_Xor(&frame[1], body_len);
    frame[idx++] = SHIP_PROTO_TAIL;
    if (log_frame != 0U) {
        ShipProtocol_LogFrameBrief("tx send", channel, frame, idx);
    }

    return Wireless_SendOnChannel(channel, frame, idx);
}

static void ShipProtocol_ApplyDefaultRf(void)
{
    u8 seed[4];
    u8 channel;

    ShipProtocol_GetPairSeed(seed);

    g_ship_rt.rf_send_key[0] =
        (u8)(((u8)((u8)(seed[0] << 4) >> 4)) + ((u8)(seed[3] >> 2) + (u8)(seed[3] % 0x03U)));
    g_ship_rt.rf_send_key[1] =
        (u8)(((u8)((u8)(seed[1] << 4) >> 4)) + ((u8)(seed[2] >> 3) + (u8)(seed[0] % 0x06U)));

    channel = (u8)(((u8)(((seed[3] + 0x06U) % 0x40U) +
                         ((seed[2] >> 3) * 0x08U) +
                         (((seed[1] | seed[0]) % 0x08U) / 2U))) % 0x40U);
    g_ship_rt.rf_channel[0] = channel;
    g_ship_rt.rf_channel[1] = channel;
    g_ship_rt.rf_channel[2] = (u8)(channel + 0x40U);
}

static s8 ShipProtocol_ApplyWorkSyncIdle(u8 log_rxdbg)
{
    u16 reg36;
    u16 reg39;
    s8 rc;

    reg36 = (u16)(((u16)g_ship_rt.rf_send_key[0] << 8) | g_ship_rt.rf_send_key[0]);
    reg39 = (u16)(((u16)g_ship_rt.rf_send_key[1] << 8) | g_ship_rt.rf_send_key[1]);

    rc = Wireless_SetSyncRegsIdle(reg36, reg39);
    if (rc != SUCCESS) {
        return rc;
    }

    g_ship_rt.work_rx_configured = 0U;
    g_ship_rt.work_rx_reopen_ticks = 0U;
    if (log_rxdbg != 0U) {
        ShipProtocol_LogRxDebug("pair-sync-idle");
    }

    return SUCCESS;
}

static s8 ShipProtocol_ApplyWorkRx(u8 log_rxdbg)
{
    s8 rc;

    rc = Wireless_SetChannel(g_ship_rt.rf_channel[0]);
    if (rc == SUCCESS) {
        g_ship_rt.work_rx_configured = 1U;
        g_ship_rt.work_rx_reopen_ticks = 0U;
        if (log_rxdbg != 0U) {
            ShipProtocol_LogRxDebug("work-rx");
        }
    }
    return rc;
}

static void ShipProtocol_ReopenWorkRx(const char *reason, u8 log_rxdbg, u8 log_ok)
{
    s8 rc;

    rc = ShipProtocol_ApplyWorkRx(log_rxdbg);
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "work-rx reopen fail reason=%s rc=%d", reason, rc);
        return;
    }

    if (log_ok != 0U) {
        LOGI(SHIP_TAG, "work-rx reopen reason=%s ch=%u",
             reason,
             (u16)g_ship_rt.rf_channel[0]);
    }
}

static void ShipProtocol_LogRxDebug(const u8 *stage)
{
    Wireless_RxDebug_t dbg;
    s8 rc;

    rc = Wireless_GetRxDebug(&dbg);
    if (rc != SUCCESS) {
        LOGW(SHIP_TAG, "rxdbg %s read fail rc=%d", stage, rc);
        return;
    }

    LOGI(SHIP_TAG,
         "rxdbg %s ch=%u reg7=0x%04X reg8=0x%04X reg36=0x%04X reg37=0x%04X reg38=0x%04X reg39=0x%04X reg48=0x%04X reg52=0x%04X rssi=%u rxbit=%u mode=%u rxen=%u txen=%u",
         stage,
         (u16)dbg.channel,
         dbg.reg7,
         dbg.reg8,
         dbg.reg36,
         dbg.reg37,
         dbg.reg38,
         dbg.reg39,
         dbg.reg48,
         dbg.reg52,
         (u16)dbg.rssi,
         (u16)dbg.rx_mode_bit,
         (u16)dbg.mode,
         (u16)dbg.rx_en,
         (u16)dbg.tx_en);
}

static void ShipProtocol_LogPayloadBrief(const u8 *stage, u8 cmd, const u8 *payload, u8 payload_len)
{
    LOGI(SHIP_TAG,
         "%s cmd=0x%02X(%s) len=%u data=%02X %02X %02X %02X %02X %02X",
         stage,
         (u16)cmd,
         ShipProtocol_CmdName(cmd),
         (u16)payload_len,
         (u16)((payload_len > 0U) ? payload[0] : 0U),
         (u16)((payload_len > 1U) ? payload[1] : 0U),
         (u16)((payload_len > 2U) ? payload[2] : 0U),
         (u16)((payload_len > 3U) ? payload[3] : 0U),
         (u16)((payload_len > 4U) ? payload[4] : 0U),
         (u16)((payload_len > 5U) ? payload[5] : 0U));
}

static void ShipProtocol_LogFrameBrief(const u8 *stage, u8 channel, const u8 *frame, u8 frame_len)
{
    if ((frame == 0) || (frame_len < 3U)) {
        return;
    }

    LOGI(SHIP_TAG,
         "%s ch=%u frame_len=%u cmd=0x%02X(%s) raw=%02X %02X %02X %02X %02X %02X %02X %02X",
         stage,
         (u16)channel,
         (u16)frame_len,
         (u16)frame[2],
         ShipProtocol_CmdName(frame[2]),
         (u16)((frame_len > 0U) ? frame[0] : 0U),
         (u16)((frame_len > 1U) ? frame[1] : 0U),
         (u16)((frame_len > 2U) ? frame[2] : 0U),
         (u16)((frame_len > 3U) ? frame[3] : 0U),
         (u16)((frame_len > 4U) ? frame[4] : 0U),
         (u16)((frame_len > 5U) ? frame[5] : 0U),
         (u16)((frame_len > 6U) ? frame[6] : 0U),
         (u16)((frame_len > 7U) ? frame[7] : 0U));
}

static s8 ShipProtocol_TryPairSend(u16 left_after_send)
{
    u8 pair_data[4];
    u8 pair_xor;
    s8 rc;

    ShipProtocol_GetPairSeed(pair_data);
    pair_xor = (u8)(0x06U ^ SHIP_CMD_PAIR ^ pair_data[0] ^
                    pair_data[1] ^ pair_data[2] ^ pair_data[3]);

    rc = ShipProtocol_SendFrame(SHIP_PAIR_CHANNEL_DEFAULT, SHIP_CMD_PAIR, pair_data, 4U, 1U);
    if (rc == SUCCESS) {
        LOGI(SHIP_TAG,
             "pair req sent seq=%u/%u retry=%u ch=0x%02X",
             (u16)(SHIP_PAIR_SEND_TIMES - left_after_send),
             (u16)SHIP_PAIR_SEND_TIMES,
             (u16)g_ship_rt.pair_retry_count,
             (u16)SHIP_PAIR_CHANNEL_DEFAULT);
        if (left_after_send == (SHIP_PAIR_SEND_TIMES - 1U)) {
            LOGI(SHIP_TAG,
                 "pair req start retry=%u pair_ch=0x%02X seed=%02X%02X%02X%02X work_rx=%u key=%u/%u",
                 (u16)g_ship_rt.pair_retry_count,
                 (u16)SHIP_PAIR_CHANNEL_DEFAULT,
                 (u16)pair_data[0], (u16)pair_data[1],
                 (u16)pair_data[2], (u16)pair_data[3],
                 (u16)g_ship_rt.rf_channel[0],
                 (u16)g_ship_rt.rf_send_key[0],
                 (u16)g_ship_rt.rf_send_key[1]);
            LOGI(SHIP_TAG,
                 "pair req frame=AA 06 10 %02X %02X %02X %02X %02X BB",
                 (u16)pair_data[0],
                 (u16)pair_data[1],
                 (u16)pair_data[2],
                 (u16)pair_data[3],
                 (u16)pair_xor);
        } else if (left_after_send == 0U) {
            LOGI(SHIP_TAG, "pair req burst done, wait rsp");
        }
    } else {
        LOGE(SHIP_TAG, "pair req tx fail rc=%d", rc);
    }

    return rc;
}

static void ShipProtocol_SendGpsOnce(u8 log_this_tx)
{
    u8 payload[15];
    ShipPowerSample_t power;
    const GPS_State_t *gps;
    u8 idx;
    u16 angle;
    u32 abs_lon;
    u32 abs_lat;
    u16 coord1;
    u16 coord2;
    s8 rc;
    char lon_dir;
    char lat_dir;

    gps = GPS_GetState();
    idx = 0U;

    payload[idx++] = gps->satellites_used;

    angle = gps->course_deg_x100;
    payload[idx++] = (u8)(angle & 0xFFU);
    payload[idx++] = (u8)(angle >> 8);

    if (gps->lon_deg1e7 < 0) {
        lon_dir = 'W';
        payload[idx++] = (u8)lon_dir;
        abs_lon = (u32)(-gps->lon_deg1e7);
    } else {
        lon_dir = 'E';
        payload[idx++] = (u8)lon_dir;
        abs_lon = (u32)gps->lon_deg1e7;
    }
    coord1 = ShipProtocol_ToCoord1(abs_lon);
    coord2 = ShipProtocol_ToCoord2(abs_lon);
    ShipProtocol_WriteU16LE(&payload[idx], coord1);
    idx += 2U;
    ShipProtocol_WriteU16LE(&payload[idx], coord2);
    idx += 2U;

    if (gps->lat_deg1e7 < 0) {
        lat_dir = 'S';
        payload[idx++] = (u8)lat_dir;
        abs_lat = (u32)(-gps->lat_deg1e7);
    } else {
        lat_dir = 'N';
        payload[idx++] = (u8)lat_dir;
        abs_lat = (u32)gps->lat_deg1e7;
    }
    coord1 = ShipProtocol_ToCoord1(abs_lat);
    coord2 = ShipProtocol_ToCoord2(abs_lat);
    ShipProtocol_WriteU16LE(&payload[idx], coord1);
    idx += 2U;
    ShipProtocol_WriteU16LE(&payload[idx], coord2);
    idx += 2U;

    ShipProtocol_ReadPowerSample(&power);
    payload[idx++] = power.report;
    payload[idx++] = 0U;

    if (idx != 15U) {
        LOGE(SHIP_TAG, "gps payload len bad=%u", (u16)idx);
        return;
    }

    ShipProtocol_LogPowerSample(&power, log_this_tx);
    if (log_this_tx != 0U) {
        LOGI(SHIP_TAG,
             "tx cmd=0x12 ch=%u payload_len=%u sat=%u angle=%u power=0x%02X auto=0x%02X",
             (u16)g_ship_rt.rf_channel[0],
             (u16)idx,
             (u16)payload[0],
             (u16)(((u16)payload[2] << 8) | payload[1]),
             (u16)payload[13],
             (u16)payload[14]);
        LOGI(SHIP_TAG,
             "gps state fix=%u sat=%u lon=%c%lu lat=%c%lu angle=%u power=0x%02X seq=%lu",
             (u16)gps->fix_valid,
             (u16)gps->satellites_used,
             lon_dir,
             (u32)abs_lon,
             lat_dir,
             (u32)abs_lat,
             (u16)gps->course_deg_x100,
             (u16)payload[13],
             (u32)gps->update_sequence);
        ShipProtocol_LogPayloadBrief("tx frame", SHIP_CMD_GPS_REPORT, payload, idx);
    }

    rc = ShipProtocol_SendFrame(g_ship_rt.rf_channel[0], SHIP_CMD_GPS_REPORT, payload, idx, log_this_tx);
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "gps tx fail rc=%d ch=0x%02X", rc, (u16)g_ship_rt.rf_channel[0]);
    }
    if (g_ship_rt.paired != 0U) {
        ShipProtocol_ReopenWorkRx("gps report tx", 0U, log_this_tx);
    } else {
        g_ship_rt.work_rx_configured = 0U;
    }
}

static void ShipProtocol_HandlePairRsp(const u8 *payload, u8 payload_len)
{
    if (g_ship_rt.pair_wait_rsp_time == 0U) {
        return;
    }

    ShipProtocol_LogPayloadBrief("pair rsp rx", SHIP_CMD_PAIR_RSP, payload, payload_len);
    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.paired = 1U;
    LOGI(SHIP_TAG, "pair ok, enter work channel rx_ch=%u tx_ch=%u",
         (u16)g_ship_rt.rf_channel[0],
         (u16)g_ship_rt.rf_channel[0]);

    if ((payload != 0) && (payload_len == 4U)) {
        LOGI(SHIP_TAG,
             "pair success paired=1 work_rx=%u work_tx=%u key=%u/%u rsp=%02X%02X%02X%02X",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)payload[0], (u16)payload[1],
             (u16)payload[2], (u16)payload[3]);
    } else {
        LOGI(SHIP_TAG,
             "pair success paired=1 work_rx=%u work_tx=%u key=%u/%u rsp_len=%u",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)payload_len);
    }
}

static u8 ShipProtocol_HandleThrottle(const u8 *payload, u8 payload_len)
{
    u32 now_ms;
    u8 log_this_sample;

    if (payload_len < 3U) {
        LOGW(SHIP_TAG, "throttle short len=%u", (u16)payload_len);
        return 1U;
    }

    g_ship_rt.lr = payload[0];
    g_ship_rt.ud = payload[1];
    g_ship_rt.key = payload[2];
    g_ship_rt.valid = 1U;
    now_ms = Task_GetTickMs();
    g_ship_rt.last_throttle_rx_ms = now_ms;
    g_ship_rt.last_proto_rx_ms = now_ms;
    g_ship_rt.rx_idle_warned = 0U;
    g_ship_rt.throttle_recover_done = 0U;
    log_this_sample = ShipProtocol_ShouldLogManualSample(payload[0], payload[1], payload[2], now_ms);
    if (g_ship_rt.throttle_online == 0U) {
        g_ship_rt.throttle_online = 1U;
        LOGI(SHIP_TAG, "remote link online by cmd=0x11");
    }

    if (log_this_sample != 0U) {
        LOGI(SHIP_TAG, "rc cmd=0x11 lr=%u ud=%u key=0x%02X(%s) paired=%u",
             (u16)g_ship_rt.lr,
             (u16)g_ship_rt.ud,
             (u16)g_ship_rt.key,
             ShipProtocol_KeyName(g_ship_rt.key),
             (u16)g_ship_rt.paired);
        LOGI(SHIP_TAG, "throttle_raw=%u steering_raw=%u throttle_val=%d steering_val=%d key=0x%02X(%s)",
             (u16)g_ship_rt.ud,
             (u16)g_ship_rt.lr,
             (int16)g_ship_rt.ud - (int16)SHIP_AXIS_CENTER,
             (int16)g_ship_rt.lr - (int16)SHIP_AXIS_CENTER,
             (u16)g_ship_rt.key,
             ShipProtocol_KeyName(g_ship_rt.key));
    }

    ShipProtocol_HandleKey(g_ship_rt.ud, g_ship_rt.key);
    if (g_ship_rt.pulse_active == 0U) {
        ShipProtocol_ApplyManualControl(g_ship_rt.lr, g_ship_rt.ud, log_this_sample);
    }
    return log_this_sample;
}

static void ShipProtocol_Dispatch(u8 cmd, const u8 *payload, u8 payload_len)
{
    u8 log_gps_after_rsp;

    log_gps_after_rsp = 1U;
    if (cmd != SHIP_CMD_THROTTLE) {
        LOGI(SHIP_TAG, "dispatch cmd=0x%02X(%s) payload_len=%u",
             (u16)cmd,
             ShipProtocol_CmdName(cmd),
             (u16)payload_len);
    }
    switch (cmd) {
    case SHIP_CMD_PAIR_RSP:
        ShipProtocol_HandlePairRsp(payload, payload_len);
        break;
    case SHIP_CMD_PAIR:
        break;
    case SHIP_CMD_THROTTLE:
        log_gps_after_rsp = ShipProtocol_HandleThrottle(payload, payload_len);
        break;
    case SHIP_CMD_GPS_REPORT:
        break;
    case SHIP_CMD_RETURN_HOME:
        LOGI(SHIP_TAG, "cmd=0x13 return-home compat-only len=%u", (u16)payload_len);
        ShipProtocol_LogCoordBE(payload, payload_len);
        break;
    case SHIP_CMD_GOTO_POINT:
        LOGI(SHIP_TAG, "cmd=0x14 goto-point compat-only len=%u", (u16)payload_len);
        ShipProtocol_LogCoordBE(payload, payload_len);
        break;
    case SHIP_CMD_RETURN_SWITCH:
        if (payload_len < 1U) {
            LOGW(SHIP_TAG, "return-switch short len=%u", (u16)payload_len);
            break;
        }
        LOGI(SHIP_TAG, "cmd=0x15 compat-only switch=0x%02X len=%u",
             (u16)payload[0], (u16)payload_len);
        if (payload_len > 1U) {
            ShipProtocol_LogCoordBE(&payload[1], (u8)(payload_len - 1U));
        }
        break;
    default:
        LOGW(SHIP_TAG, "unknown cmd=0x%02X len=%u", (u16)cmd, (u16)payload_len);
        break;
    }

    ShipProtocol_SendGpsOnce(log_gps_after_rsp);
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
        LOGW(SHIP_TAG, "bad frame edge h=0x%02X t=0x%02X len=%u",
             (u16)frame[0], (u16)frame[frame_len - 1U], (u16)frame_len);
        return WIRELESS_ERR_VERIFY;
    }

    body_len = frame[1];
    if ((body_len < 2U) || ((u8)(body_len + 3U) != frame_len)) {
        LOGW(SHIP_TAG, "bad len field=%u frame=%u", (u16)body_len, (u16)frame_len);
        return WIRELESS_ERR_VERIFY;
    }

    xor_recv = frame[frame_len - 2U];
    xor_calc = ShipProtocol_Xor(&frame[1], body_len);
    if (xor_recv != xor_calc) {
        LOGW(SHIP_TAG, "bad xor cmd=0x%02X calc=0x%02X recv=0x%02X",
             (u16)frame[2], (u16)xor_calc, (u16)xor_recv);
        return WIRELESS_ERR_VERIFY;
    }

    cmd = frame[2];
    data_len = (u8)(body_len - 2U);
    g_ship_rt.last_proto_rx_ms = Task_GetTickMs();
    g_ship_rt.rx_idle_warned = 0U;
    if (cmd != SHIP_CMD_THROTTLE) {
        ShipProtocol_LogPayloadBrief("rx frame ok", cmd, &frame[3], data_len);
    }
    ShipProtocol_Dispatch(cmd, &frame[3], data_len);
    return SUCCESS;
}

static void ShipProtocol_ReceiveHandle(const u8 *rx_buf, u8 len)
{
    u8 i;
    u8 frame[SHIP_LEGACY_PROTO_MAX_LEN];
    u8 frame_index;
    u8 frame_left;
    u8 frame_finish;
    u8 check_ok;
    u8 check_sum;

    if (rx_buf == 0) {
        return;
    }

    if (len > SHIP_LEGACY_PROTO_MAX_LEN) {
        len = 10U;
    }

    frame_index = 0U;
    frame_left = 0U;
    frame_finish = 0U;
    for (i = 0U; i < len; i++) {
        check_ok = 1U;
        switch (frame_index) {
        case 0U:
            if (rx_buf[i] != SHIP_PROTO_HEAD) {
                frame_index = 0U;
                frame_left = 0U;
                check_ok = 0U;
            }
            break;
        case 1U:
            frame_left = (u8)(rx_buf[i] + 1U);
            if (frame_left > (SHIP_LEGACY_PROTO_MAX_LEN - 2U)) {
                frame_index = 0U;
                frame_left = 0U;
                check_ok = 0U;
            }
            break;
        default:
            if (frame_left > 0U) {
                frame_left--;
                if (frame_left == 0U) {
                    frame_finish = 1U;
                }
            }
            break;
        }

        if (check_ok == 0U) {
            continue;
        }

        if (frame_index < SHIP_LEGACY_PROTO_MAX_LEN) {
            frame[frame_index++] = rx_buf[i];
        } else {
            frame_index = 0U;
            frame_left = 0U;
            frame_finish = 0U;
            continue;
        }

        if (frame_finish != 0U) {
            frame_finish = 0U;
            check_sum = ShipProtocol_Xor(&frame[1], (u8)(frame_index - 3U));
            if ((check_sum == frame[frame_index - 2U]) &&
                (frame[frame_index - 1U] == SHIP_PROTO_TAIL)) {
                (void)ShipProtocol_ParseFrame(frame, frame_index);
            }
            frame_index = 0U;
            frame_left = 0U;
        }
    }
}

void ShipProtocol_Poll(void)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 frame_len;
    s8 rc;

    do {
        frame_len = 0U;
        rc = Wireless_Receive(frame, SHIP_PROTO_MAX_FRAME_LEN, &frame_len);
        if (rc == SUCCESS) {
            ShipProtocol_ReceiveHandle(frame, frame_len);
        }
    } while (rc == SUCCESS);
}

static void ShipProtocol_PollRxFrames(void)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 frame_len;
    s8 rc;

    do {
        frame_len = 0U;
        rc = Wireless_Receive(frame, SHIP_PROTO_MAX_FRAME_LEN, &frame_len);
        if (rc == SUCCESS) {
            ShipProtocol_ReceiveHandle(frame, frame_len);
        }
    } while (rc == SUCCESS);
}

static void ShipProtocol_InitRuntime(void)
{
    u8 seed[4];

    ShipProtocol_ApplyDefaultRf();
    ShipProtocol_GetPairSeed(seed);

    g_ship_rt.lr = SHIP_AXIS_CENTER;
    g_ship_rt.ud = SHIP_AXIS_CENTER;
    g_ship_rt.key = SHIP_KEY_NULL;
    g_ship_rt.last_key = SHIP_KEY_NULL;
    g_ship_rt.valid = 0U;
    g_ship_rt.paired = 0U;
    g_ship_rt.work_rx_configured = 0U;
    g_ship_rt.work_state_logged = 0U;
    g_ship_rt.light_toggle_pending = 0U;
    g_ship_rt.state = SHIP_STATE_BOOT_WAIT;
    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
    g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
    g_ship_rt.pair_retry_count = 0U;
    g_ship_rt.work_rx_reopen_ticks = 0U;
    g_ship_rt.work_rx_reopen_total = 0U;
    g_ship_rt.pair_wait_start_ms = 0UL;
    g_ship_rt.last_proto_rx_ms = 0UL;
    g_ship_rt.last_throttle_rx_ms = 0UL;
    g_ship_rt.pair_rsp_timeout_logged = 0U;
    g_ship_rt.rx_idle_warned = 0U;
    g_ship_rt.throttle_online = 0U;
    g_ship_rt.throttle_recover_done = 0U;
    g_ship_rt.motor_initialized = 0U;
    g_ship_rt.motion = SHIP_MOTION_STOP;
    g_ship_rt.pulse_motion = SHIP_MOTION_STOP;
    g_ship_rt.pulse_expire_ms = 0UL;
    g_ship_rt.pulse_active = 0U;

    LOGI(SHIP_TAG,
         "scheduler init rev=%s wait=%u pair_send=%u pair_ch=0x%02X seed=%02X%02X%02X%02X",
         SHIP_PAIR_FIX_REV,
         (u16)g_ship_rt.wait_ticks,
         (u16)g_ship_rt.pair_left,
         (u16)SHIP_PAIR_CHANNEL_DEFAULT,
         (u16)seed[0], (u16)seed[1], (u16)seed[2], (u16)seed[3]);
}

static void ShipProtocol_StepPairSend(void)
{
    s8 rc;
    u16 left_after_send;

    if (g_ship_rt.pair_left > 0U) {
        g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
        left_after_send = (u16)(g_ship_rt.pair_left - 1U);
        rc = ShipProtocol_TryPairSend(left_after_send);
        if (rc != SUCCESS) {
            return;
        }
        g_ship_rt.pair_left = left_after_send;

        if (g_ship_rt.pair_left == 0U) {
            rc = ShipProtocol_ApplyWorkSyncIdle(1U);
            if (rc != SUCCESS) {
                LOGE(SHIP_TAG, "pair sync idle fail rc=%d", rc);
                g_ship_rt.pair_retry_count++;
                g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
                g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
                return;
            }
            g_ship_rt.pair_wait_rsp_time = SHIP_PAIR_WAIT_RSP_TICKS;
            g_ship_rt.pair_wait_start_ms = Task_GetTickMs();
            g_ship_rt.last_proto_rx_ms = g_ship_rt.pair_wait_start_ms;
            g_ship_rt.pair_rsp_timeout_logged = 0U;
            g_ship_rt.state = SHIP_STATE_WORK_RX;
            LOGI(SHIP_TAG, "pair sync idle done, wait %u ticks then work-rx",
                 (u16)g_ship_rt.wait_ticks);
        }
    }
}

static void ShipProtocol_StepWorkRx(void)
{
    s8 rc;

    if (g_ship_rt.work_rx_configured == 0U) {
        rc = ShipProtocol_ApplyWorkRx(1U);
        if (rc != SUCCESS) {
            LOGE(SHIP_TAG, "enter work rx fail rc=%d", rc);
            return;
        }
        if (g_ship_rt.last_proto_rx_ms == 0UL) {
            g_ship_rt.last_proto_rx_ms = Task_GetTickMs();
        }
    }

    if (g_ship_rt.work_state_logged == 0U) {
        g_ship_rt.work_state_logged = 1U;
        LOGI(SHIP_TAG, "enter work-state rx_ch=%u tx_ch=%u",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[0]);
    } else if (g_ship_rt.work_rx_configured == 0U) {
        rc = ShipProtocol_ApplyWorkRx(1U);
        if (rc != SUCCESS) {
            LOGE(SHIP_TAG, "restore work rx fail rc=%d", rc);
        }
    } else {
        g_ship_rt.work_rx_reopen_ticks++;
        if (g_ship_rt.work_rx_reopen_ticks > SHIP_WORK_RX_REOPEN_TICKS) {
            rc = ShipProtocol_ApplyWorkRx(0U);
            if (rc != SUCCESS) {
                LOGE(SHIP_TAG, "periodic work rx reopen fail rc=%d", rc);
            } else {
                g_ship_rt.work_rx_reopen_total++;
                if ((g_ship_rt.work_rx_reopen_total <= 3U) ||
                    ((g_ship_rt.work_rx_reopen_total % 50U) == 0U)) {
                    LOGI(SHIP_TAG, "work-rx reopen cnt=%u ch=%u",
                         (u16)g_ship_rt.work_rx_reopen_total,
                         (u16)g_ship_rt.rf_channel[0]);
                }
            }
        }
    }
}

void ShipProtocol_RunScheduler(void)
{
    static u8 initialized = 0U;
    static u32 last_tick_ms = 0U;
    u32 now_ms;

    if (!initialized) {
        ShipProtocol_InitRuntime();
        initialized = 1U;
        last_tick_ms = Task_GetTickMs();
    }

    now_ms = Task_GetTickMs();
    ShipProtocol_ServicePulse(now_ms);

    if ((now_ms - last_tick_ms) < 10U) {
        ShipProtocol_PollRxFrames();
        return;
    }
    last_tick_ms += 10U;

    ShipProtocol_PollRxFrames();

    if (g_ship_rt.pair_wait_rsp_time > 0U) {
        g_ship_rt.pair_wait_rsp_time--;
    } else if ((g_ship_rt.pair_wait_start_ms != 0UL) &&
               (g_ship_rt.pair_rsp_timeout_logged == 0U) &&
               ((now_ms - g_ship_rt.pair_wait_start_ms) >= SHIP_PAIR_RSP_EXPIRE_LOG_MS) &&
               (g_ship_rt.paired == 0U)) {
        g_ship_rt.pair_rsp_timeout_logged = 1U;
        LOGW(SHIP_TAG, "pair rsp window expired, no rsp");
        ShipProtocol_LogRxDebug("pair-rsp-expired");
    }

    if (g_ship_rt.wait_ticks > 0U) {
        g_ship_rt.wait_ticks--;
    } else {
        if (g_ship_rt.state == SHIP_STATE_BOOT_WAIT) {
            g_ship_rt.state = SHIP_STATE_PAIR_SEND;
        }

        switch (g_ship_rt.state) {
        case SHIP_STATE_PAIR_SEND:
            ShipProtocol_StepPairSend();
            break;
        case SHIP_STATE_WORK_RX:
            ShipProtocol_StepWorkRx();
            break;
        default:
            g_ship_rt.state = SHIP_STATE_PAIR_SEND;
            break;
        }
    }

    now_ms = Task_GetTickMs();

    if (g_ship_rt.state == SHIP_STATE_WORK_RX) {
        if ((g_ship_rt.last_proto_rx_ms != 0UL) &&
            (g_ship_rt.rx_idle_warned == 0U) &&
            ((now_ms - g_ship_rt.last_proto_rx_ms) >= SHIP_RX_IDLE_WARN_MS)) {
            g_ship_rt.rx_idle_warned = 1U;
            LOGW(SHIP_TAG, "work-rx idle %lums, no valid frame", (u32)(now_ms - g_ship_rt.last_proto_rx_ms));
            ShipProtocol_LogRxDebug("rx-idle");
        }

        if ((g_ship_rt.throttle_online != 0U) &&
            (g_ship_rt.last_throttle_rx_ms != 0UL) &&
            ((now_ms - g_ship_rt.last_throttle_rx_ms) >= SHIP_THROTTLE_TIMEOUT_MS)) {
            g_ship_rt.throttle_online = 0U;
            LOGW(SHIP_TAG, "remote link timeout by cmd=0x11, dt=%lums",
                 (u32)(now_ms - g_ship_rt.last_throttle_rx_ms));
            ShipProtocol_StopMotion("remote timeout", 1U);
            ShipProtocol_LogRxDebug("throttle-timeout");
        }

        if ((g_ship_rt.paired != 0U) &&
            (g_ship_rt.throttle_recover_done == 0U) &&
            (g_ship_rt.last_throttle_rx_ms != 0UL) &&
            ((now_ms - g_ship_rt.last_throttle_rx_ms) >= SHIP_THROTTLE_RECOVER_MS)) {
            g_ship_rt.throttle_recover_done = 1U;
            ShipProtocol_ApplyDefaultRf();
            g_ship_rt.work_rx_reopen_ticks = 0U;
            LOGW(SHIP_TAG,
                 "legacy remote recovery after %lums silence, re-open work-rx",
                 (u32)(now_ms - g_ship_rt.last_throttle_rx_ms));
            ShipProtocol_ReopenWorkRx("legacy remote recovery", 1U, 1U);
        }
    }
}

u8 ShipProtocol_IsPaired(void)
{
    return g_ship_rt.paired;
}
