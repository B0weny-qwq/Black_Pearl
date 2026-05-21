/**
 * @file    ShipControl.c
 * @brief   Unified manual, cruise, GPS-nav yaw-hold motor control.
 */

#include "ShipControl.h"

#include "..\Motor\Motor.h"
#include "..\..\Function\PID\PID.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\..\User\MainLoop.h"
#include "..\..\..\User\Task.h"

#define SHIP_CONTROL_TAG                 "CTRL"
#define SHIP_AXIS_CENTER                 100U
#define SHIP_LR_DEAD_LOW                 90U
#define SHIP_LR_DEAD_HIGH                110U
#define SHIP_FB_DEAD_LOW                 90U
#define SHIP_FB_DEAD_HIGH                110U
#define SHIP_TURN_COMPARE_BIAS           5U
#define SHIP_THROTTLE_DEADBAND           4
#define SHIP_STEERING_DEADBAND           8
#define SHIP_THROTTLE_MIN_COMMAND        180
#define SHIP_THROTTLE_MAX_COMMAND        850
#define SHIP_STEERING_MAX_COMMAND        700
#define SHIP_CENTER_STOP_CONFIRM_FRAMES  2U
#define SHIP_YAW_HOLD_FORWARD_ONLY       1U
#define SHIP_CRUISE_BASE_SPEED           SHIP_THROTTLE_MAX_COMMAND

#ifndef SHIP_AXIS_FILTER_SHIFT
#define SHIP_AXIS_FILTER_SHIFT           1U
#endif
#ifndef SHIP_MANUAL_CONTROL_PERIOD_MS
#define SHIP_MANUAL_CONTROL_PERIOD_MS    10UL
#endif
#ifndef SHIP_YAW_HOLD_LOG_PERIOD_MS
#define SHIP_YAW_HOLD_LOG_PERIOD_MS      1000UL
#endif
#ifndef SHIP_MANUAL_GATE_LOG_PERIOD_MS
#define SHIP_MANUAL_GATE_LOG_PERIOD_MS   300UL
#endif
#ifndef SHIP_YAW_HOLD_PERIOD_MS
#define SHIP_YAW_HOLD_PERIOD_MS          50UL
#endif
#ifndef SHIP_YAW_HOLD_FULL_ERROR_CD
#define SHIP_YAW_HOLD_FULL_ERROR_CD      1000
#endif
#ifndef SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE
#define SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE 400
#endif
#ifndef SHIP_MANUAL_YAW_HOLD_DIFF_PERCENT
#define SHIP_MANUAL_YAW_HOLD_DIFF_PERCENT 20U
#endif
#ifndef SHIP_YAW_HOLD_OUTPUT_SIGN
#define SHIP_YAW_HOLD_OUTPUT_SIGN        1
#endif
#ifndef SHIP_YAW_HOLD_DERATE_ENABLE
#define SHIP_YAW_HOLD_DERATE_ENABLE      1
#endif
#ifndef SHIP_YAW_HOLD_DERATE_START_CD
#define SHIP_YAW_HOLD_DERATE_START_CD    300
#endif
#ifndef SHIP_YAW_HOLD_DERATE_FULL_CD
#define SHIP_YAW_HOLD_DERATE_FULL_CD     1000
#endif
#ifndef SHIP_YAW_HOLD_DERATE_MIN_BASE
#define SHIP_YAW_HOLD_DERATE_MIN_BASE    500
#endif
#ifndef SHIP_YAW_HOLD_GYRO_DAMP_Q10
#define SHIP_YAW_HOLD_GYRO_DAMP_Q10      3072
#endif
#ifndef SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP
#define SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP 30
#endif
#ifndef SHIP_YAW_HOLD_STEER_STABLE_FRAMES
#define SHIP_YAW_HOLD_STEER_STABLE_FRAMES 2U
#endif
#ifndef SHIP_YAW_HOLD_OUTPUT_LIMIT
#define SHIP_YAW_HOLD_OUTPUT_LIMIT       1000
#endif
#ifndef SHIP_YAW_HOLD_DEADBAND_CD
#define SHIP_YAW_HOLD_DEADBAND_CD        50
#endif
#ifndef SHIP_YAW_HOLD_KP_Q10
#define SHIP_YAW_HOLD_KP_Q10             768
#endif
#ifndef SHIP_YAW_HOLD_KI_Q10
#define SHIP_YAW_HOLD_KI_Q10             0
#endif
#ifndef SHIP_YAW_HOLD_KD_Q10
#define SHIP_YAW_HOLD_KD_Q10             0
#endif

#define SHIP_CONTROL_REASON_MANUAL_OPEN  20U
#define SHIP_CONTROL_REASON_MANUAL_YAW   21U
#define SHIP_CONTROL_REASON_CRUISE       22U
#define SHIP_CONTROL_REASON_GPS_NAV      23U

#define SHIP_CTRL_GATE_INVALID           0xFFU
#define SHIP_CTRL_GATE_CENTER            0U
#define SHIP_CTRL_GATE_WAIT_STABLE       1U
#define SHIP_CTRL_GATE_READY             2U
#define SHIP_CTRL_GATE_DIFF              3U
#define SHIP_CTRL_GATE_THROTTLE          4U
#define SHIP_CTRL_GATE_HEADING_LOST      5U
#define SHIP_CTRL_GATE_NO_INPUT          6U

typedef enum
{
    SHIP_CONTROL_MOTION_STOP = 0,
    SHIP_CONTROL_MOTION_FORWARD,
    SHIP_CONTROL_MOTION_BACKWARD,
    SHIP_CONTROL_MOTION_LEFT,
    SHIP_CONTROL_MOTION_RIGHT
} ShipControl_Motion_t;

typedef struct
{
    u8 initialized;
    u8 motor_initialized;
    u8 mode;
    u8 lr;
    u8 ud;
    u8 key;
    u8 manual_valid;
    u8 manual_accelerator;
    int32 filtered_lr_q8;
    int32 filtered_ud_q8;
    u32 manual_last_apply_ms;
    u32 auto_last_apply_ms;
    u32 manual_last_log_ms;
    u8 center_stop_count;
    u8 yaw_hold_active;
    u16 yaw_hold_target_cd;
    int16 yaw_hold_error_cd;
    int16 yaw_hold_error_ctrl;
    int16 yaw_hold_output;
    int16 yaw_hold_last_yaw_speed;
    u32 yaw_hold_last_update_ms;
    u8 yaw_hold_stable_count;
    int16 left_speed;
    int16 right_speed;
    int16 throttle_speed;
    int16 base_speed;
    int16 steering_speed;
    int16 yaw_diff_speed;
    int16 manual_gate_diff;
    int16 manual_gate_limit;
    u8 manual_gate_state;
    u32 manual_gate_last_log_ms;
    u8 last_logged_mode;
    ShipControl_Motion_t motion;
} ShipControl_Runtime_t;

static ShipControl_Runtime_t xdata g_ship_ctrl;
static PID_Controller_t xdata g_ship_ctrl_yaw_pid;

static void ShipControl_EnsureMotorInit(void);
static void ShipControl_ResetAxisFilter(void);
static u8 ShipControl_ConfirmCenterStop(void);
static u8 ShipControl_AbsAxisDiff(u8 value);
static int16 ShipControl_AbsSpeed(int16 speed);
static int16 ShipControl_LimitSpeed(int16 speed);
static int16 ShipControl_WrapSignedCd(int32 angle_cd);
static u16 ShipControl_WrapUnsignedCd(int32 angle_cd);
static int16 ShipControl_YawErrorToControl(int16 yaw_error_cd);
static int16 ShipControl_ApplyYawHoldDamping(int16 yaw_control);
static int16 ShipControl_ApplyYawOutputSlew(int16 yaw_output);
static int16 ShipControl_ApplyYawHoldBaseDerate(int16 base_speed, int16 yaw_error_cd);
static int16 ShipControl_YawControlToSpeed(int16 yaw_control, int16 base_speed);
static u8 ShipControl_YawHoldGateStable(void);
static int16 ShipControl_FilterAxis(u8 raw, int32 *state_q8);
static int16 ShipControl_ApplyAxisCurve(int16 value,
                                        int16 deadband,
                                        int16 min_command,
                                        int16 max_command);
static int16 ShipControl_ThrottleToSignedSpeed(int16 value);
static int16 ShipControl_SteeringToSignedSpeed(int16 value);
static void ShipControl_UpdateManualAcceleratorRaw(u8 left_right, u8 front_back);
static void ShipControl_SetMotorTargets(int16 left_speed, int16 right_speed);
static void ShipControl_ApplyOpenLoop(ShipControl_Motion_t motion,
                                      int16 left_speed,
                                      int16 right_speed,
                                      int16 throttle_speed,
                                      int16 steering_speed);
static u8 ShipControl_ApplyYawHoldTarget(u16 target_heading_cd,
                                         int16 base_speed,
                                         u8 mode);
static void ShipControl_ApplyManualControl(void);
static void ShipControl_LogSample(u32 now_ms);
static void ShipControl_LogModeEvent(u8 old_mode, u8 new_mode, u8 reason);
static void ShipControl_LogManualGate(u8 state,
                                      int16 throttle_speed,
                                      int16 steering_speed,
                                      int16 left_speed,
                                      int16 right_speed,
                                      int16 diff,
                                      int16 gate);
static void ShipControl_SetMode(u8 mode, u8 reason);

void ShipControl_Init(void)
{
    g_ship_ctrl.initialized = 1U;
    g_ship_ctrl.motor_initialized = 0U;
    g_ship_ctrl.mode = SHIP_CONTROL_MODE_STOP;
    g_ship_ctrl.lr = SHIP_AXIS_CENTER;
    g_ship_ctrl.ud = SHIP_AXIS_CENTER;
    g_ship_ctrl.key = 0U;
    g_ship_ctrl.manual_valid = 0U;
    g_ship_ctrl.manual_accelerator = 0U;
    ShipControl_ResetAxisFilter();
    g_ship_ctrl.manual_last_apply_ms = 0UL;
    g_ship_ctrl.auto_last_apply_ms = 0UL;
    g_ship_ctrl.manual_last_log_ms = 0UL;
    g_ship_ctrl.center_stop_count = 0U;
    g_ship_ctrl.yaw_hold_active = 0U;
    g_ship_ctrl.yaw_hold_target_cd = 0U;
    g_ship_ctrl.yaw_hold_error_cd = 0;
    g_ship_ctrl.yaw_hold_error_ctrl = 0;
    g_ship_ctrl.yaw_hold_output = 0;
    g_ship_ctrl.yaw_hold_last_yaw_speed = 0;
    g_ship_ctrl.yaw_hold_last_update_ms = 0UL;
    g_ship_ctrl.yaw_hold_stable_count = 0U;
    g_ship_ctrl.left_speed = 0;
    g_ship_ctrl.right_speed = 0;
    g_ship_ctrl.throttle_speed = 0;
    g_ship_ctrl.base_speed = 0;
    g_ship_ctrl.steering_speed = 0;
    g_ship_ctrl.yaw_diff_speed = 0;
    g_ship_ctrl.manual_gate_diff = 0;
    g_ship_ctrl.manual_gate_limit = 0;
    g_ship_ctrl.manual_gate_state = SHIP_CTRL_GATE_INVALID;
    g_ship_ctrl.manual_gate_last_log_ms = 0UL;
    g_ship_ctrl.last_logged_mode = SHIP_CONTROL_MODE_STOP;
    g_ship_ctrl.motion = SHIP_CONTROL_MOTION_STOP;

    PID_Init(&g_ship_ctrl_yaw_pid,
             SHIP_YAW_HOLD_KP_Q10,
             SHIP_YAW_HOLD_KI_Q10,
             SHIP_YAW_HOLD_KD_Q10,
             -SHIP_YAW_HOLD_OUTPUT_LIMIT,
             SHIP_YAW_HOLD_OUTPUT_LIMIT,
             -((int32)SHIP_YAW_HOLD_OUTPUT_LIMIT * 64L),
             ((int32)SHIP_YAW_HOLD_OUTPUT_LIMIT * 64L));
}

void ShipControl_Tick(u32 now_ms)
{
    if (g_ship_ctrl.initialized == 0U) {
        ShipControl_Init();
    }

    if ((g_ship_ctrl.manual_valid != 0U) &&
        ((g_ship_ctrl.mode == SHIP_CONTROL_MODE_STOP) ||
         (g_ship_ctrl.mode == SHIP_CONTROL_MODE_MANUAL_OPEN_LOOP) ||
         (g_ship_ctrl.mode == SHIP_CONTROL_MODE_MANUAL_YAW_HOLD))) {
        if ((now_ms - g_ship_ctrl.manual_last_apply_ms) >= SHIP_MANUAL_CONTROL_PERIOD_MS) {
            g_ship_ctrl.manual_last_apply_ms = now_ms;
            ShipControl_UpdateManualAcceleratorRaw(g_ship_ctrl.lr, g_ship_ctrl.ud);
            ShipControl_ApplyManualControl();
        }
    } else {
        g_ship_ctrl.center_stop_count = 0U;
    }

    if ((g_ship_ctrl.mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) &&
        ((now_ms - g_ship_ctrl.auto_last_apply_ms) >= SHIP_MANUAL_CONTROL_PERIOD_MS)) {
        g_ship_ctrl.auto_last_apply_ms = now_ms;
        (void)ShipControl_ApplyYawHoldTarget(g_ship_ctrl.yaw_hold_target_cd,
                                             g_ship_ctrl.base_speed,
                                             SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD);
    }

    ShipControl_LogSample(now_ms);
}

void ShipControl_UpdateManualInput(u8 lr, u8 ud, u8 key, u32 now_ms)
{
    if (g_ship_ctrl.initialized == 0U) {
        ShipControl_Init();
    }

    g_ship_ctrl.lr = lr;
    g_ship_ctrl.ud = ud;
    g_ship_ctrl.key = key;
    g_ship_ctrl.manual_valid = 1U;
    ShipControl_UpdateManualAcceleratorRaw(lr, ud);
    g_ship_ctrl.manual_last_apply_ms = now_ms;

    if ((g_ship_ctrl.mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) ||
        (g_ship_ctrl.mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD)) {
        return;
    }

    ShipControl_ApplyManualControl();
}

void ShipControl_RequestCruise(u16 heading_cd, int16 base_speed)
{
    if (g_ship_ctrl.initialized == 0U) {
        ShipControl_Init();
    }

    if (MainLoop_IsHeadingReady() == 0U) {
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_HEADING_LOST);
        return;
    }

    ShipControl_ResetYawHoldController();
    g_ship_ctrl.auto_last_apply_ms = Task_GetTickMs();
    g_ship_ctrl.yaw_hold_target_cd = ShipControl_WrapUnsignedCd((int32)heading_cd);
    g_ship_ctrl.base_speed = ShipControl_LimitSpeed(base_speed);
    if (g_ship_ctrl.base_speed == 0) {
        g_ship_ctrl.base_speed = SHIP_CRUISE_BASE_SPEED;
    } else if (g_ship_ctrl.base_speed > SHIP_CRUISE_BASE_SPEED) {
        g_ship_ctrl.base_speed = SHIP_CRUISE_BASE_SPEED;
    } else if (g_ship_ctrl.base_speed < -SHIP_CRUISE_BASE_SPEED) {
        g_ship_ctrl.base_speed = -SHIP_CRUISE_BASE_SPEED;
    }
    (void)ShipControl_ApplyYawHoldTarget(g_ship_ctrl.yaw_hold_target_cd,
                                         g_ship_ctrl.base_speed,
                                         SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD);
}

void ShipControl_RequestGpsNav(u16 target_heading_cd, int16 base_speed)
{
    if (g_ship_ctrl.initialized == 0U) {
        ShipControl_Init();
    }

    if (MainLoop_IsHeadingReady() == 0U) {
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_HEADING_LOST);
        return;
    }

    if (g_ship_ctrl.mode != SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD) {
        ShipControl_ResetYawHoldController();
    }
    g_ship_ctrl.auto_last_apply_ms = Task_GetTickMs();
    g_ship_ctrl.yaw_hold_target_cd = ShipControl_WrapUnsignedCd((int32)target_heading_cd);
    g_ship_ctrl.base_speed = ShipControl_LimitSpeed(base_speed);
    (void)ShipControl_ApplyYawHoldTarget(g_ship_ctrl.yaw_hold_target_cd,
                                         g_ship_ctrl.base_speed,
                                         SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD);
}

void ShipControl_Stop(u8 reason)
{
    if (g_ship_ctrl.initialized == 0U) {
        ShipControl_Init();
    }

    ShipControl_ResetYawHoldController();
    ShipControl_ResetAxisFilter();
    ShipControl_SetMode((reason == SHIP_CONTROL_STOP_REASON_FAILSAFE) ?
                        SHIP_CONTROL_MODE_FAILSAFE_STOP :
                        SHIP_CONTROL_MODE_STOP,
                        reason);
    g_ship_ctrl.manual_valid = 0U;
    g_ship_ctrl.manual_accelerator = 0U;
    g_ship_ctrl.center_stop_count = 0U;
    g_ship_ctrl.auto_last_apply_ms = 0UL;
    g_ship_ctrl.throttle_speed = 0;
    g_ship_ctrl.base_speed = 0;
    g_ship_ctrl.steering_speed = 0;
    g_ship_ctrl.yaw_diff_speed = 0;
    g_ship_ctrl.left_speed = 0;
    g_ship_ctrl.right_speed = 0;
    g_ship_ctrl.motion = SHIP_CONTROL_MOTION_STOP;
    ShipControl_EnsureMotorInit();
#if SHIP_THROTTLE_PWM_ENABLE
    Motor_StopAll();
#endif
}

void ShipControl_StopGpsNav(void)
{
    if (g_ship_ctrl.mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD) {
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_GPS_NAV_STOP);
    }
}

void ShipControl_ResetYawHoldController(void)
{
    g_ship_ctrl.yaw_hold_active = 0U;
    g_ship_ctrl.yaw_hold_target_cd = 0U;
    g_ship_ctrl.yaw_hold_error_cd = 0;
    g_ship_ctrl.yaw_hold_error_ctrl = 0;
    g_ship_ctrl.yaw_hold_output = 0;
    g_ship_ctrl.yaw_hold_last_yaw_speed = 0;
    g_ship_ctrl.yaw_hold_last_update_ms = 0UL;
    g_ship_ctrl.yaw_hold_stable_count = 0U;
    PID_Reset(&g_ship_ctrl_yaw_pid);
}

u8 ShipControl_IsAutoMode(void)
{
    if ((g_ship_ctrl.mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) ||
        (g_ship_ctrl.mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD)) {
        return 1U;
    }
    return 0U;
}

u8 ShipControl_GetMode(void)
{
    return g_ship_ctrl.mode;
}

u8 ShipControl_GetManualAccelerator(void)
{
    return g_ship_ctrl.manual_accelerator;
}

static void ShipControl_EnsureMotorInit(void)
{
#if SHIP_THROTTLE_PWM_ENABLE
    if (g_ship_ctrl.motor_initialized == 0U) {
        Motor_Init();
        g_ship_ctrl.motor_initialized = 1U;
        LOGI(SHIP_CONTROL_TAG, "motor pwm init");
    }
#else
    if (g_ship_ctrl.motor_initialized == 0U) {
        g_ship_ctrl.motor_initialized = 1U;
        LOGI(SHIP_CONTROL_TAG, "pwm disabled");
    }
#endif
}

static void ShipControl_ResetAxisFilter(void)
{
    g_ship_ctrl.filtered_lr_q8 = ((int32)SHIP_AXIS_CENTER << 8);
    g_ship_ctrl.filtered_ud_q8 = ((int32)SHIP_AXIS_CENTER << 8);
}

static void ShipControl_LogModeEvent(u8 old_mode, u8 new_mode, u8 reason)
{
#if SHIP_YAW_HOLD_LOG_ENABLE
    LOGI(SHIP_CONTROL_TAG,
         "ev old=%u new=%u rsn=%u yaw=%u tgt=%u",
         (u16)old_mode,
         (u16)new_mode,
         (u16)reason,
         (u16)g_ship_ctrl.yaw_hold_active,
         g_ship_ctrl.yaw_hold_target_cd);
#else
    (void)old_mode;
    (void)new_mode;
    (void)reason;
#endif
}

static void ShipControl_LogManualGate(u8 state,
                                      int16 throttle_speed,
                                      int16 steering_speed,
                                      int16 left_speed,
                                      int16 right_speed,
                                      int16 diff,
                                      int16 gate)
{
#if SHIP_YAW_HOLD_LOG_ENABLE
    u32 now_ms;

    now_ms = Task_GetTickMs();
    g_ship_ctrl.manual_gate_diff = diff;
    g_ship_ctrl.manual_gate_limit = gate;

    if (state == g_ship_ctrl.manual_gate_state) {
        return;
    }

    g_ship_ctrl.manual_gate_state = state;
    g_ship_ctrl.manual_gate_last_log_ms = now_ms;
    LOGI(SHIP_CONTROL_TAG,
         "gate st=%u m=%u u=%u l=%u tv=%d sv=%d df=%d gt=%d sb=%u hd=%u",
         (u16)state,
         (u16)g_ship_ctrl.mode,
         (u16)g_ship_ctrl.ud,
         (u16)g_ship_ctrl.lr,
         throttle_speed,
         steering_speed,
         diff,
         gate,
         (u16)g_ship_ctrl.yaw_hold_stable_count,
         (u16)MainLoop_IsHeadingReady());
#else
    (void)state;
    (void)throttle_speed;
    (void)steering_speed;
    (void)left_speed;
    (void)right_speed;
    (void)diff;
    (void)gate;
#endif
}

static void ShipControl_SetMode(u8 mode, u8 reason)
{
    u8 old_mode;

    old_mode = g_ship_ctrl.mode;
    g_ship_ctrl.mode = mode;
    if ((old_mode != mode) || (g_ship_ctrl.last_logged_mode != mode)) {
        ShipControl_LogModeEvent(old_mode, mode, reason);
        g_ship_ctrl.last_logged_mode = mode;
    }
}

static u8 ShipControl_ConfirmCenterStop(void)
{
    if (g_ship_ctrl.motion == SHIP_CONTROL_MOTION_STOP) {
        g_ship_ctrl.center_stop_count = 0U;
        return 1U;
    }

    if (g_ship_ctrl.center_stop_count < SHIP_CENTER_STOP_CONFIRM_FRAMES) {
        g_ship_ctrl.center_stop_count++;
    }
    if (g_ship_ctrl.center_stop_count < SHIP_CENTER_STOP_CONFIRM_FRAMES) {
        return 0U;
    }
    return 1U;
}

static u8 ShipControl_AbsAxisDiff(u8 value)
{
    return (value > SHIP_AXIS_CENTER) ?
           (u8)(value - SHIP_AXIS_CENTER) :
           (u8)(SHIP_AXIS_CENTER - value);
}

static int16 ShipControl_AbsSpeed(int16 speed)
{
    return (speed >= 0) ? speed : (int16)(-speed);
}

static int16 ShipControl_LimitSpeed(int16 speed)
{
    if (speed > MOTOR_SPEED_MAX) {
        return MOTOR_SPEED_MAX;
    }
    if (speed < -MOTOR_SPEED_MAX) {
        return -MOTOR_SPEED_MAX;
    }
    return speed;
}

static int16 ShipControl_WrapSignedCd(int32 angle_cd)
{
    while (angle_cd >= 18000L) {
        angle_cd -= 36000L;
    }
    while (angle_cd < -18000L) {
        angle_cd += 36000L;
    }
    return (int16)angle_cd;
}

static u16 ShipControl_WrapUnsignedCd(int32 angle_cd)
{
    while (angle_cd >= 36000L) {
        angle_cd -= 36000L;
    }
    while (angle_cd < 0L) {
        angle_cd += 36000L;
    }
    return (u16)angle_cd;
}

static int16 ShipControl_YawErrorToControl(int16 yaw_error_cd)
{
    int16 sign;
    int32 abs_error_cd;
    int32 active_range_cd;
    int32 control;

    if (yaw_error_cd == 0) {
        return 0;
    }

    sign = (yaw_error_cd > 0) ? 1 : -1;
    abs_error_cd = (yaw_error_cd > 0) ? (int32)yaw_error_cd : -(int32)yaw_error_cd;
    if (abs_error_cd <= (int32)SHIP_YAW_HOLD_DEADBAND_CD) {
        return 0;
    }

    if ((int32)SHIP_YAW_HOLD_FULL_ERROR_CD <= (int32)SHIP_YAW_HOLD_DEADBAND_CD) {
        return (int16)(sign * SHIP_YAW_HOLD_OUTPUT_LIMIT);
    }

    active_range_cd = (int32)SHIP_YAW_HOLD_FULL_ERROR_CD -
                      (int32)SHIP_YAW_HOLD_DEADBAND_CD;
    abs_error_cd -= (int32)SHIP_YAW_HOLD_DEADBAND_CD;
    if (abs_error_cd >= active_range_cd) {
        return (int16)(sign * SHIP_YAW_HOLD_OUTPUT_LIMIT);
    }

    control = (abs_error_cd * (int32)SHIP_YAW_HOLD_OUTPUT_LIMIT) / active_range_cd;
    if (control > (int32)SHIP_YAW_HOLD_OUTPUT_LIMIT) {
        control = (int32)SHIP_YAW_HOLD_OUTPUT_LIMIT;
    }

    return (int16)(sign * (int16)control);
}

static int16 ShipControl_ApplyYawHoldDamping(int16 yaw_control)
{
    int32 damp;
    int32 output;

    damp = ((int32)MainLoop_GetGyroZDps100() *
            (int32)SHIP_YAW_HOLD_GYRO_DAMP_Q10) /
           (100L * 1024L);
    output = (int32)yaw_control - damp;
    if (output > (int32)SHIP_YAW_HOLD_OUTPUT_LIMIT) {
        output = (int32)SHIP_YAW_HOLD_OUTPUT_LIMIT;
    } else if (output < -(int32)SHIP_YAW_HOLD_OUTPUT_LIMIT) {
        output = -(int32)SHIP_YAW_HOLD_OUTPUT_LIMIT;
    }

    return (int16)output;
}

static int16 ShipControl_ApplyYawOutputSlew(int16 yaw_output)
{
    int16 delta;
    int16 step;

    step = (int16)SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP;
    if (step <= 0) {
        g_ship_ctrl.yaw_hold_last_yaw_speed = yaw_output;
        return yaw_output;
    }

    delta = (int16)(yaw_output - g_ship_ctrl.yaw_hold_last_yaw_speed);
    if (delta > step) {
        yaw_output = (int16)(g_ship_ctrl.yaw_hold_last_yaw_speed + step);
    } else if (delta < (int16)(-step)) {
        yaw_output = (int16)(g_ship_ctrl.yaw_hold_last_yaw_speed - step);
    }
    g_ship_ctrl.yaw_hold_last_yaw_speed = yaw_output;
    return yaw_output;
}

static int16 ShipControl_ApplyYawHoldBaseDerate(int16 base_speed, int16 yaw_error_cd)
{
#if SHIP_YAW_HOLD_DERATE_ENABLE
    int16 sign;
    int32 abs_base;
    int32 abs_error_cd;
    int32 start_cd;
    int32 full_cd;
    int32 min_base;
    int32 max_base;
    int32 span_cd;

    if (base_speed == 0) {
        return 0;
    }

    sign = (base_speed >= 0) ? 1 : -1;
    abs_base = (base_speed >= 0) ? (int32)base_speed : -(int32)base_speed;
    abs_error_cd = (yaw_error_cd >= 0) ? (int32)yaw_error_cd : -(int32)yaw_error_cd;
    start_cd = (int32)SHIP_YAW_HOLD_DERATE_START_CD;
    full_cd = (int32)SHIP_YAW_HOLD_DERATE_FULL_CD;
    min_base = (int32)SHIP_YAW_HOLD_DERATE_MIN_BASE;

    if ((start_cd < 0L) || (full_cd <= start_cd) ||
        (min_base < 0L) || (min_base >= (int32)MOTOR_SPEED_MAX) ||
        (abs_error_cd <= start_cd) || (abs_base <= min_base)) {
        return base_speed;
    }

    if (abs_error_cd >= full_cd) {
        max_base = min_base;
    } else {
        span_cd = full_cd - start_cd;
        max_base = abs_base -
                   ((abs_base - min_base) *
                    (abs_error_cd - start_cd)) / span_cd;
    }

    if (max_base < min_base) {
        max_base = min_base;
    }
    if (abs_base > max_base) {
        abs_base = max_base;
    }

    return (int16)(sign * (int16)abs_base);
#else
    (void)yaw_error_cd;
    return base_speed;
#endif
}

static int16 ShipControl_YawControlToSpeed(int16 yaw_control, int16 base_speed)
{
    int32 scale;
    int32 yaw_limit;
    int32 yaw_speed;
    int32 diff_limit_permille;
    int32 speed_headroom;

    scale = (base_speed >= 0) ? (int32)base_speed : -(int32)base_speed;
    if (scale == 0L) {
        scale = (int32)MOTOR_SPEED_MAX;
    }

    diff_limit_permille = (int32)SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE;
    if (diff_limit_permille < 0L) {
        diff_limit_permille = 0L;
    } else if (diff_limit_permille > 1000L) {
        diff_limit_permille = 1000L;
    }

    yaw_limit = (scale * diff_limit_permille) / 1000L;
    if (base_speed != 0) {
        speed_headroom = (int32)MOTOR_SPEED_MAX - scale;
        if (speed_headroom < 0L) {
            speed_headroom = 0L;
        }
        if (yaw_limit > speed_headroom) {
            yaw_limit = speed_headroom;
        }
    }
    if (yaw_limit <= 0L) {
        return 0;
    }

    yaw_speed = ((int32)yaw_control * yaw_limit) / (int32)SHIP_YAW_HOLD_OUTPUT_LIMIT;
    if (yaw_speed > yaw_limit) {
        yaw_speed = yaw_limit;
    } else if (yaw_speed < -yaw_limit) {
        yaw_speed = -yaw_limit;
    }

    return ShipControl_LimitSpeed((int16)yaw_speed);
}

static u8 ShipControl_YawHoldGateStable(void)
{
    if (g_ship_ctrl.yaw_hold_stable_count < SHIP_YAW_HOLD_STEER_STABLE_FRAMES) {
        g_ship_ctrl.yaw_hold_stable_count++;
    }
    if (g_ship_ctrl.yaw_hold_stable_count >= SHIP_YAW_HOLD_STEER_STABLE_FRAMES) {
        return 1U;
    }
    return 0U;
}

static int16 ShipControl_FilterAxis(u8 raw, int32 *state_q8)
{
    int32 target_q8;

    target_q8 = ((int32)raw << 8);
    *state_q8 += ((target_q8 - *state_q8) >> SHIP_AXIS_FILTER_SHIFT);
    return (int16)((*state_q8 + 128) >> 8);
}

static int16 ShipControl_ApplyAxisCurve(int16 value,
                                        int16 deadband,
                                        int16 min_command,
                                        int16 max_command)
{
    int16 delta;
    int16 sign;
    int16 magnitude;
    int16 range;
    int32 command;

    delta = (int16)(value - (int16)SHIP_AXIS_CENTER);
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

static int16 ShipControl_ThrottleToSignedSpeed(int16 value)
{
    return ShipControl_ApplyAxisCurve(value,
                                      SHIP_THROTTLE_DEADBAND,
                                      SHIP_THROTTLE_MIN_COMMAND,
                                      SHIP_THROTTLE_MAX_COMMAND);
}

static int16 ShipControl_SteeringToSignedSpeed(int16 value)
{
    return ShipControl_ApplyAxisCurve(value,
                                      SHIP_STEERING_DEADBAND,
                                      0,
                                      SHIP_STEERING_MAX_COMMAND);
}

static void ShipControl_UpdateManualAcceleratorRaw(u8 left_right, u8 front_back)
{
    u8 abs_left_right;
    u8 abs_front_back;

    abs_left_right = ShipControl_AbsAxisDiff(left_right);
    abs_front_back = ShipControl_AbsAxisDiff(front_back);
    abs_front_back = (u8)(abs_front_back + 5U);

    if ((abs_front_back > abs_left_right) &&
        ((abs_left_right > 10U) || (abs_front_back > 20U)) &&
        (front_back > 110U)) {
        g_ship_ctrl.manual_accelerator = (u8)(front_back - SHIP_AXIS_CENTER);
    } else {
        g_ship_ctrl.manual_accelerator = 0U;
    }
}

static void ShipControl_SetMotorTargets(int16 left_speed, int16 right_speed)
{
    ShipControl_EnsureMotorInit();
#if SHIP_THROTTLE_PWM_ENABLE
    Motor_SetBothSpeed(left_speed, right_speed);
#endif
    g_ship_ctrl.left_speed = left_speed;
    g_ship_ctrl.right_speed = right_speed;
}

static void ShipControl_ApplyOpenLoop(ShipControl_Motion_t motion,
                                      int16 left_speed,
                                      int16 right_speed,
                                      int16 throttle_speed,
                                      int16 steering_speed)
{
    ShipControl_SetMode((motion == SHIP_CONTROL_MOTION_STOP) ?
                        SHIP_CONTROL_MODE_STOP :
                        SHIP_CONTROL_MODE_MANUAL_OPEN_LOOP,
                        SHIP_CONTROL_REASON_MANUAL_OPEN);
    g_ship_ctrl.motion = motion;
    g_ship_ctrl.throttle_speed = throttle_speed;
    g_ship_ctrl.base_speed = 0;
    g_ship_ctrl.steering_speed = steering_speed;
    g_ship_ctrl.yaw_diff_speed = 0;
    ShipControl_SetMotorTargets(left_speed, right_speed);
}

static u8 ShipControl_ApplyYawHoldTarget(u16 target_heading_cd,
                                         int16 base_speed,
                                         u8 mode)
{
#if SHIP_YAW_HOLD_ENABLE
    u32 now_ms;
    u16 current_heading_cd;
    int16 yaw_error_cd;
    int16 yaw_error_ctrl;
    int16 yaw_base_speed;
    int16 yaw_output;
    int16 left_speed;
    int16 right_speed;
    int16 pid_output;

    if (MainLoop_IsHeadingReady() == 0U) {
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_HEADING_LOST);
        return 0U;
    }

    now_ms = Task_GetTickMs();
    current_heading_cd = MainLoop_GetHeadingDeg100();
    target_heading_cd = ShipControl_WrapUnsignedCd((int32)target_heading_cd);

    if (g_ship_ctrl.yaw_hold_active == 0U) {
        g_ship_ctrl.yaw_hold_active = 1U;
        g_ship_ctrl.yaw_hold_output = 0;
        g_ship_ctrl.yaw_hold_last_yaw_speed = 0;
        g_ship_ctrl.yaw_hold_last_update_ms = now_ms - SHIP_YAW_HOLD_PERIOD_MS;
        g_ship_ctrl.yaw_hold_stable_count = SHIP_YAW_HOLD_STEER_STABLE_FRAMES;
        PID_Reset(&g_ship_ctrl_yaw_pid);
        PID_SetTarget(&g_ship_ctrl_yaw_pid, 0);
    }

    g_ship_ctrl.yaw_hold_target_cd = target_heading_cd;
    if ((now_ms - g_ship_ctrl.yaw_hold_last_update_ms) >= SHIP_YAW_HOLD_PERIOD_MS) {
        g_ship_ctrl.yaw_hold_last_update_ms = now_ms;
        yaw_error_cd = ShipControl_WrapSignedCd((int32)target_heading_cd -
                                                (int32)current_heading_cd);
        yaw_error_ctrl = ShipControl_YawErrorToControl(yaw_error_cd);
        g_ship_ctrl.yaw_hold_error_cd = yaw_error_cd;
        g_ship_ctrl.yaw_hold_error_ctrl = yaw_error_ctrl;
        if (yaw_error_ctrl == 0) {
            PID_Reset(&g_ship_ctrl_yaw_pid);
            g_ship_ctrl.yaw_hold_output = ShipControl_ApplyYawHoldDamping(0);
        } else {
            pid_output = PID_UpdateTarget(&g_ship_ctrl_yaw_pid,
                                          yaw_error_ctrl,
                                          0);
            g_ship_ctrl.yaw_hold_output = ShipControl_ApplyYawHoldDamping(pid_output);
        }
    }

    yaw_base_speed = ShipControl_ApplyYawHoldBaseDerate(base_speed,
                                                        g_ship_ctrl.yaw_hold_error_cd);
    yaw_output = ShipControl_YawControlToSpeed(g_ship_ctrl.yaw_hold_output,
                                               yaw_base_speed);
#if SHIP_YAW_HOLD_OUTPUT_SIGN < 0
    yaw_output = (int16)(-yaw_output);
#endif
    yaw_output = ShipControl_ApplyYawOutputSlew(yaw_output);
    left_speed = ShipControl_LimitSpeed((int16)(yaw_base_speed + yaw_output));
    right_speed = ShipControl_LimitSpeed((int16)(yaw_base_speed - yaw_output));

    ShipControl_SetMode(mode,
                        (mode == SHIP_CONTROL_MODE_MANUAL_YAW_HOLD) ?
                        SHIP_CONTROL_REASON_MANUAL_YAW :
                        ((mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) ?
                         SHIP_CONTROL_REASON_CRUISE :
                         SHIP_CONTROL_REASON_GPS_NAV));
    if (yaw_base_speed > 0) {
        g_ship_ctrl.motion = SHIP_CONTROL_MOTION_FORWARD;
    } else if (yaw_base_speed < 0) {
        g_ship_ctrl.motion = SHIP_CONTROL_MOTION_BACKWARD;
    } else if (yaw_output > 0) {
        g_ship_ctrl.motion = SHIP_CONTROL_MOTION_RIGHT;
    } else if (yaw_output < 0) {
        g_ship_ctrl.motion = SHIP_CONTROL_MOTION_LEFT;
    } else {
        g_ship_ctrl.motion = SHIP_CONTROL_MOTION_STOP;
    }
    g_ship_ctrl.throttle_speed = base_speed;
    g_ship_ctrl.base_speed = yaw_base_speed;
    g_ship_ctrl.steering_speed = 0;
    g_ship_ctrl.yaw_diff_speed = yaw_output;
    ShipControl_SetMotorTargets(left_speed, right_speed);
    return 1U;
#else
    (void)target_heading_cd;
    (void)base_speed;
    (void)mode;
    return 0U;
#endif
}

static void ShipControl_ApplyManualControl(void)
{
    int16 throttle_speed;
    int16 steering_speed;
    int16 left_speed;
    int16 right_speed;
    int16 abs_throttle;
    int16 abs_steering;
    int16 abs_left_input;
    int16 abs_right_input;
    int16 max_manual_input;
    int16 manual_input_diff;
    int16 manual_diff_gate;
    int16 yaw_base_speed;
    u8 yaw_hold_gate_open;
    ShipControl_Motion_t target_motion;

    if ((g_ship_ctrl.lr >= SHIP_LR_DEAD_LOW) && (g_ship_ctrl.lr <= SHIP_LR_DEAD_HIGH) &&
        (g_ship_ctrl.ud >= SHIP_FB_DEAD_LOW) && (g_ship_ctrl.ud <= SHIP_FB_DEAD_HIGH)) {
        ShipControl_LogManualGate(SHIP_CTRL_GATE_CENTER,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0);
        if (ShipControl_ConfirmCenterStop() == 0U) {
            return;
        }
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_MANUAL_CENTER);
        g_ship_ctrl.manual_valid = 1U;
        return;
    }
    g_ship_ctrl.center_stop_count = 0U;

    throttle_speed = ShipControl_ThrottleToSignedSpeed(
        ShipControl_FilterAxis(g_ship_ctrl.ud, &g_ship_ctrl.filtered_ud_q8));
    steering_speed = ShipControl_SteeringToSignedSpeed(
        ShipControl_FilterAxis(g_ship_ctrl.lr, &g_ship_ctrl.filtered_lr_q8));
    abs_throttle = ShipControl_AbsSpeed(throttle_speed);
    abs_steering = ShipControl_AbsSpeed(steering_speed);
    left_speed = ShipControl_LimitSpeed((int16)(throttle_speed + steering_speed));
    right_speed = ShipControl_LimitSpeed((int16)(throttle_speed - steering_speed));
    abs_left_input = ShipControl_AbsSpeed(left_speed);
    abs_right_input = ShipControl_AbsSpeed(right_speed);
    max_manual_input = (abs_left_input >= abs_right_input) ?
                       abs_left_input :
                       abs_right_input;
    manual_input_diff = ShipControl_AbsSpeed((int16)(left_speed - right_speed));
    manual_diff_gate =
        (int16)(((int32)max_manual_input *
                 (int32)SHIP_MANUAL_YAW_HOLD_DIFF_PERCENT) / 100L);
    g_ship_ctrl.manual_gate_diff = manual_input_diff;
    g_ship_ctrl.manual_gate_limit = manual_diff_gate;
    yaw_hold_gate_open = 0U;

    if ((abs_throttle == 0) && (abs_steering == 0)) {
        ShipControl_LogManualGate(SHIP_CTRL_GATE_NO_INPUT,
                                  throttle_speed,
                                  steering_speed,
                                  left_speed,
                                  right_speed,
                                  manual_input_diff,
                                  manual_diff_gate);
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_MANUAL_CENTER);
        g_ship_ctrl.manual_valid = 1U;
        return;
    }

#if SHIP_YAW_HOLD_ENABLE && SHIP_YAW_HOLD_MANUAL_ENABLE
    if ((max_manual_input > 0) &&
        (manual_input_diff < manual_diff_gate) &&
#if SHIP_YAW_HOLD_FORWARD_ONLY
        (throttle_speed > 0)
#else
        (abs_throttle > 0)
#endif
        ) {
        yaw_hold_gate_open = 1U;
        if (ShipControl_YawHoldGateStable() != 0U) {
            if (g_ship_ctrl.yaw_hold_active == 0U) {
                if (MainLoop_IsHeadingReady() == 0U) {
                    ShipControl_LogManualGate(SHIP_CTRL_GATE_HEADING_LOST,
                                              throttle_speed,
                                              steering_speed,
                                              left_speed,
                                              right_speed,
                                              manual_input_diff,
                                              manual_diff_gate);
                    goto ship_control_manual_open_loop;
                }
                ShipControl_LogManualGate(SHIP_CTRL_GATE_READY,
                                          throttle_speed,
                                          steering_speed,
                                          left_speed,
                                          right_speed,
                                          manual_input_diff,
                                          manual_diff_gate);
                g_ship_ctrl.yaw_hold_active = 1U;
                g_ship_ctrl.yaw_hold_target_cd = MainLoop_GetHeadingDeg100();
                g_ship_ctrl.yaw_hold_output = 0;
                g_ship_ctrl.yaw_hold_last_yaw_speed = 0;
                g_ship_ctrl.yaw_hold_last_update_ms = Task_GetTickMs() - SHIP_YAW_HOLD_PERIOD_MS;
                PID_Reset(&g_ship_ctrl_yaw_pid);
                PID_SetTarget(&g_ship_ctrl_yaw_pid, 0);
            }

            yaw_base_speed = throttle_speed;
            if (ShipControl_ApplyYawHoldTarget(g_ship_ctrl.yaw_hold_target_cd,
                                               yaw_base_speed,
                                               SHIP_CONTROL_MODE_MANUAL_YAW_HOLD) != 0U) {
                return;
            }
        } else {
            ShipControl_LogManualGate(SHIP_CTRL_GATE_WAIT_STABLE,
                                      throttle_speed,
                                      steering_speed,
                                      left_speed,
                                      right_speed,
                                      manual_input_diff,
                                      manual_diff_gate);
        }
    }

    if (yaw_hold_gate_open == 0U) {
        ShipControl_LogManualGate(
#if SHIP_YAW_HOLD_FORWARD_ONLY
            (throttle_speed <= 0) ? SHIP_CTRL_GATE_THROTTLE : SHIP_CTRL_GATE_DIFF,
#else
            (abs_throttle == 0) ? SHIP_CTRL_GATE_THROTTLE : SHIP_CTRL_GATE_DIFF,
#endif
            throttle_speed,
            steering_speed,
            left_speed,
            right_speed,
            manual_input_diff,
            manual_diff_gate);
        g_ship_ctrl.yaw_hold_stable_count = 0U;
        g_ship_ctrl.yaw_hold_last_yaw_speed = 0;
    }
#endif

ship_control_manual_open_loop:
    g_ship_ctrl.manual_valid = 1U;
#if SHIP_YAW_HOLD_ENABLE
    if (g_ship_ctrl.yaw_hold_active != 0U) {
        ShipControl_ResetYawHoldController();
    }
#endif

    if ((abs_throttle + SHIP_TURN_COMPARE_BIAS) >= abs_steering) {
        target_motion = (throttle_speed >= 0) ?
                        SHIP_CONTROL_MOTION_FORWARD :
                        SHIP_CONTROL_MOTION_BACKWARD;
    } else {
        target_motion = (steering_speed >= 0) ?
                        SHIP_CONTROL_MOTION_RIGHT :
                        SHIP_CONTROL_MOTION_LEFT;
    }

    ShipControl_ApplyOpenLoop(target_motion,
                              left_speed,
                              right_speed,
                              throttle_speed,
                              steering_speed);
}

static void ShipControl_LogSample(u32 now_ms)
{
#if SHIP_YAW_HOLD_LOG_ENABLE
    if ((SHIP_YAW_HOLD_LOG_PERIOD_MS != 0U) &&
        ((now_ms - g_ship_ctrl.manual_last_log_ms) < SHIP_YAW_HOLD_LOG_PERIOD_MS)) {
        return;
    }
    g_ship_ctrl.manual_last_log_ms = now_ms;

    if ((g_ship_ctrl.mode == SHIP_CONTROL_MODE_MANUAL_YAW_HOLD) ||
        (g_ship_ctrl.mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) ||
        (g_ship_ctrl.mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD)) {
        LOGI(SHIP_CONTROL_TAG,
             "mode=%u tgt=%u err=%d pid=%d df=%d th=%d base=%d l=%d r=%d gs=%u gd=%d gl=%d",
             (u16)g_ship_ctrl.mode,
             g_ship_ctrl.yaw_hold_target_cd,
             g_ship_ctrl.yaw_hold_error_cd,
             g_ship_ctrl.yaw_hold_output,
             g_ship_ctrl.yaw_diff_speed,
             g_ship_ctrl.throttle_speed,
             g_ship_ctrl.base_speed,
             g_ship_ctrl.left_speed,
             g_ship_ctrl.right_speed,
             (u16)g_ship_ctrl.manual_gate_state,
             g_ship_ctrl.manual_gate_diff,
             g_ship_ctrl.manual_gate_limit);
    }
#else
    (void)now_ms;
#endif
}
