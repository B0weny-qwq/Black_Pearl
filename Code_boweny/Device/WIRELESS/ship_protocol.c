/**
 * @file    ship_protocol.c
 * @brief   鑸圭鏃ч仴鎺у櫒鏃犵嚎涓氬姟鍗忚绉绘瀹炵幇銆?
 * @author  boweny
 * @date    2026-05-07
 * @version v1.2
 *
 * @details
 * 鏈枃浠朵繚鎸佹棫鐗?`Wireless/wirelessProtocal.c` 鐨勯厤瀵广€佹敹鍖呰В鏋愩€? * 鍥哄畾 `0x12` 鍥炰紶鍜?`0x11` 杞借嵎璇箟锛屽悓鏃舵寜褰撳墠鏍圭洰褰曞伐绋嬬殑鐪熷疄鐘舵€? * 鎺ュ叆濡備笅琛屼负锛? * - `0x11` 鎵嬪姩閾捐矾浼氭墽琛岃酱婊ゆ尝銆佸樊閫熸槧灏勶紝骞跺湪闂ㄦ帶婊¤冻鏃跺彔鍔?yaw-hold銆? * - `0x13/0x14/0x15` 浼氶┍鍔?`AutoDrive` 璁剧疆杩旇埅鐐广€佺洰鏍囩偣鍜岃嚜鍔ㄨ繑鑸紑鍏炽€? * - 閬ユ帶澶辫仈銆佷綆鐢靛拰绌洪棽鎬佷細涓?`AutoDrive`銆乣AHRS`銆乣Motor` 鍏卞悓宸ヤ綔銆? * 鍥犳鏈枃浠剁幇鍦ㄤ笉鏄€滃彧淇濈暀寮€鐜帶鑸光€濈殑鍘嗗彶鑱旇皟鐗堬紝鑰屾槸褰撳墠鏃犵嚎涓讳笟鍔″叆鍙ｃ€? */
#include "ship_protocol.h"
#include "wireless.h"
#include "..\..\Device\GPS\GPS.h"
#include "..\..\Device\AutoDrive\autodrive.h"
#include "..\..\Device\Motor\Motor.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\Function\PID\PID.h"
#include "..\..\..\User\MainLoop.h"
#include "..\..\..\User\Task.h"
#include "..\..\..\Driver\inc\STC32G_ADC.h"

#if !SHIP_PROTOCOL_DIAG_ENABLE
#undef LOGI
#undef LOGW
#undef LOGD
#define LOGI(tag, ...)
#define LOGW(tag, ...)
#define LOGD(tag, ...)
#endif

#ifndef SHIP_PROTOCOL_ERROR_LOG_ENABLE
#define SHIP_PROTOCOL_ERROR_LOG_ENABLE SHIP_PROTOCOL_DIAG_ENABLE
#endif

#if !SHIP_PROTOCOL_ERROR_LOG_ENABLE
#undef LOGE
#define LOGE(tag, ...)
#endif

#if SHIP_PROTO_DEBUG_ENABLE
#define SHIP_PROTO_DBG(...) LOGI(SHIP_TAG, __VA_ARGS__)
#else
#define SHIP_PROTO_DBG(...)
#endif

#if SHIP_PROTOCOL_DIAG_ENABLE
#define SHIP_REASON_C(text)  (text)
#define SHIP_REASON_U8(text) ((const u8 *)(text))
#define SHIP_STAGE_U8(text)  ((const u8 *)(text))
#else
#define SHIP_REASON_C(text)  ((const char *)0)
#define SHIP_REASON_U8(text) ((const u8 *)0)
#define SHIP_STAGE_U8(text)  ((const u8 *)0)
#endif

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
#define SHIP_CRUISE_STOP               0U
#define SHIP_CRUISE_HIGH               3U
#define SHIP_LEGACY_PWM_SCALE          10
#define SHIP_POWER_LEVEL_0             0U
#define SHIP_POWER_LEVEL_1             1U
#define SHIP_POWER_LEVEL_2             2U
#define SHIP_POWER_LEVEL_3             3U
#define SHIP_POWER_LEVEL_4             4U
#define SHIP_AXIS_FILTER_SHIFT         2U
#define SHIP_AXIS_MIX_DEADBAND         10
#define SHIP_THROTTLE_DEADBAND         4
#define SHIP_STEERING_DEADBAND         8
#define SHIP_THROTTLE_MIN_COMMAND      180
#define SHIP_THROTTLE_MAX_COMMAND      850
#define SHIP_STEERING_MAX_COMMAND      700
#define SHIP_PULSE_DURATION_MS         150U
#define SHIP_PULSE_SPEED               700
#define SHIP_THROTTLE_RECOVER_MS       3000UL
#define SHIP_REPEAT_LOG_MS             500UL
#define SHIP_LOWPOWER_CHECK_TICKS      600U
#define SHIP_POWER_SAMPLE_DIVIDER      100U
#define SHIP_YAW_HOLD_FORWARD_ONLY     1U
#define SHIP_CENTER_STOP_CONFIRM_FRAMES 2U
#ifndef SHIP_YAW_HOLD_PERIOD_MS
#define SHIP_YAW_HOLD_PERIOD_MS        100UL
#endif
#ifndef SHIP_YAW_HOLD_LOG_ENABLE
#define SHIP_YAW_HOLD_LOG_ENABLE       1
#endif
#ifndef SHIP_YAW_HOLD_LOG_PERIOD_MS
#define SHIP_YAW_HOLD_LOG_PERIOD_MS    1000UL
#endif

#ifdef BOARD_12V
#define SHIP_BATT_ADC_FULL_RAW         2000U
#define SHIP_BATT_ADC_LEVEL3_RAW       1900U
#define SHIP_BATT_ADC_LEVEL2_RAW       1730U
#define SHIP_BATT_ADC_LEVEL1_RAW       1620U
#else
#define SHIP_BATT_ADC_FULL_RAW         1710U
#define SHIP_BATT_ADC_LEVEL3_RAW       1630U
#define SHIP_BATT_ADC_LEVEL2_RAW       1530U
#define SHIP_BATT_ADC_LEVEL1_RAW       1420U
#endif

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
    int32 filtered_lr_q8;
    int32 filtered_ud_q8;
    u8 pair_rsp_timeout_logged;
    u8 rx_idle_warned;
    u8 remote_online;
    u8 throttle_online;
    u8 throttle_recover_done;
    u8 motor_initialized;
    u8 yaw_hold_active;
    int16 yaw_hold_target_cd;
    int16 yaw_hold_output;
    u32 yaw_hold_last_update_ms;
    ShipMotion_t motion;
    ShipMotion_t pulse_motion;
    u32 pulse_expire_ms;
    u8 pulse_active;
    u8 center_stop_count;
} ShipRuntime_t;

typedef struct
{
    u16 raw;
    u16 adc_mv;
    u32 bat_mv;
    u8 report;
    u8 valid;
} ShipPowerSample_t;

static ShipRuntime_t xdata g_ship_rt;
static PID_Controller_t xdata g_ship_yaw_pid;
static u8 g_ship_power_sample_times = 0U;
static u16 g_lowpower_check_times = 0U;
static u8 g_now_pwm_accelerator = 0U;
static u8 g_ship_power_level = SHIP_POWER_LEVEL_0;
static ShipPowerSample_t g_ship_power_sample;
static u8 g_manual_cruise_mode = SHIP_CRUISE_STOP;
static u8 g_manual_in_cruise_run = 0U;
static u8 g_manual_first_cruise_run = 0U;
static u8 xdata g_ship_tx_frame[SHIP_PROTO_MAX_FRAME_LEN];
static u8 xdata g_ship_rx_frame[SHIP_PROTO_MAX_FRAME_LEN];
static u8 xdata g_ship_parse_frame[SHIP_LEGACY_PROTO_MAX_LEN];

static s8 ShipProtocol_ApplyWorkSyncIdle(u8 log_rxdbg);
static s8 ShipProtocol_ApplyWorkRx(u8 log_rxdbg);
static void ShipProtocol_ReopenWorkRx(const char *reason, u8 log_rxdbg, u8 log_ok);
static void ShipProtocol_MarkPairedByFrame(u8 cmd, const char *reason);
static void ShipProtocol_ResetYawHold(const char *reason, u8 force_log);
static void ShipProtocol_EnsureMotorInit(void);
static int16 ShipProtocol_LimitSpeed(int16 speed);
static int16 ShipProtocol_WrapCd(int32 angle_cd);
static void ShipProtocol_ReadPowerSample(ShipPowerSample_t *sample);
static void ShipProtocol_ServicePowerSample(void);
static u8 ShipProtocol_AdcRawToPowerLevel(u16 adc_raw);
static int16 ShipProtocol_YawOutputToSpeed(int16 yaw_output, int16 throttle_speed);
static void ShipProtocol_UpdateManualAcceleratorRaw(u8 left_right, u8 front_back);
static u8 ShipProtocol_AbsAxisDiff(u8 value);
static int16 ShipProtocol_LegacyPwmToSpeed(u8 pwm);
static u8 ShipProtocol_IsLowPower(void);
static void ShipProtocol_LowPowerCheck(void);
static const char *ShipProtocol_KeyNameAlways(u8 key);
static u8 ShipProtocol_ConfirmCenterStop(u8 log_this_sample);
#if SHIP_YAW_HOLD_ENABLE
static u8 ShipProtocol_UpdateYawHoldPid(u32 now_ms, int16 *yaw_cd, u8 *pid_updated);
static void ShipProtocol_LogMotorOutput(u8 mode, int16 yaw_cd, int16 throttle_speed, int16 yaw_output, int16 left_speed, int16 right_speed, u8 force_log);
static void ShipProtocol_ServiceIdleYawHold(u32 now_ms);
#endif
#if SHIP_PROTOCOL_DIAG_ENABLE
static const char *ShipProtocol_CmdName(u8 cmd);
static const char *ShipProtocol_KeyName(u8 key);
static const char *ShipProtocol_MotionName(ShipMotion_t motion);
static void ShipProtocol_LogManualDecision(int16 left_right, int16 front_back, u8 key, ShipMotion_t target_motion, int16 speed, u8 force_log);
static void ShipProtocol_LogMotion(ShipMotion_t motion, int16 left_speed, int16 right_speed);
static void ShipProtocol_LogRxDebug(const u8 *stage);
static void ShipProtocol_LogPayloadBrief(const u8 *stage, u8 cmd, const u8 *payload, u8 payload_len);
static void ShipProtocol_LogFrameBrief(const u8 *stage, u8 channel, const u8 *frame, u8 frame_len);
static void ShipProtocol_LogPwmSnapshot(u8 force_log);
static void ShipProtocol_LogPowerSample(const ShipPowerSample_t *sample, u8 force_log);
static void ShipProtocol_LogCoordBE(const u8 *buf, u8 len);
#else
#define ShipProtocol_LogManualDecision(left_right, front_back, key, target_motion, speed, force_log)
#define ShipProtocol_LogMotion(motion, left_speed, right_speed)
#define ShipProtocol_LogRxDebug(stage)
#define ShipProtocol_LogPayloadBrief(stage, cmd, payload, payload_len)
#define ShipProtocol_LogFrameBrief(stage, channel, frame, frame_len)
#define ShipProtocol_LogPwmSnapshot(force_log)
#define ShipProtocol_LogPowerSample(sample, force_log)
#define ShipProtocol_LogCoordBE(buf, len)
#endif
static u8 ShipProtocol_ShouldLogManualSample(u8 left_right, u8 front_back, u8 key, u32 now_ms);

static int16 ShipProtocol_WrapCd(int32 angle_cd)
{
    while (angle_cd >= 18000L) {
        angle_cd -= 36000L;
    }
    while (angle_cd < -18000L) {
        angle_cd += 36000L;
    }
    return (int16)angle_cd;
}

static u32 ShipProtocol_ElapsedMs(u32 now_ms, u32 start_ms)
{
    if (start_ms == 0UL) {
        return 0UL;
    }
    if (now_ms < start_ms) {
        return 0UL;
    }
    return (u32)(now_ms - start_ms);
}

static void ShipProtocol_ResetYawHold(const char *reason, u8 force_log)
{
#if SHIP_YAW_HOLD_ENABLE
    if ((g_ship_rt.yaw_hold_active != 0U) && (force_log != 0U)) {
        LOGI(SHIP_TAG, "yaw hold off reason=%s tgt=%d out=%d",
             reason,
             g_ship_rt.yaw_hold_target_cd,
             g_ship_rt.yaw_hold_output);
    }
    g_ship_rt.yaw_hold_active = 0U;
    g_ship_rt.yaw_hold_target_cd = 0;
    g_ship_rt.yaw_hold_output = 0;
    g_ship_rt.yaw_hold_last_update_ms = 0UL;
    PID_Reset(&g_ship_yaw_pid);
#else
    (void)reason;
    (void)force_log;
#endif
}

static const char *ShipProtocol_KeyNameAlways(u8 key)
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

#if SHIP_YAW_HOLD_ENABLE
static u8 ShipProtocol_UpdateYawHoldPid(u32 now_ms, int16 *yaw_cd, u8 *pid_updated)
{
    int16 current_yaw_cd;
    int16 yaw_error_cd;

    if (pid_updated != 0) {
        *pid_updated = 0U;
    }
    if (MainLoop_IsHeadingReady() == 0U) {
        return 0U;
    }

    current_yaw_cd = MainLoop_GetHeadingRelativeDeg100();
    if (yaw_cd != 0) {
        *yaw_cd = current_yaw_cd;
    }

    if (g_ship_rt.yaw_hold_active == 0U) {
        g_ship_rt.yaw_hold_active = 1U;
        g_ship_rt.yaw_hold_target_cd = current_yaw_cd;
        g_ship_rt.yaw_hold_output = 0;
        g_ship_rt.yaw_hold_last_update_ms = now_ms - SHIP_YAW_HOLD_PERIOD_MS;
        PID_Reset(&g_ship_yaw_pid);
        PID_SetTarget(&g_ship_yaw_pid, 0);
    }

    if ((now_ms - g_ship_rt.yaw_hold_last_update_ms) >= SHIP_YAW_HOLD_PERIOD_MS) {
        g_ship_rt.yaw_hold_last_update_ms = now_ms;
        yaw_error_cd = ShipProtocol_WrapCd((int32)g_ship_rt.yaw_hold_target_cd -
                                           (int32)current_yaw_cd);
        if ((yaw_error_cd <= SHIP_YAW_HOLD_DEADBAND_CD) &&
            (yaw_error_cd >= (int16)(-SHIP_YAW_HOLD_DEADBAND_CD))) {
            yaw_error_cd = 0;
        }
        g_ship_rt.yaw_hold_output = PID_UpdateTarget(&g_ship_yaw_pid, yaw_error_cd, 0);
        if (pid_updated != 0) {
            *pid_updated = 1U;
        }
    }

    return 1U;
}

static void ShipProtocol_LogMotorOutput(u8 mode, int16 yaw_cd, int16 throttle_speed, int16 yaw_output, int16 left_speed, int16 right_speed, u8 force_log)
{
#if SHIP_YAW_HOLD_LOG_ENABLE
    static u32 last_log_ms = 0UL;
    u32 now_ms;

    now_ms = Task_GetTickMs();
    if ((force_log == 0U) &&
        (SHIP_MOT_LOG_PERIOD_MS != 0U) &&
        ((now_ms - last_log_ms) < SHIP_MOT_LOG_PERIOD_MS)) {
        return;
    }
    last_log_ms = now_ms;

    log_info((u8 *)"MOT",
             (u8 *)"m=%u y=%d t=%d o=%d l=%d r=%d",
             (u16)mode,
             yaw_cd,
             throttle_speed,
             yaw_output,
             left_speed,
             right_speed);
#else
    (void)mode;
    (void)yaw_cd;
    (void)throttle_speed;
    (void)yaw_output;
    (void)left_speed;
    (void)right_speed;
    (void)force_log;
#endif
}

static void ShipProtocol_ServiceIdleYawHold(u32 now_ms)
{
    int16 yaw_cd;
    int16 yaw_output;
    int16 left_speed;
    int16 right_speed;
    u8 pid_updated;

    if (g_ship_rt.pulse_active != 0U) {
        return;
    }
    if ((g_ship_rt.throttle_online != 0U) && (g_ship_rt.valid != 0U)) {
        return;
    }
    if (ShipProtocol_UpdateYawHoldPid(now_ms, &yaw_cd, &pid_updated) == 0U) {
        return;
    }
    if (pid_updated == 0U) {
        return;
    }

    yaw_output = ShipProtocol_YawOutputToSpeed(g_ship_rt.yaw_hold_output, 0);
    left_speed = ShipProtocol_LimitSpeed(yaw_output);
    right_speed = ShipProtocol_LimitSpeed((int16)(-yaw_output));

    ShipProtocol_EnsureMotorInit();
#if SHIP_THROTTLE_PWM_ENABLE
    Motor_SetBothSpeed(left_speed, right_speed);
#endif
    ShipProtocol_LogMotorOutput(0U, yaw_cd, 0, yaw_output, left_speed, right_speed, 0U);
}
#endif

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

static int16 ShipProtocol_YawOutputToSpeed(int16 yaw_output, int16 throttle_speed)
{
    int32 scale;

    scale = (int32)MOTOR_SPEED_MAX;
    if (throttle_speed > 0) {
        scale = (int32)throttle_speed;
    } else if (throttle_speed < 0) {
        scale = (int32)(-throttle_speed);
    }

    return ShipProtocol_LimitSpeed((int16)(((int32)yaw_output * scale) / 100L));
}

static u8 ShipProtocol_AbsAxisDiff(u8 value)
{
    return (value > SHIP_AXIS_CENTER) ?
           (u8)(value - SHIP_AXIS_CENTER) :
           (u8)(SHIP_AXIS_CENTER - value);
}

static int16 ShipProtocol_LegacyPwmToSpeed(u8 pwm)
{
    if (pwm > 100U) {
        pwm = 100U;
    }
    return ShipProtocol_LimitSpeed((int16)((int16)pwm * SHIP_LEGACY_PWM_SCALE));
}

static int16 ShipProtocol_FilterAxis(u8 raw, int32 *state_q8)
{
    int32 target_q8;

    target_q8 = ((int32)raw << 8);
    *state_q8 += ((target_q8 - *state_q8) >> SHIP_AXIS_FILTER_SHIFT);
    return (int16)((*state_q8 + 128) >> 8);
}

static int16 ShipProtocol_ApplyAxisCurve(int16 value, int16 deadband,
                                         int16 min_command, int16 max_command)
{
    int16 delta;
    int16 sign;
    int16 magnitude;
    int16 range;
    int32 command;

    delta = value - (int16)SHIP_AXIS_CENTER;
    if (delta == 0) {
        return 0;
    }

    sign = (delta > 0) ? 1 : -1;
    magnitude = (delta > 0) ? delta : (int16)(-delta);
    if (magnitude <= deadband) {
        return 0;
    }

    range = (int16)(SHIP_AXIS_CENTER - deadband);
    magnitude = (int16)(magnitude - deadband);
    if (range <= 0) {
        return 0;
    }

    if (magnitude > range) {
        magnitude = range;
    }

    if (max_command <= min_command) {
        command = max_command;
    } else {
        command = min_command;
        command += ((int32)magnitude * (int32)magnitude *
                    (int32)(max_command - min_command)) /
                   ((int32)range * (int32)range);
    }

    if (command > max_command) {
        command = max_command;
    }

    return (int16)(sign * (int16)command);
}

static int16 ShipProtocol_ThrottleToSignedSpeed(int16 value)
{
    return ShipProtocol_ApplyAxisCurve(value,
                                       SHIP_THROTTLE_DEADBAND,
                                       SHIP_THROTTLE_MIN_COMMAND,
                                       SHIP_THROTTLE_MAX_COMMAND);
}

static int16 ShipProtocol_SteeringToSignedSpeed(int16 value)
{
    return ShipProtocol_ApplyAxisCurve(value,
                                       SHIP_STEERING_DEADBAND,
                                       0,
                                       SHIP_STEERING_MAX_COMMAND);
}

static void ShipProtocol_UpdateManualAcceleratorRaw(u8 left_right, u8 front_back)
{
    u8 abs_left_right;
    u8 abs_front_back;

    abs_left_right = ShipProtocol_AbsAxisDiff(left_right);
    abs_front_back = ShipProtocol_AbsAxisDiff(front_back);
    abs_front_back = (u8)(abs_front_back + 5U);

    if ((abs_front_back > abs_left_right) &&
        ((abs_left_right > 10U) || (abs_front_back > 20U)) &&
        (front_back > 110U)) {
        g_now_pwm_accelerator = (u8)(front_back - SHIP_AXIS_CENTER);
    } else {
        g_now_pwm_accelerator = 0U;
    }
}

static u8 ShipProtocol_IsLowPower(void)
{
    return (g_ship_power_level == SHIP_POWER_LEVEL_0) ? 1U : 0U;
}

static void ShipProtocol_LowPowerCheck(void)
{
    ShipProtocol_ServicePowerSample();
    ShipProtocol_LogPowerSample(&g_ship_power_sample, 0U);
    g_lowpower_check_times++;
    if (g_lowpower_check_times > SHIP_LOWPOWER_CHECK_TICKS) {
        g_lowpower_check_times = 0U;
        if (ShipProtocol_IsLowPower() &&
            (AutoDrive_GetMode() == AUTO_DRIVE_CLOSE) &&
            (g_now_pwm_accelerator < 10U)) {
            AutoDrive_TriggerReturn();
        }
    } else if (ShipProtocol_IsLowPower() == 0U) {
        g_lowpower_check_times = 0U;
    }
}

static void ShipProtocol_ResetAxisFilter(void)
{
    g_ship_rt.filtered_lr_q8 = ((int32)SHIP_AXIS_CENTER << 8);
    g_ship_rt.filtered_ud_q8 = ((int32)SHIP_AXIS_CENTER << 8);
}

static u8 ShipProtocol_ConfirmCenterStop(u8 log_this_sample)
{
    if (g_ship_rt.motion == SHIP_MOTION_STOP) {
        g_ship_rt.center_stop_count = 0U;
        return 1U;
    }

    if (g_ship_rt.center_stop_count < SHIP_CENTER_STOP_CONFIRM_FRAMES) {
        g_ship_rt.center_stop_count++;
    }

    if (g_ship_rt.center_stop_count < SHIP_CENTER_STOP_CONFIRM_FRAMES) {
#if SHIP_PROTOCOL_DIAG_ENABLE
        if (log_this_sample != 0U) {
            LOGI(SHIP_TAG,
                 "manual center debounce count=%u/%u keep=%s",
                 (u16)g_ship_rt.center_stop_count,
                 (u16)SHIP_CENTER_STOP_CONFIRM_FRAMES,
                 ShipProtocol_MotionName(g_ship_rt.motion));
        }
#else
        (void)log_this_sample;
#endif
        return 0U;
    }

    return 1U;
}

#if SHIP_PROTOCOL_DIAG_ENABLE
static const char *ShipProtocol_CmdName(u8 cmd)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)cmd;
    return (const char *)0;
#endif
}

static const char *ShipProtocol_KeyName(u8 key)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)key;
    return (const char *)0;
#endif
}

static const char *ShipProtocol_MotionName(ShipMotion_t motion)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)motion;
    return (const char *)0;
#endif
}
#endif

static u8 ShipProtocol_ShouldLogManualSample(u8 left_right, u8 front_back, u8 key, u32 now_ms)
{
    static u8 last_left_right = SHIP_AXIS_CENTER;
    static u8 last_front_back = SHIP_AXIS_CENTER;
    static u8 last_key = SHIP_KEY_NULL;
    static u32 last_log_ms = 0UL;

    if ((left_right != last_left_right) ||
        (front_back != last_front_back) ||
        (key != last_key) ||
        (SHIP_RC_INPUT_LOG_PERIOD_MS == 0U) ||
        ((now_ms - last_log_ms) >= SHIP_RC_INPUT_LOG_PERIOD_MS)) {
        last_left_right = left_right;
        last_front_back = front_back;
        last_key = key;
        last_log_ms = now_ms;
        return 1U;
    }

    return 0U;
}

#if SHIP_PROTOCOL_DIAG_ENABLE
static void ShipProtocol_LogManualDecision(int16 left_right, int16 front_back, u8 key, ShipMotion_t target_motion, int16 speed, u8 force_log)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)left_right;
    (void)front_back;
    (void)key;
    (void)target_motion;
    (void)speed;
    (void)force_log;
#endif
}

static void ShipProtocol_LogMotion(ShipMotion_t motion, int16 left_speed, int16 right_speed)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
    LOGI(SHIP_TAG, "manual motion=%s left=%d right=%d",
         ShipProtocol_MotionName(motion),
         left_speed,
         right_speed);
#else
    (void)motion;
    (void)left_speed;
    (void)right_speed;
#endif
}

static void ShipProtocol_LogPwmSnapshot(u8 force_log)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
#if SHIP_THROTTLE_PWM_ENABLE
    Motor_PwmSnapshot_t snapshot;

    if (force_log == 0U) {
        return;
    }

    Motor_GetPwmSnapshot(&snapshot);
    LOGI(SHIP_TAG,
         "pwm pins mla=%u mlb=%u mra=%u mrb=%u period=%u",
         snapshot.mla_duty,
         snapshot.mlb_duty,
         snapshot.mra_duty,
         snapshot.mrb_duty,
         snapshot.period);
#else
    (void)force_log;
#endif
#else
    (void)force_log;
#endif
}
#endif

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
        right_speed = speed;
        break;
    case SHIP_MOTION_BACKWARD:
        left_speed = -speed;
        right_speed = -speed;
        break;
    case SHIP_MOTION_LEFT:
        left_speed = -speed;
        right_speed = speed;
        break;
    case SHIP_MOTION_RIGHT:
        left_speed = speed;
        right_speed = -speed;
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
        ShipProtocol_LogPwmSnapshot(1U);
    }
}

static void ShipProtocol_StopMotion(const u8 *reason, u8 force_log)
{
    ShipMotion_t prev_motion;

    ShipProtocol_EnsureMotorInit();
    ShipProtocol_ResetYawHold((const char *)reason, force_log);
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
        ShipProtocol_LogPwmSnapshot(1U);
    }
}

static void ShipProtocol_StartPulse(ShipMotion_t motion)
{
    ShipProtocol_ResetYawHold(SHIP_REASON_C("pulse"), 1U);
    g_ship_rt.pulse_active = 1U;
    g_ship_rt.pulse_motion = motion;
    g_ship_rt.pulse_expire_ms = Task_GetTickMs() + SHIP_PULSE_DURATION_MS;
    ShipProtocol_ApplyMotion(motion, SHIP_PULSE_SPEED, 1U);
    LOGI(SHIP_TAG, "key pulse motion=%s dur=%ums",
         ShipProtocol_MotionName(motion),
         (u16)SHIP_PULSE_DURATION_MS);
}

static void ShipProtocol_ServicePulse(u32 now_ms)
{
    if ((g_ship_rt.pulse_active != 0U) && ((int32)(now_ms - g_ship_rt.pulse_expire_ms) >= 0)) {
        g_ship_rt.pulse_active = 0U;
        g_ship_rt.pulse_motion = SHIP_MOTION_STOP;
        g_ship_rt.pulse_expire_ms = 0UL;
        ShipProtocol_StopMotion(SHIP_REASON_U8("pulse done"), 1U);
    }
}

static void ShipProtocol_LogLightPending(void)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
    if (g_ship_rt.light_toggle_pending == 0U) {
        g_ship_rt.light_toggle_pending = 1U;
        LOGW(SHIP_TAG, "key action=A light-unbound");
    } else {
        LOGI(SHIP_TAG, "key action=A light-unbound repeat");
    }
#else
    g_ship_rt.light_toggle_pending = 1U;
#endif
}

static void ShipProtocol_ApplyManualControl(u8 left_right, u8 front_back, u8 log_this_sample)
{
    int16 throttle_speed;
    int16 steering_speed;
    int16 left_speed;
    int16 right_speed;
    int16 abs_throttle;
    int16 abs_steering;
    ShipMotion_t target_motion;
    int16 yaw_cd;
    int16 yaw_output;
    u32 now_ms;
    u8 pid_updated;

    if ((left_right >= SHIP_LR_DEAD_LOW) && (left_right <= SHIP_LR_DEAD_HIGH) &&
        (front_back >= SHIP_FB_DEAD_LOW) && (front_back <= SHIP_FB_DEAD_HIGH)) {
        if (ShipProtocol_ConfirmCenterStop(log_this_sample) == 0U) {
            return;
        }
        ShipProtocol_ResetAxisFilter();
        g_manual_in_cruise_run = 0U;
        g_now_pwm_accelerator = 0U;
        ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key,
                                       SHIP_MOTION_STOP, 0, log_this_sample);
        ShipProtocol_StopMotion(SHIP_REASON_U8("manual center"), log_this_sample);
        return;
    }
    g_ship_rt.center_stop_count = 0U;

    throttle_speed = ShipProtocol_ThrottleToSignedSpeed(ShipProtocol_FilterAxis(front_back, &g_ship_rt.filtered_ud_q8));
    steering_speed = ShipProtocol_SteeringToSignedSpeed(ShipProtocol_FilterAxis(left_right, &g_ship_rt.filtered_lr_q8));
    abs_throttle = (throttle_speed >= 0) ? throttle_speed : (int16)(-throttle_speed);
    abs_steering = (steering_speed >= 0) ? steering_speed : (int16)(-steering_speed);
    left_speed = ShipProtocol_LimitSpeed((int16)(throttle_speed + steering_speed));
    right_speed = ShipProtocol_LimitSpeed((int16)(throttle_speed - steering_speed));
    target_motion = SHIP_MOTION_STOP;
    yaw_output = 0;

    if ((abs_throttle == 0) && (abs_steering == 0)) {
        ShipProtocol_ResetAxisFilter();
        g_manual_in_cruise_run = 0U;
        g_now_pwm_accelerator = 0U;
        ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key,
                                       SHIP_MOTION_STOP, 0, log_this_sample);
        ShipProtocol_StopMotion(SHIP_REASON_U8("manual center"), log_this_sample);
        return;
    }

#if SHIP_YAW_HOLD_ENABLE && SHIP_YAW_HOLD_MANUAL_ENABLE
    if ((abs_steering <= (int16)SHIP_YAW_HOLD_STEER_GATE) &&
#if SHIP_YAW_HOLD_FORWARD_ONLY
        (throttle_speed > 0)
#else
        (throttle_speed != 0)
#endif
        ) {
        now_ms = Task_GetTickMs();
        if (ShipProtocol_UpdateYawHoldPid(now_ms, &yaw_cd, &pid_updated) != 0U) {
            yaw_output = ShipProtocol_YawOutputToSpeed(g_ship_rt.yaw_hold_output, throttle_speed);
            left_speed = ShipProtocol_LimitSpeed((int16)(throttle_speed + yaw_output));
            right_speed = ShipProtocol_LimitSpeed((int16)(throttle_speed - yaw_output));
            target_motion = (throttle_speed >= 0) ? SHIP_MOTION_FORWARD : SHIP_MOTION_BACKWARD;
            if (log_this_sample != 0U) {
                LOGI(SHIP_TAG,
                     "yaw hold tgt=%d yr=%d out=%d throttle=%d steer=%d gate=%u left=%d right=%d",
                     g_ship_rt.yaw_hold_target_cd,
                     yaw_cd,
                     yaw_output,
                     throttle_speed,
                     steering_speed,
                     (u16)SHIP_YAW_HOLD_STEER_GATE,
                     left_speed,
                     right_speed);
            }
            ShipProtocol_EnsureMotorInit();
#if SHIP_THROTTLE_PWM_ENABLE
            Motor_SetBothSpeed(left_speed, right_speed);
#endif
            g_ship_rt.motion = target_motion;
            ShipProtocol_LogMotion(target_motion, left_speed, right_speed);
            ShipProtocol_LogPwmSnapshot(1U);
            ShipProtocol_LogMotorOutput((u8)target_motion, yaw_cd, throttle_speed, yaw_output, left_speed, right_speed, log_this_sample);
            return;
        }
    }
#endif

#if SHIP_YAW_HOLD_ENABLE
    if (g_ship_rt.yaw_hold_active != 0U) {
        ShipProtocol_ResetYawHold(SHIP_REASON_C("manual steer/open"), log_this_sample);
    }
#endif

    if ((abs_throttle + SHIP_TURN_COMPARE_BIAS) >= abs_steering) {
        target_motion = (throttle_speed >= 0) ? SHIP_MOTION_FORWARD : SHIP_MOTION_BACKWARD;
    } else {
        target_motion = (steering_speed >= 0) ? SHIP_MOTION_RIGHT : SHIP_MOTION_LEFT;
    }

    ShipProtocol_LogManualDecision(left_right, front_back, g_ship_rt.key,
                                   target_motion,
                                   (abs_throttle >= abs_steering) ? abs_throttle : abs_steering,
                                   log_this_sample);
    ShipProtocol_ApplyMotion(target_motion,
                             (abs_throttle >= abs_steering) ? abs_throttle : abs_steering,
                             log_this_sample);
    ShipProtocol_LogMotorOutput((u8)target_motion, yaw_cd, throttle_speed, yaw_output, left_speed, right_speed, log_this_sample);
}

static void ShipProtocol_HandleKey(u8 front_back, u8 key)
{
    if (key == g_ship_rt.last_key) {
        return;
    }
    g_ship_rt.last_key = key;

    switch (key) {
    case SHIP_KEY_A_TOGGLE_LIGHT:
        ShipProtocol_LogLightPending();
        break;
    case SHIP_KEY_B_UNUSED:
        LOGI(SHIP_TAG, "key action=B noop");
        break;
    case SHIP_KEY_C_PULSE_FORWARD:
        LOGI(SHIP_TAG, "key action=C pulse-forward %ums", (u16)SHIP_PULSE_DURATION_MS);
        ShipProtocol_StartPulse(SHIP_MOTION_FORWARD);
        break;
    case SHIP_KEY_D_PULSE_BACKWARD:
        LOGI(SHIP_TAG, "key action=D pulse-backward %ums", (u16)SHIP_PULSE_DURATION_MS);
        ShipProtocol_StartPulse(SHIP_MOTION_BACKWARD);
        break;
    case SHIP_KEY_E_RESERVED:
        if (front_back > 150U) {
            g_manual_in_cruise_run = 0U;
            g_manual_cruise_mode = SHIP_CRUISE_HIGH;
            g_manual_first_cruise_run = 1U;
            g_now_pwm_accelerator = 100U;
            ShipProtocol_ApplyMotion(SHIP_MOTION_FORWARD,
                                     ShipProtocol_LegacyPwmToSpeed(100U),
                                     1U);
            LOGI(SHIP_TAG, "key action=E cruise-high");
        } else if (front_back < 110U) {
            g_manual_cruise_mode = SHIP_CRUISE_STOP;
            g_manual_in_cruise_run = 0U;
            g_manual_first_cruise_run = 0U;
            g_now_pwm_accelerator = 0U;
            LOGI(SHIP_TAG, "key action=E cruise-stop");
        }
        AutoDrive_SetMode(AUTO_DRIVE_CLOSE);
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

static u8 ShipProtocol_AdcRawToPowerLevel(u16 adc_raw)
{
    if (adc_raw >= SHIP_BATT_ADC_FULL_RAW) {
        return SHIP_POWER_LEVEL_4;
    }
    if (adc_raw >= SHIP_BATT_ADC_LEVEL3_RAW) {
        return SHIP_POWER_LEVEL_3;
    }
    if (adc_raw >= SHIP_BATT_ADC_LEVEL2_RAW) {
        return SHIP_POWER_LEVEL_2;
    }
    if (adc_raw >= SHIP_BATT_ADC_LEVEL1_RAW) {
        return SHIP_POWER_LEVEL_1;
    }
    return SHIP_POWER_LEVEL_0;
}

static void ShipProtocol_ReadPowerSample(ShipPowerSample_t *sample)
{
    u16 adc_raw;

    if (sample == 0) {
        return;
    }

    sample->raw = g_ship_power_sample.raw;
    sample->adc_mv = g_ship_power_sample.adc_mv;
    sample->bat_mv = g_ship_power_sample.bat_mv;
    sample->report = g_ship_power_level;
    sample->valid = g_ship_power_sample.valid;
    adc_raw = Get_ADCResult(ADC_CH8);
    if (adc_raw > 4095U) {
        sample->raw = adc_raw;
        sample->adc_mv = 0U;
        sample->bat_mv = 0UL;
        sample->report = g_ship_power_level;
        sample->valid = 0U;
        return;
    }

    sample->raw = adc_raw;
    sample->adc_mv = ShipProtocol_AdcRawToMv(adc_raw);
    sample->bat_mv = ShipProtocol_AdcMvToBatteryMv(sample->adc_mv);
    sample->report = ShipProtocol_AdcRawToPowerLevel(adc_raw);
    sample->valid = 1U;
}

static void ShipProtocol_ServicePowerSample(void)
{
    if (g_ship_power_sample_times < SHIP_POWER_SAMPLE_DIVIDER) {
        g_ship_power_sample_times++;
        return;
    }
    g_ship_power_sample_times = 0U;

    ShipProtocol_ReadPowerSample(&g_ship_power_sample);
    if (g_ship_power_sample.valid != 0U) {
        g_ship_power_level = g_ship_power_sample.report;
    }
}

#if SHIP_PROTOCOL_DIAG_ENABLE
static void ShipProtocol_LogPowerSample(const ShipPowerSample_t *sample, u8 force_log)
{
#if SHIP_ADC_LOG_ENABLE
    static u32 last_log_ms = 0UL;
    u32 now_ms;

    if (sample == 0) {
        return;
    }
    now_ms = Task_GetTickMs();
    if ((force_log == 0U) &&
        (SHIP_POWER_LOG_PERIOD_MS != 0U) &&
        ((now_ms - last_log_ms) < SHIP_POWER_LOG_PERIOD_MS)) {
        return;
    }
    last_log_ms = now_ms;

    if (sample->valid == 0U) {
        LOGW(SHIP_TAG, "adc p0.0 read fail raw=%u power_level=%u",
             (u16)sample->raw,
             (u16)sample->report);
        return;
    }

    LOGI(SHIP_TAG, "adc p0.0 raw=%u adc_mv=%u bat_mv=%lu power_level=%u",
         (u16)sample->raw,
         (u16)sample->adc_mv,
         (u32)sample->bat_mv,
         (u16)sample->report);
#else
    (void)force_log;
    (void)sample;
#endif
}
#endif

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

#if SHIP_PROTOCOL_DIAG_ENABLE
static u16 ShipProtocol_ReadU16Legacy(const u8 *buf)
{
    return (u16)(((u16)buf[1] << 8) | buf[0]);
}
#endif

static void ShipProtocol_ToLegacyNmeaCoord(u32 abs_deg1e7, u16 *coord1, u16 *coord2)
{
    u32 degrees;
    u32 minutes_scaled1e4;

    degrees = abs_deg1e7 / 10000000UL;
    minutes_scaled1e4 = (((abs_deg1e7 % 10000000UL) * 6UL) + 50UL) / 100UL;
    if (minutes_scaled1e4 >= 600000UL) {
        degrees++;
        minutes_scaled1e4 = 0UL;
    }

    *coord1 = (u16)((degrees * 100UL) + (minutes_scaled1e4 / 10000UL));
    *coord2 = (u16)(minutes_scaled1e4 % 10000UL);
}

static void ShipProtocol_WriteU16Legacy(u8 *dst, u16 value)
{
    dst[0] = (u8)(value & 0xFFU);
    dst[1] = (u8)(value >> 8);
}

#if SHIP_PROTOCOL_DIAG_ENABLE
static void ShipProtocol_LogCoordBE(const u8 *buf, u8 len)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
    if (len < 10U) {
        LOGW(SHIP_TAG, "coord short len=%u", (u16)len);
        return;
    }

    LOGI(SHIP_TAG,
         "coord lonEW=0x%02X lon=%u.%u latNS=0x%02X lat=%u.%u",
         (u16)buf[0],
         ShipProtocol_ReadU16Legacy(&buf[1]),
         ShipProtocol_ReadU16Legacy(&buf[3]),
         (u16)buf[5],
         ShipProtocol_ReadU16Legacy(&buf[6]),
         ShipProtocol_ReadU16Legacy(&buf[8]));
#else
    (void)buf;
    (void)len;
#endif
}
#endif

static s8 ShipProtocol_SendFrame(u8 channel, u8 cmd, const u8 *payload, u8 payload_len, u8 log_frame)
{
    u8 *frame;
    u8 idx;
    u8 body_len;
    u8 i;

    if (payload_len > (SHIP_PROTO_MAX_FRAME_LEN - 5U)) {
        return WIRELESS_ERR_PARAM;
    }

    frame = g_ship_tx_frame;
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
        ShipProtocol_LogFrameBrief(SHIP_STAGE_U8("tx send"), channel, frame, idx);
    }

    return Wireless_SendOnChannel(channel, frame, idx);
}

static void ShipProtocol_CalcDefaultRf(u8 *channel, u8 *key0, u8 *key1)
{
    u8 seed[4];

    ShipProtocol_GetPairSeed(seed);

    *key0 =
        (u8)(((u8)((u8)(seed[0] << 4) >> 4)) + ((u8)(seed[3] >> 2) + (u8)(seed[3] % 0x03U)));
    *key1 =
        (u8)(((u8)((u8)(seed[1] << 4) >> 4)) + ((u8)(seed[2] >> 3) + (u8)(seed[0] % 0x06U)));

    *channel = (u8)(((u8)(((seed[3] + 0x06U) % 0x40U) +
                          ((seed[2] >> 3) * 0x08U) +
                          (((seed[1] | seed[0]) % 0x08U) / 2U))) % 0x40U);
}

static u8 ShipProtocol_RefreshDefaultRf(const char *stage, u8 log_mismatch)
{
    u8 channel;
    u8 key0;
    u8 key1;

    ShipProtocol_CalcDefaultRf(&channel, &key0, &key1);
    if ((log_mismatch != 0U) &&
        ((g_ship_rt.rf_channel[0] != channel) ||
         (g_ship_rt.rf_send_key[0] != key0) ||
         (g_ship_rt.rf_send_key[1] != key1))) {
        LOGW(SHIP_TAG,
             "rf cache mismatch stage=%s cache_ch=%u calc_ch=%u cache_key=%u/%u calc_key=%u/%u",
             stage,
             (u16)g_ship_rt.rf_channel[0],
             (u16)channel,
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)key0,
             (u16)key1);
    }

    g_ship_rt.rf_channel[0] = channel;
    g_ship_rt.rf_channel[1] = channel;
    g_ship_rt.rf_channel[2] = (u8)(channel + 0x40U);
    g_ship_rt.rf_send_key[0] = key0;
    g_ship_rt.rf_send_key[1] = key1;

    return channel;
}

static void ShipProtocol_ApplyDefaultRf(void)
{
    (void)ShipProtocol_RefreshDefaultRf(SHIP_REASON_C("default"), 0U);
}

static s8 ShipProtocol_ApplyWorkSyncIdle(u8 log_rxdbg)
{
    u16 reg36;
    u16 reg39;
    s8 rc;

    (void)ShipProtocol_RefreshDefaultRf(SHIP_REASON_C("work-sync"), 1U);
    reg36 = (u16)(((u16)g_ship_rt.rf_send_key[0] << 8) | g_ship_rt.rf_send_key[0]);
    reg39 = (u16)(((u16)g_ship_rt.rf_send_key[1] << 8) | g_ship_rt.rf_send_key[1]);

    rc = Wireless_SetSyncRegsIdle(reg36, reg39);
    if (rc != SUCCESS) {
        return rc;
    }

    g_ship_rt.work_rx_configured = 0U;
    g_ship_rt.work_rx_reopen_ticks = 0U;
    if (log_rxdbg != 0U) {
        ShipProtocol_LogRxDebug(SHIP_STAGE_U8("pair-sync-idle"));
    }

    return SUCCESS;
}

static s8 ShipProtocol_ApplyWorkRx(u8 log_rxdbg)
{
    s8 rc;
    u8 channel;

    channel = ShipProtocol_RefreshDefaultRf(SHIP_REASON_C("work-rx"), 1U);
    rc = Wireless_SetChannel(channel);
    if (rc == SUCCESS) {
        g_ship_rt.work_rx_configured = 1U;
        g_ship_rt.work_rx_reopen_ticks = 0U;
        if (log_rxdbg != 0U) {
            ShipProtocol_LogRxDebug(SHIP_STAGE_U8("work-rx"));
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

static void ShipProtocol_MarkPairedByFrame(u8 cmd, const char *reason)
{
    u32 now_ms;

    now_ms = Task_GetTickMs();
    g_ship_rt.last_proto_rx_ms = now_ms;
    g_ship_rt.rx_idle_warned = 0U;
    g_ship_rt.throttle_recover_done = 0U;
    if (g_ship_rt.remote_online == 0U) {
        g_ship_rt.remote_online = 1U;
        log_info((u8 *)SHIP_TAG,
                 (u8 *)"remote link online by %s cmd=0x%02X",
                 reason,
                 (u16)cmd);
    }

    if (g_ship_rt.paired != 0U) {
        return;
    }

    g_ship_rt.paired = 1U;
    g_ship_rt.pair_left = 0U;
    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.pair_wait_start_ms = 0UL;
    g_ship_rt.pair_rsp_timeout_logged = 0U;
    g_ship_rt.state = SHIP_STATE_WORK_RX;
    g_ship_rt.work_state_logged = 0U;
    log_info((u8 *)SHIP_TAG,
             (u8 *)"pair ok by %s cmd=0x%02X work_rx=%u key=%u/%u",
             reason,
             (u16)cmd,
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1]);
}

#if SHIP_PROTOCOL_DIAG_ENABLE
static void ShipProtocol_LogRxDebug(const u8 *stage)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)stage;
#endif
}

static void ShipProtocol_LogPayloadBrief(const u8 *stage, u8 cmd, const u8 *payload, u8 payload_len)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)stage;
    (void)cmd;
    (void)payload;
    (void)payload_len;
#endif
}

static void ShipProtocol_LogFrameBrief(const u8 *stage, u8 channel, const u8 *frame, u8 frame_len)
{
#if SHIP_PROTOCOL_DIAG_ENABLE
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
#else
    (void)stage;
    (void)channel;
    (void)frame;
    (void)frame_len;
#endif
}
#endif

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

static s8 ShipProtocol_ArmPairRspWindow(u8 log_rxdbg)
{
    s8 rc;

    rc = ShipProtocol_ApplyWorkSyncIdle(log_rxdbg);
    if (rc != SUCCESS) {
        return rc;
    }

    g_ship_rt.pair_wait_rsp_time = SHIP_PAIR_WAIT_RSP_TICKS;
    g_ship_rt.pair_wait_start_ms = Task_GetTickMs();
    g_ship_rt.last_proto_rx_ms = g_ship_rt.pair_wait_start_ms;
    g_ship_rt.pair_rsp_timeout_logged = 0U;

    rc = ShipProtocol_ApplyWorkRx(log_rxdbg);
    if (rc != SUCCESS) {
        return rc;
    }

    return SUCCESS;
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
    u16 lon_coord1;
    u16 lon_coord2;
    u16 lat_coord1;
    u16 lat_coord2;
    u8 sat_report;
    s8 rc;
    char lon_dir;
    char lat_dir;
    char payload_lon_dir;
    char payload_lat_dir;

    gps = GPS_GetState();
    idx = 0U;

    sat_report = (gps->satellites_used_gsa > 0U) ? gps->satellites_used_gsa : gps->satellites_used;
    if (sat_report > 24U) {
        sat_report = 24U;
    }
    payload[idx++] = sat_report;

    if (MainLoop_IsHeadingReady() != 0U) {
        angle = (u16)((MainLoop_GetHeadingDeg100() / 100U) % 360U);
    } else {
        angle = (u16)((gps->course_deg_x100 / 100U) % 360U);
    }
    ShipProtocol_WriteU16Legacy(&payload[idx], angle);
    idx += 2U;

    if (gps->legacy_coord_valid != 0U) {
        lon_dir = (char)gps->legacy_lon_dir;
        lat_dir = (char)gps->legacy_lat_dir;
        lon_coord1 = gps->legacy_lon1;
        lon_coord2 = gps->legacy_lon2;
        lat_coord1 = gps->legacy_lat1;
        lat_coord2 = gps->legacy_lat2;
        abs_lon = (gps->lon_deg1e7 < 0) ? (u32)(-gps->lon_deg1e7) : (u32)gps->lon_deg1e7;
        abs_lat = (gps->lat_deg1e7 < 0) ? (u32)(-gps->lat_deg1e7) : (u32)gps->lat_deg1e7;
    } else {
        if (gps->lon_deg1e7 < 0) {
            lon_dir = 'W';
            abs_lon = (u32)(-gps->lon_deg1e7);
        } else {
            lon_dir = 'E';
            abs_lon = (u32)gps->lon_deg1e7;
        }
        ShipProtocol_ToLegacyNmeaCoord(abs_lon, &lon_coord1, &lon_coord2);

        if (gps->lat_deg1e7 < 0) {
            lat_dir = 'S';
            abs_lat = (u32)(-gps->lat_deg1e7);
        } else {
            lat_dir = 'N';
            abs_lat = (u32)gps->lat_deg1e7;
        }
        ShipProtocol_ToLegacyNmeaCoord(abs_lat, &lat_coord1, &lat_coord2);
    }

    payload_lon_dir = lon_dir;
    payload_lat_dir = lat_dir;

    payload[idx++] = (u8)payload_lon_dir;
    ShipProtocol_WriteU16Legacy(&payload[idx], lon_coord1);
    idx += 2U;
    ShipProtocol_WriteU16Legacy(&payload[idx], lon_coord2);
    idx += 2U;

    payload[idx++] = (u8)payload_lat_dir;
    ShipProtocol_WriteU16Legacy(&payload[idx], lat_coord1);
    idx += 2U;
    ShipProtocol_WriteU16Legacy(&payload[idx], lat_coord2);
    idx += 2U;

    power = g_ship_power_sample;
    power.report = g_ship_power_level;
    payload[idx++] = g_ship_power_level;
    payload[idx++] = AutoDrive_InActive();

    if (idx != 15U) {
        LOGE(SHIP_TAG, "gps payload len bad=%u", (u16)idx);
        return;
    }

    ShipProtocol_LogPowerSample(&power, 0U);
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
             "gps state fix=%u legacy=%u sat=%u lon=%c%lu lat=%c%lu angle=%u power=0x%02X seq=%lu",
             (u16)gps->fix_valid,
             (u16)gps->legacy_coord_valid,
             (u16)sat_report,
             lon_dir,
             (u32)abs_lon,
             lat_dir,
             (u32)abs_lat,
             (u16)angle,
             (u16)payload[13],
             (u32)gps->update_sequence);
        LOGI(SHIP_TAG,
             "gps sat source gsa=%u gga=%u report=%u",
             (u16)gps->satellites_used_gsa,
             (u16)gps->satellites_used,
             (u16)sat_report);
        LOGI(SHIP_TAG,
             "gps payload oldfmt ew=%c lon1=%u lon2=%u ns=%c lat1=%u lat2=%u",
             payload_lon_dir,
             (u16)lon_coord1,
             (u16)lon_coord2,
             payload_lat_dir,
             (u16)lat_coord1,
             (u16)lat_coord2);
        LOGI(SHIP_TAG,
             "gps payload bytes=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             (u16)payload[0],
             (u16)payload[1],
             (u16)payload[2],
             (u16)payload[3],
             (u16)payload[4],
             (u16)payload[5],
             (u16)payload[6],
             (u16)payload[7],
             (u16)payload[8],
             (u16)payload[9],
             (u16)payload[10],
             (u16)payload[11],
             (u16)payload[12],
             (u16)payload[13],
             (u16)payload[14]);
        ShipProtocol_LogPayloadBrief(SHIP_STAGE_U8("tx frame"), SHIP_CMD_GPS_REPORT, payload, idx);
    }

    rc = ShipProtocol_SendFrame(g_ship_rt.rf_channel[0], SHIP_CMD_GPS_REPORT, payload, idx, log_this_tx);
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "gps tx fail rc=%d ch=0x%02X", rc, (u16)g_ship_rt.rf_channel[0]);
    }
    if (g_ship_rt.paired != 0U) {
        ShipProtocol_ReopenWorkRx(SHIP_REASON_C("gps report tx"), 0U, log_this_tx);
    } else {
        g_ship_rt.work_rx_configured = 0U;
    }
}

static void ShipProtocol_HandlePairRsp(const u8 *payload, u8 payload_len)
{
    if (g_ship_rt.pair_wait_rsp_time == 0U) {
        return;
    }

    ShipProtocol_LogPayloadBrief(SHIP_STAGE_U8("pair rsp rx"), SHIP_CMD_PAIR_RSP, payload, payload_len);
    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.pair_wait_start_ms = 0UL;
    g_ship_rt.last_proto_rx_ms = Task_GetTickMs();
    g_ship_rt.paired = 1U;
    g_ship_rt.pair_left = 0U;
    g_ship_rt.wait_ticks = 0U;
    g_ship_rt.state = SHIP_STATE_WORK_RX;
    g_ship_rt.work_rx_configured = 1U;
    g_ship_rt.work_state_logged = 0U;
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
    g_ship_rt.throttle_recover_done = 0U;
    log_this_sample = ShipProtocol_ShouldLogManualSample(payload[0], payload[1], payload[2], now_ms);
    if (g_ship_rt.throttle_online == 0U) {
        g_ship_rt.throttle_online = 1U;
        log_info((u8 *)SHIP_TAG, (u8 *)"manual control online by cmd=0x11");
    }

    if (log_this_sample != 0U) {
        if (SHIP_RC_INPUT_LOG_PERIOD_MS != 0U) {
            static u32 last_rc_log_ms = 0UL;
            u32 now_ms = Task_GetTickMs();
            if ((now_ms - last_rc_log_ms) >= SHIP_RC_INPUT_LOG_PERIOD_MS) {
                last_rc_log_ms = now_ms;
                log_info((u8 *)SHIP_TAG,
                         (u8 *)"rc input cmd=0x11 raw_ud=%u raw_lr=%u throttle_val=%d steering_val=%d key=0x%02X(%s)",
                         (u16)g_ship_rt.ud,
                         (u16)g_ship_rt.lr,
                         (int16)g_ship_rt.ud - (int16)SHIP_AXIS_CENTER,
                         (int16)g_ship_rt.lr - (int16)SHIP_AXIS_CENTER,
                         (u16)g_ship_rt.key,
                         ShipProtocol_KeyNameAlways(g_ship_rt.key));
            }
        } else {
            log_info((u8 *)SHIP_TAG,
                     (u8 *)"rc input cmd=0x11 raw_ud=%u raw_lr=%u throttle_val=%d steering_val=%d key=0x%02X(%s)",
                     (u16)g_ship_rt.ud,
                     (u16)g_ship_rt.lr,
                     (int16)g_ship_rt.ud - (int16)SHIP_AXIS_CENTER,
                     (int16)g_ship_rt.lr - (int16)SHIP_AXIS_CENTER,
                     (u16)g_ship_rt.key,
                     ShipProtocol_KeyNameAlways(g_ship_rt.key));
        }
        log_info((u8 *)SHIP_TAG,
                 (u8 *)"rc cmd=0x11 lr=%u ud=%u key=0x%02X(%s) paired=%u",
                 (u16)g_ship_rt.lr,
                 (u16)g_ship_rt.ud,
                 (u16)g_ship_rt.key,
                 ShipProtocol_KeyNameAlways(g_ship_rt.key),
                 (u16)g_ship_rt.paired);
        log_info((u8 *)SHIP_TAG,
                 (u8 *)"throttle_raw=%u steering_raw=%u throttle_val=%d steering_val=%d key=0x%02X(%s)",
                 (u16)g_ship_rt.ud,
                 (u16)g_ship_rt.lr,
                 (int16)g_ship_rt.ud - (int16)SHIP_AXIS_CENTER,
                 (int16)g_ship_rt.lr - (int16)SHIP_AXIS_CENTER,
                 (u16)g_ship_rt.key,
                 ShipProtocol_KeyNameAlways(g_ship_rt.key));
    }
    if (AutoDrive_IsBusy() != 0U) {
        ShipProtocol_HandleKey(g_ship_rt.ud, g_ship_rt.key);
        AutoDrive_LinkAliveKick();
        return log_this_sample;
    }
    ShipProtocol_UpdateManualAcceleratorRaw(payload[0], payload[1]);
    if (g_ship_rt.pulse_active == 0U) {
        ShipProtocol_ApplyManualControl(g_ship_rt.lr, g_ship_rt.ud, log_this_sample);
    }
    ShipProtocol_HandleKey(g_ship_rt.ud, g_ship_rt.key);
    AutoDrive_LinkAliveKick();
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
        if (payload_len < AUTODRIVE_LEGACY_POINT_WIRE_LEN) {
            break;
        }
        LOGI(SHIP_TAG, "cmd=0x13 return-home rx len=%u", (u16)payload_len);
        ShipProtocol_LogCoordBE(payload, payload_len);
        AutoDrive_SetReturnPositionRaw(payload);
        break;
    case SHIP_CMD_GOTO_POINT:
        if (payload_len < AUTODRIVE_LEGACY_POINT_WIRE_LEN) {
            break;
        }
        LOGI(SHIP_TAG, "cmd=0x14 goto-point rx len=%u", (u16)payload_len);
        ShipProtocol_LogCoordBE(payload, payload_len);
        AutoDrive_SetFishPositionRaw(payload);
        break;
    case SHIP_CMD_RETURN_SWITCH:
        if (payload_len < 1U) {
            LOGW(SHIP_TAG, "return-switch short len=%u", (u16)payload_len);
            break;
        }
        LOGI(SHIP_TAG, "cmd=0x15 return-switch rx len=%u state=%u",
             (u16)payload_len,
             (u16)payload[0]);
        if (payload_len >= (u8)(1U + AUTODRIVE_LEGACY_POINT_WIRE_LEN)) {
            ShipProtocol_LogCoordBE(&payload[1], (u8)(payload_len - 1U));
        }
        AutoDrive_SetSwitchRaw(payload, payload_len);
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
        log_warn((u8 *)SHIP_TAG,
                 (u8 *)"aa-bb xor bad cmd=0x%02X len=%u calc=0x%02X recv=0x%02X",
                 (u16)frame[2],
                 (u16)body_len,
                 (u16)xor_calc,
                 (u16)xor_recv);
        return WIRELESS_ERR_VERIFY;
    }

    cmd = frame[2];
    data_len = (u8)(body_len - 2U);
    SHIP_PROTO_DBG("frame ok cmd=0x%02X len=%u data_len=%u xor=0x%02X",
                   (u16)cmd,
                   (u16)frame_len,
                   (u16)data_len,
                   (u16)xor_recv);
    ShipProtocol_MarkPairedByFrame(cmd, "valid-frame");
    if (cmd != SHIP_CMD_THROTTLE) {
        log_info((u8 *)SHIP_TAG,
                 (u8 *)"aa-bb xor ok cmd=0x%02X data_len=%u",
                 (u16)cmd,
                 (u16)data_len);
    }
    if (cmd != SHIP_CMD_THROTTLE) {
        ShipProtocol_LogPayloadBrief(SHIP_STAGE_U8("rx frame ok"), cmd, &frame[3], data_len);
    }
    ShipProtocol_Dispatch(cmd, &frame[3], data_len);
    return SUCCESS;
}

static void ShipProtocol_ReceiveHandle(const u8 *rx_buf, u8 len)
{
    u8 i;
    u8 *frame;
    u8 frame_index;
    u8 frame_left;
    u8 frame_finish;
    u8 check_ok;
    u8 check_sum;

    if (rx_buf == 0) {
        return;
    }

    frame = g_ship_parse_frame;
    SHIP_PROTO_DBG("rx raw len=%u b0=%02X b1=%02X b2=%02X b3=%02X b4=%02X b5=%02X b6=%02X b7=%02X",
                   (u16)len,
                   (u16)((len > 0U) ? rx_buf[0] : 0U),
                   (u16)((len > 1U) ? rx_buf[1] : 0U),
                   (u16)((len > 2U) ? rx_buf[2] : 0U),
                   (u16)((len > 3U) ? rx_buf[3] : 0U),
                   (u16)((len > 4U) ? rx_buf[4] : 0U),
                   (u16)((len > 5U) ? rx_buf[5] : 0U),
                   (u16)((len > 6U) ? rx_buf[6] : 0U),
                   (u16)((len > 7U) ? rx_buf[7] : 0U));

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
            SHIP_PROTO_DBG("frame build len=%u head=0x%02X tail=0x%02X lenfield=%u",
                           (u16)frame_index,
                           (u16)frame[0],
                           (u16)frame[frame_index - 1U],
                           (u16)frame[1]);
            check_sum = ShipProtocol_Xor(&frame[1], (u8)(frame_index - 3U));
            if ((frame_index >= 5U) &&
                (frame[1] >= 2U) &&
                (frame[frame_index - 1U] == SHIP_PROTO_TAIL)) {
                ShipProtocol_MarkPairedByFrame(frame[2], "aa-bb-frame");
            }
            if ((check_sum == frame[frame_index - 2U]) &&
                (frame[frame_index - 1U] == SHIP_PROTO_TAIL)) {
                (void)ShipProtocol_ParseFrame(frame, frame_index);
            } else {
                SHIP_PROTO_DBG("frame drop len=%u calc=0x%02X recv=0x%02X tail=0x%02X",
                               (u16)frame_index,
                               (u16)check_sum,
                               (u16)frame[frame_index - 2U],
                               (u16)frame[frame_index - 1U]);
            }
            frame_index = 0U;
            frame_left = 0U;
        }
    }
}

void ShipProtocol_Poll(void)
{
    u8 *frame;
    u8 frame_len;
    s8 rc;

    frame = g_ship_rx_frame;
    do {
        frame_len = 0U;
        rc = Wireless_Receive(frame, SHIP_PROTO_MAX_FRAME_LEN, &frame_len);
        if (rc == SUCCESS) {
            SHIP_PROTO_DBG("rx pop len=%u", (u16)frame_len);
            ShipProtocol_ReceiveHandle(frame, frame_len);
        }
    } while (rc == SUCCESS);
}

static void ShipProtocol_PollRxFrames(void)
{
    u8 *frame;
    u8 frame_len;
    s8 rc;

    frame = g_ship_rx_frame;
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
    ShipProtocol_ResetAxisFilter();
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
    g_ship_rt.remote_online = 0U;
    g_ship_rt.throttle_online = 0U;
    g_ship_rt.throttle_recover_done = 0U;
    g_ship_rt.motor_initialized = 0U;
    g_ship_rt.yaw_hold_active = 0U;
    g_ship_rt.yaw_hold_target_cd = 0;
    g_ship_rt.yaw_hold_output = 0;
    g_ship_rt.yaw_hold_last_update_ms = 0UL;
    g_ship_rt.motion = SHIP_MOTION_STOP;
    g_ship_rt.pulse_motion = SHIP_MOTION_STOP;
    g_ship_rt.pulse_expire_ms = 0UL;
    g_ship_rt.pulse_active = 0U;
    g_ship_rt.center_stop_count = 0U;
    g_ship_power_sample_times = 0U;
    g_lowpower_check_times = 0U;
    g_now_pwm_accelerator = 0U;
    AutoDrive_Init();

#if SHIP_YAW_HOLD_ENABLE
    PID_Init(&g_ship_yaw_pid,
             SHIP_YAW_HOLD_KP_Q10,
             SHIP_YAW_HOLD_KI_Q10,
             SHIP_YAW_HOLD_KD_Q10,
             -SHIP_YAW_HOLD_OUTPUT_LIMIT,
             SHIP_YAW_HOLD_OUTPUT_LIMIT,
             -(int32)(SHIP_YAW_HOLD_OUTPUT_LIMIT * 64),
             (int32)(SHIP_YAW_HOLD_OUTPUT_LIMIT * 64));
#endif

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

    if (g_ship_rt.paired != 0U) {
        g_ship_rt.pair_left = 0U;
        g_ship_rt.state = SHIP_STATE_WORK_RX;
        return;
    }

    if (g_ship_rt.pair_left > 0U) {
        g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
        left_after_send = (u16)(g_ship_rt.pair_left - 1U);
        rc = ShipProtocol_TryPairSend(left_after_send);
        if (rc != SUCCESS) {
            return;
        }
        g_ship_rt.pair_left = left_after_send;

        if (g_ship_rt.pair_left == 0U) {
            rc = ShipProtocol_ArmPairRspWindow(1U);
            if (rc != SUCCESS) {
                LOGE(SHIP_TAG, "pair rsp window arm fail rc=%d", rc);
                g_ship_rt.pair_retry_count++;
                g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
                g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
                g_ship_rt.pair_wait_rsp_time = 0U;
                g_ship_rt.pair_wait_start_ms = 0UL;
                return;
            }
            g_ship_rt.state = SHIP_STATE_WORK_RX;
            LOGI(SHIP_TAG, "pair req burst done, enter rsp wait on work-rx");
        } else {
            LOGI(SHIP_TAG, "pair req sent seq_left=%u wait=%u",
                 (u16)g_ship_rt.pair_left,
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
    AutoDrive_LinkAliveTick();
    ShipProtocol_LowPowerCheck();

    if (g_ship_rt.pair_wait_rsp_time > 0U) {
        g_ship_rt.pair_wait_rsp_time--;
    } else if ((g_ship_rt.pair_wait_start_ms != 0UL) &&
               (g_ship_rt.pair_rsp_timeout_logged == 0U) &&
               (ShipProtocol_ElapsedMs(now_ms, g_ship_rt.pair_wait_start_ms) >= SHIP_PAIR_RSP_EXPIRE_LOG_MS) &&
               (g_ship_rt.paired == 0U)) {
        g_ship_rt.pair_rsp_timeout_logged = 1U;
        LOGW(SHIP_TAG, "pair rsp window expired, no rsp");
        ShipProtocol_LogRxDebug(SHIP_STAGE_U8("pair-rsp-expired"));
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
    {
        u32 rx_silence_ms;

        rx_silence_ms = ShipProtocol_ElapsedMs(now_ms, g_ship_rt.last_proto_rx_ms);

        if (g_ship_rt.state == SHIP_STATE_WORK_RX) {
            if ((g_ship_rt.last_proto_rx_ms != 0UL) &&
                (g_ship_rt.rx_idle_warned == 0U) &&
                (rx_silence_ms >= SHIP_RX_IDLE_WARN_MS)) {
                g_ship_rt.rx_idle_warned = 1U;
                LOGW(SHIP_TAG, "work-rx idle %lums, no aa-bb frame", rx_silence_ms);
                ShipProtocol_LogRxDebug(SHIP_STAGE_U8("rx-idle"));
            }

            if ((g_ship_rt.remote_online != 0U) &&
                (g_ship_rt.last_proto_rx_ms != 0UL) &&
                (rx_silence_ms >= SHIP_THROTTLE_TIMEOUT_MS)) {
                g_ship_rt.remote_online = 0U;
                if (g_ship_rt.throttle_online != 0U) {
                    g_ship_rt.throttle_online = 0U;
                    ShipProtocol_ResetAxisFilter();
                    ShipProtocol_StopMotion(SHIP_REASON_U8("remote link timeout"), 1U);
                }
                LOGW(SHIP_TAG, "remote link timeout by aa-bb-frame, dt=%lums",
                     rx_silence_ms);
                ShipProtocol_LogRxDebug(SHIP_STAGE_U8("remote-timeout"));
            }

            if ((g_ship_rt.throttle_online != 0U) &&
                (g_ship_rt.last_throttle_rx_ms != 0UL) &&
                ((now_ms - g_ship_rt.last_throttle_rx_ms) >= SHIP_THROTTLE_TIMEOUT_MS)) {
                g_ship_rt.throttle_online = 0U;
                ShipProtocol_ResetAxisFilter();
                LOGW(SHIP_TAG, "manual control timeout by cmd=0x11, dt=%lums",
                     (u32)(now_ms - g_ship_rt.last_throttle_rx_ms));
                ShipProtocol_StopMotion(SHIP_REASON_U8("manual timeout"), 1U);
                ShipProtocol_LogRxDebug(SHIP_STAGE_U8("throttle-timeout"));
            }

            if ((g_ship_rt.paired != 0U) &&
                (g_ship_rt.throttle_recover_done == 0U) &&
                (g_ship_rt.last_proto_rx_ms != 0UL) &&
                (rx_silence_ms >= SHIP_THROTTLE_RECOVER_MS)) {
                g_ship_rt.throttle_recover_done = 1U;
                ShipProtocol_ApplyDefaultRf();
                g_ship_rt.work_rx_reopen_ticks = 0U;
                LOGW(SHIP_TAG,
                     "legacy remote recovery after %lums link silence, re-open work-rx",
                     rx_silence_ms);
                ShipProtocol_ReopenWorkRx(SHIP_REASON_C("legacy remote recovery"), 1U, 1U);
            }

            if ((g_ship_rt.throttle_online != 0U) &&
                (g_ship_rt.valid != 0U) &&
                (g_ship_rt.pulse_active == 0U) &&
                (AutoDrive_IsBusy() == 0U)) {
                ShipProtocol_ApplyManualControl(g_ship_rt.lr, g_ship_rt.ud, 0U);
            }
            ShipProtocol_LogPowerSample(&g_ship_power_sample, 0U);
        }

    }

    AutoDrive_Poll();

#if SHIP_YAW_HOLD_ENABLE
    if (((g_ship_rt.throttle_online == 0U) || (g_ship_rt.valid == 0U)) &&
        (AutoDrive_IsBusy() == 0U)) {
        ShipProtocol_ServiceIdleYawHold(now_ms);
    }
#endif
}

u8 ShipProtocol_IsPaired(void)
{
    return g_ship_rt.paired;
}
