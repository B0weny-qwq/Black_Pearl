/**
 * @file    ShipControl.c
 * @brief   手动、定速巡航和 GPS 导航共用的船体运动控制实现。
 *
 * @details
 * ShipControl 是工程中唯一直接提交左右电机目标的上层模块。它把摇杆输入、
 * 巡航请求和 GPS 目标航向统一转换成左右电机速度，并复用同一套 yaw-hold
 * PID、差速限幅、陀螺阻尼和输出斜率限制。
 */

#include "ShipControl.h"

#include "..\Motor\Motor.h"
#include "..\..\Function\PID\PID.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\..\User\MainLoop.h"
#include "..\..\..\User\Task.h"

#define SHIP_CONTROL_TAG                 "CTRL"
#define SHIP_CONTROL_DATA_TAG            "DATA"

/* ==================== 遥控输入基础参数 ==================== */
/* 摇杆中心值，协议中左右/前后通道都以 100 为中心。 */
#define SHIP_AXIS_CENTER                 100U
/* 左右摇杆回中死区。 */
#define SHIP_LR_DEAD_LOW                 90U
#define SHIP_LR_DEAD_HIGH                110U
/* 前后油门摇杆回中死区。 */
#define SHIP_FB_DEAD_LOW                 90U
#define SHIP_FB_DEAD_HIGH                110U
/* 左右转向比较偏置，避免摇杆轻微抖动导致方向误判。 */
#define SHIP_TURN_COMPARE_BIAS           5U
/* 中位停车确认帧数，连续确认后才真正停车。 */
#define SHIP_CENTER_STOP_CONFIRM_FRAMES  2U
/* 手动 yaw 自稳只在前进方向启用。 */
#define SHIP_YAW_HOLD_FORWARD_ONLY       1U

/* ==================== 遥控输入滤波与曲线 ==================== */
#ifndef SHIP_AXIS_FILTER_SHIFT
/* 摇杆一阶滤波强度，越大越平滑但响应越慢。 */
#define SHIP_AXIS_FILTER_SHIFT           1U
#endif
#ifndef SHIP_RC_AXIS_MAX_DELTA
/* 单次摇杆输入允许的最大变化量，用于压制异常跳变。 */
#define SHIP_RC_AXIS_MAX_DELTA           100
#endif
#ifndef SHIP_THROTTLE_DEADBAND
/* 油门曲线死区，摇杆偏离中心小于该值时视为 0。 */
#define SHIP_THROTTLE_DEADBAND           4
#endif
#ifndef SHIP_STEERING_DEADBAND
/* 转向曲线死区，减少中位附近左右电机抖动。 */
#define SHIP_STEERING_DEADBAND           8
#endif

/* ==================== 电机输出限幅 ==================== */
#ifndef SHIP_THROTTLE_MIN_COMMAND
/* 油门离开死区后的最小有效电机命令。 */
#define SHIP_THROTTLE_MIN_COMMAND        180
#endif
#ifndef SHIP_THROTTLE_MAX_COMMAND
/* 油门最大电机命令。 */
#define SHIP_THROTTLE_MAX_COMMAND        850
#endif
#ifndef SHIP_MOTOR_OUTPUT_MAX_COMMAND
/* 所有上层控制最终写电机前的总限幅。 */
#define SHIP_MOTOR_OUTPUT_MAX_COMMAND    SHIP_THROTTLE_MAX_COMMAND
#endif
#ifndef SHIP_STEERING_MAX_COMMAND
/* 手动原地/差速转向的最大命令。 */
#define SHIP_STEERING_MAX_COMMAND        700
#endif

/* ==================== 定速巡航软启动 ==================== */
#ifndef SHIP_CRUISE_BASE_SPEED
/* 定速巡航默认基础速度，未显式传入速度时使用。 */
#define SHIP_CRUISE_BASE_SPEED           SHIP_THROTTLE_MAX_COMMAND
#endif
#ifndef SHIP_CRUISE_RAMP_MS
/* 定速巡航从低速拉到目标速度的软启动时间。 */
#define SHIP_CRUISE_RAMP_MS              1800UL
#endif
#ifndef SHIP_CRUISE_RAMP_MIN_BASE
/* 定速巡航软启动起始基础速度。 */
#define SHIP_CRUISE_RAMP_MIN_BASE        520
#endif

/* ==================== 控制周期与日志周期 ==================== */
#ifndef SHIP_MANUAL_CONTROL_PERIOD_MS
/* 手动控制和自动控制输出刷新周期。 */
#define SHIP_MANUAL_CONTROL_PERIOD_MS    10UL
#endif
#ifndef SHIP_YAW_HOLD_LOG_PERIOD_MS
/* yaw 自稳运行日志输出周期。 */
#define SHIP_YAW_HOLD_LOG_PERIOD_MS      1000UL
#endif
#ifndef SHIP_MOT_LOG_PERIOD_MS
/* 电机目标日志输出周期。 */
#define SHIP_MOT_LOG_PERIOD_MS           200UL
#endif
#ifndef SHIP_MOT_LOG_ENABLE
/* 电机日志总开关。 */
#define SHIP_MOT_LOG_ENABLE              1U
#endif
#ifndef SHIP_MANUAL_GATE_LOG_PERIOD_MS
/* 手动 yaw 自稳门控日志输出周期。 */
#define SHIP_MANUAL_GATE_LOG_PERIOD_MS   300UL
#endif

/* ==================== yaw 自稳公共参数 ==================== */
#ifndef SHIP_YAW_HOLD_PERIOD_MS
/* yaw PID 计算周期；电机输出仍按控制周期刷新。 */
#define SHIP_YAW_HOLD_PERIOD_MS          50UL
#endif
#ifndef SHIP_YAW_HOLD_FULL_ERROR_CD
/* 航向误差达到该值时映射为满控制量，单位 0.01 度。 */
#define SHIP_YAW_HOLD_FULL_ERROR_CD      1000
#endif
#ifndef SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE
/* yaw 修正差速上限，千分比；400 表示基础速度的 40%。 */
#define SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE 400
#endif
#ifndef SHIP_MANUAL_YAW_HOLD_DIFF_PERCENT
/* 手动 yaw 自稳进入条件：左右电机差值占当前输入上限的百分比。 */
#define SHIP_MANUAL_YAW_HOLD_DIFF_PERCENT 20U
#endif
#ifndef SHIP_YAW_HOLD_OUTPUT_SIGN
/* yaw 输出方向，现场若发现越修越偏可改为 -1。 */
#define SHIP_YAW_HOLD_OUTPUT_SIGN        1
#endif
#ifndef SHIP_YAW_HOLD_DERATE_ENABLE
/* 大偏航时是否压低基础速度，便于更快修正方向。 */
#define SHIP_YAW_HOLD_DERATE_ENABLE      1
#endif
#ifndef SHIP_YAW_HOLD_DERATE_START_CD
/* 开始降速的偏航误差，单位 0.01 度。 */
#define SHIP_YAW_HOLD_DERATE_START_CD    300
#endif
#ifndef SHIP_YAW_HOLD_DERATE_FULL_CD
/* 达到最大降速的偏航误差，单位 0.01 度。 */
#define SHIP_YAW_HOLD_DERATE_FULL_CD     1000
#endif
#ifndef SHIP_YAW_HOLD_DERATE_MIN_BASE
/* 大偏航满降速时允许保留的最低基础速度。 */
#define SHIP_YAW_HOLD_DERATE_MIN_BASE    500
#endif
#ifndef SHIP_YAW_HOLD_GYRO_DAMP_Q10
/* 陀螺 Z 轴阻尼系数，Q10；用于抑制转向过冲。 */
#define SHIP_YAW_HOLD_GYRO_DAMP_Q10      3072
#endif
#ifndef SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP
/* 最终差速输出每个控制步允许变化的最大值。 */
#define SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP 30
#endif
#ifndef SHIP_YAW_HOLD_STEER_STABLE_FRAMES
/* 手动进入 yaw 自稳前，转向回中需要连续稳定的帧数。 */
#define SHIP_YAW_HOLD_STEER_STABLE_FRAMES 2U
#endif
#ifndef SHIP_YAW_HOLD_OUTPUT_LIMIT
/* PID 内部归一化输出限幅，不是电机 PWM，也不是最终 speed。 */
#define SHIP_YAW_HOLD_OUTPUT_LIMIT       1000
#endif
#ifndef SHIP_YAW_HOLD_DEADBAND_CD
/* 航向误差死区，单位 0.01 度；50 表示 +/-0.50 度。 */
#define SHIP_YAW_HOLD_DEADBAND_CD        50
#endif

/* ==================== 正常巡航 yaw PID ==================== */
#ifndef SHIP_YAW_HOLD_KP_Q10
/* 正常 yaw 自稳比例增益，Q10。 */
#define SHIP_YAW_HOLD_KP_Q10             768
#endif
#ifndef SHIP_YAW_HOLD_KI_Q10
/* 正常 yaw 自稳积分增益，当前关闭以避免水面延迟导致积分堆积。 */
#define SHIP_YAW_HOLD_KI_Q10             0
#endif
#ifndef SHIP_YAW_HOLD_KD_Q10
/* 正常 yaw 自稳微分增益，Q10。 */
#define SHIP_YAW_HOLD_KD_Q10             0
#endif

/* ==================== GPS 启航前原地对准 PID ==================== */
#ifndef SHIP_GPS_ALIGN_KP_Q10
/* GPS 原地对准比例增益，Q10；越大越积极转向目标航向。 */
#define SHIP_GPS_ALIGN_KP_Q10            384
#endif
#ifndef SHIP_GPS_ALIGN_KI_Q10
/* GPS 原地对准积分增益；保持 0 可避免原地旋转时积分堆积。 */
#define SHIP_GPS_ALIGN_KI_Q10            0
#endif
#ifndef SHIP_GPS_ALIGN_KD_Q10
/* GPS 原地对准微分增益，Q10；用于抑制接近目标航向时的过冲。 */
#define SHIP_GPS_ALIGN_KD_Q10            0
#endif
#ifndef SHIP_GPS_ALIGN_DIFF_PERCENT
/* 原地对准最大差速，占电机最大命令的百分比；30 表示 30%。 */
#define SHIP_GPS_ALIGN_DIFF_PERCENT      18U
#endif
#ifndef SHIP_GPS_NAV_KP_Q10
#define SHIP_GPS_NAV_KP_Q10              1024
#endif
#ifndef SHIP_GPS_NAV_KI_Q10
#define SHIP_GPS_NAV_KI_Q10              0
#endif
#ifndef SHIP_GPS_NAV_KD_Q10
#define SHIP_GPS_NAV_KD_Q10              96
#endif
#ifndef SHIP_GPS_NAV_DIFF_LIMIT_PERMILLE
#define SHIP_GPS_NAV_DIFF_LIMIT_PERMILLE 1000
#endif
#ifndef SHIP_GPS_NAV_DERATE_START_CD
#define SHIP_GPS_NAV_DERATE_START_CD     800
#endif
#ifndef SHIP_GPS_NAV_DERATE_FULL_CD
#define SHIP_GPS_NAV_DERATE_FULL_CD      1200
#endif
#ifndef SHIP_GPS_NAV_DERATE_MIN_BASE
#define SHIP_GPS_NAV_DERATE_MIN_BASE     320
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

/**
 * @brief 控制层内部运动方向，用于日志和模式判定。
 */
typedef enum
{
    SHIP_CONTROL_MOTION_STOP = 0,
    SHIP_CONTROL_MOTION_FORWARD,
    SHIP_CONTROL_MOTION_BACKWARD,
    SHIP_CONTROL_MOTION_LEFT,
    SHIP_CONTROL_MOTION_RIGHT
} ShipControl_Motion_t;

/**
 * @brief 控制层运行时状态。
 *
 * @details
 * 保存最新遥控输入、滤波状态、当前模式、闭环航向目标、PID 输出、
 * 左右电机目标以及日志限频字段。该结构只在本文件内部使用。
 */
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
    u32 cruise_start_ms;
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
    u32 motor_last_log_ms;
    int16 motor_last_log_left;
    int16 motor_last_log_right;
    u8 motor_last_log_mode;
    u8 motor_last_log_motion;
    u8 last_logged_mode;
    ShipControl_Motion_t motion;
} ShipControl_Runtime_t;

static ShipControl_Runtime_t xdata g_ship_ctrl;
static PID_Controller_t xdata g_ship_ctrl_yaw_pid;
static PID_Controller_t xdata g_ship_ctrl_gps_nav_pid;
static PID_Controller_t xdata g_ship_ctrl_align_pid;

/** @brief 确保电机 PWM 层已初始化。 */
static void ShipControl_EnsureMotorInit(void);
/** @brief 将摇杆滤波状态复位到中心值。 */
static void ShipControl_ResetAxisFilter(void);
/** @brief 对摇杆回中停机做多帧确认，避免瞬时抖动直接停机。 */
static u8 ShipControl_ConfirmCenterStop(void);
/** @brief 计算摇杆原始值相对中心值 100 的绝对偏差。 */
static u8 ShipControl_AbsAxisDiff(u8 value);
/** @brief 计算带符号速度绝对值。 */
static int16 ShipControl_AbsSpeed(int16 speed);
/** @brief 将电机命令限制在统一输出范围内。 */
static int16 ShipControl_LimitSpeed(int16 speed);
/** @brief 将角度归一化为 [-18000, 18000) 的带符号 0.01 度。 */
static int16 ShipControl_WrapSignedCd(int32 angle_cd);
/** @brief 将角度归一化为 [0, 36000) 的无符号 0.01 度。 */
static u16 ShipControl_WrapUnsignedCd(int32 angle_cd);
/** @brief 将航向误差映射为 PID 使用的归一化控制量。 */
static int16 ShipControl_YawErrorToControl(int16 yaw_error_cd);
/** @brief 叠加陀螺 Z 轴阻尼，抑制转向过冲。 */
static int16 ShipControl_ApplyYawHoldDamping(int16 yaw_control);
/** @brief 对航向差速输出做限斜率处理。 */
static int16 ShipControl_ApplyYawOutputSlew(int16 yaw_output);
/** @brief 限制 GPS 原地对准阶段的最大差速输出。 */
static int16 ShipControl_LimitGpsAlignYawOutput(int16 yaw_output);
/** @brief 定速巡航进入阶段基础速度软启动。 */
static int16 ShipControl_ApplyCruiseBaseRamp(int16 base_speed);
/** @brief 偏航误差较大时降低基础速度，优先完成转向修正。 */
static int16 ShipControl_ApplyYawHoldBaseDerate(int16 base_speed, int16 yaw_error_cd);
static int16 ShipControl_ApplyGpsNavBaseDerate(int16 base_speed, int16 yaw_error_cd);
/** @brief 将归一化 yaw 控制量转换为左右电机差速量。 */
static int16 ShipControl_YawControlToSpeed(int16 yaw_control,
                                           int16 base_speed,
                                           int16 diff_limit_permille);
/** @brief 手动自稳进入前的连续稳定帧门控。 */
static u8 ShipControl_YawHoldGateStable(void);
/** @brief 对摇杆原始值做一阶 IIR 滤波。 */
static int16 ShipControl_FilterAxis(u8 raw, int32 *state_q8);
/** @brief 将摇杆偏移按死区和二次曲线转换为电机命令。 */
static int16 ShipControl_ApplyAxisCurve(int16 value,
                                        int16 deadband,
                                        int16 min_command,
                                        int16 max_command);
/** @brief 将前后摇杆滤波值转换为带符号油门速度。 */
static int16 ShipControl_ThrottleToSignedSpeed(int16 value);
/** @brief 将左右摇杆滤波值转换为带符号转向速度。 */
static int16 ShipControl_SteeringToSignedSpeed(int16 value);
/** @brief 更新供上层判断巡航/自动驾驶条件的手动前进油门幅度。 */
static void ShipControl_UpdateManualAcceleratorRaw(u8 left_right, u8 front_back);
/** @brief 写入左右电机目标并记录运行时输出。 */
static void ShipControl_SetMotorTargets(int16 left_speed, int16 right_speed);
/** @brief 应用手动开环输出。 */
static void ShipControl_ApplyOpenLoop(ShipControl_Motion_t motion,
                                      int16 left_speed,
                                      int16 right_speed,
                                      int16 throttle_speed,
                                      int16 steering_speed);
/** @brief 使用普通 yaw-hold PID 应用目标航向。 */
static u8 ShipControl_ApplyYawHoldTarget(u16 target_heading_cd,
                                         int16 base_speed,
                                         u8 mode);
/** @brief 应用目标航向，可选择 GPS 对准专用 PID。 */
static u8 ShipControl_ApplyYawHoldTargetEx(u16 target_heading_cd,
                                           int16 base_speed,
                                           u8 mode,
                                           u8 use_align_pid);
/** @brief 根据最新遥控输入刷新手动开环或手动航向自稳输出。 */
static void ShipControl_ApplyManualControl(void);
/** @brief 输出航向保持状态日志。 */
static void ShipControl_LogSample(u32 now_ms);
/** @brief 输出电机目标日志，支持强制打印。 */
static void ShipControl_LogMotorOutput(u8 force);
/** @brief 输出控制模式变化事件日志。 */
static void ShipControl_LogModeEvent(u8 old_mode, u8 new_mode, u8 reason);
/** @brief 输出手动航向自稳门控状态变化日志。 */
static void ShipControl_LogManualGate(u8 state,
                                      int16 throttle_speed,
                                      int16 steering_speed,
                                      int16 diff,
                                      int16 gate);
/** @brief 设置当前控制模式并在变化时记录事件。 */
static void ShipControl_SetMode(u8 mode, u8 reason);

/**
 * @brief 初始化统一控制层。
 *
 * @details
 * 复位运行时状态、摇杆滤波器、日志状态和两套 PID。普通 yaw PID 用于
 * 手动自稳/巡航/GPS 前进导航；align PID 用于 GPS 原地对准。
 */
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
    g_ship_ctrl.cruise_start_ms = 0UL;
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
    g_ship_ctrl.motor_last_log_ms = 0UL;
    g_ship_ctrl.motor_last_log_left = 0;
    g_ship_ctrl.motor_last_log_right = 0;
    g_ship_ctrl.motor_last_log_mode = SHIP_CONTROL_MODE_STOP;
    g_ship_ctrl.motor_last_log_motion = SHIP_CONTROL_MOTION_STOP;
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
    PID_Init(&g_ship_ctrl_gps_nav_pid,
             SHIP_GPS_NAV_KP_Q10,
             SHIP_GPS_NAV_KI_Q10,
             SHIP_GPS_NAV_KD_Q10,
             -SHIP_YAW_HOLD_OUTPUT_LIMIT,
             SHIP_YAW_HOLD_OUTPUT_LIMIT,
             -((int32)SHIP_YAW_HOLD_OUTPUT_LIMIT * 64L),
             ((int32)SHIP_YAW_HOLD_OUTPUT_LIMIT * 64L));
    PID_Init(&g_ship_ctrl_align_pid,
             SHIP_GPS_ALIGN_KP_Q10,
             SHIP_GPS_ALIGN_KI_Q10,
             SHIP_GPS_ALIGN_KD_Q10,
             -SHIP_YAW_HOLD_OUTPUT_LIMIT,
             SHIP_YAW_HOLD_OUTPUT_LIMIT,
             -((int32)SHIP_YAW_HOLD_OUTPUT_LIMIT * 64L),
             ((int32)SHIP_YAW_HOLD_OUTPUT_LIMIT * 64L));
}

/**
 * @brief 周期刷新控制输出和日志。
 *
 * @param now_ms 当前系统 tick，单位 ms。
 */
void ShipControl_Tick(u32 now_ms)
{
    int16 cruise_request_speed;

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
        cruise_request_speed = g_ship_ctrl.throttle_speed;
        if (cruise_request_speed == 0) {
            cruise_request_speed = g_ship_ctrl.base_speed;
        }
        (void)ShipControl_ApplyYawHoldTarget(g_ship_ctrl.yaw_hold_target_cd,
                                             cruise_request_speed,
                                             SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD);
    }

    ShipControl_LogSample(now_ms);
}

/**
 * @brief 接收并缓存遥控输入。
 *
 * @param lr     左右摇杆原始值。
 * @param ud     前后摇杆原始值。
 * @param key    按键位。
 * @param now_ms 接收时间，单位 ms。
 */
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

/**
 * @brief 请求定速巡航航向保持。
 *
 * @param heading_cd 目标航向，单位 0.01 度。
 * @param base_speed 基础速度，0 时使用默认巡航速度。
 */
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
    g_ship_ctrl.cruise_start_ms = g_ship_ctrl.auto_last_apply_ms;
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

/**
 * @brief 请求 GPS 前进导航航向保持。
 *
 * @param target_heading_cd 目标航向，单位 0.01 度。
 * @param base_speed        基础速度。
 */
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

/**
 * @brief 请求 GPS 原地对准目标航向。
 *
 * @param target_heading_cd 目标航向，单位 0.01 度。
 */
void ShipControl_RequestGpsAlign(u16 target_heading_cd)
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
    g_ship_ctrl.base_speed = 0;
    (void)ShipControl_ApplyYawHoldTargetEx(g_ship_ctrl.yaw_hold_target_cd,
                                           0,
                                           SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD,
                                           1U);
}

/**
 * @brief 停止控制层输出。
 *
 * @param reason 停止原因，见 @ref ShipControl_StopReason_t。
 */
void ShipControl_Stop(u8 reason)
{
    u8 was_running;

    if (g_ship_ctrl.initialized == 0U) {
        ShipControl_Init();
    }

    was_running = ((g_ship_ctrl.mode != SHIP_CONTROL_MODE_STOP) ||
                   (g_ship_ctrl.left_speed != 0) ||
                   (g_ship_ctrl.right_speed != 0)) ? 1U : 0U;

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
    if (was_running != 0U) {
        ShipControl_LogMotorOutput(1U);
    }
}

/**
 * @brief 停止 GPS 导航模式输出。
 */
void ShipControl_StopGpsNav(void)
{
    if (g_ship_ctrl.mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD) {
        ShipControl_Stop(SHIP_CONTROL_STOP_REASON_GPS_NAV_STOP);
    }
}

/**
 * @brief 复位航向保持控制器状态。
 */
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
    g_ship_ctrl.cruise_start_ms = 0UL;
    PID_Reset(&g_ship_ctrl_yaw_pid);
    PID_Reset(&g_ship_ctrl_gps_nav_pid);
    PID_Reset(&g_ship_ctrl_align_pid);
}

/**
 * @brief 判断是否处于自动类控制模式。
 *
 * @return 1 表示定速巡航或 GPS 导航中，否则返回 0。
 */
u8 ShipControl_IsAutoMode(void)
{
    if ((g_ship_ctrl.mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) ||
        (g_ship_ctrl.mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD)) {
        return 1U;
    }
    return 0U;
}

/**
 * @brief 获取当前控制模式。
 *
 * @return 当前模式值，见 @ref ShipControl_Mode_t。
 */
u8 ShipControl_GetMode(void)
{
    return g_ship_ctrl.mode;
}

/**
 * @brief 获取当前有效前进油门幅度。
 *
 * @return 前进油门幅度，0 表示无有效前进油门。
 */
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
    if (speed > SHIP_MOTOR_OUTPUT_MAX_COMMAND) {
        return SHIP_MOTOR_OUTPUT_MAX_COMMAND;
    }
    if (speed < -SHIP_MOTOR_OUTPUT_MAX_COMMAND) {
        return -SHIP_MOTOR_OUTPUT_MAX_COMMAND;
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

/**
 * @brief 将航向误差转换为归一化 PID 输入。
 *
 * @param yaw_error_cd 航向误差，单位 0.01 度，正负表示左右偏差方向。
 *
 * @return 归一化控制量，范围约为 [-SHIP_YAW_HOLD_OUTPUT_LIMIT,
 *         SHIP_YAW_HOLD_OUTPUT_LIMIT]。
 *
 * @details
 * 小于死区的误差直接视为 0；超过满量程误差后饱和，中间区间线性映射。
 */
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

/**
 * @brief 对 PID 控制量叠加角速度阻尼。
 *
 * @param yaw_control PID 原始输出。
 *
 * @return 限幅后的阻尼输出。
 */
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

static int16 ShipControl_LimitGpsAlignYawOutput(int16 yaw_output)
{
    int32 limit;

    limit = ((int32)SHIP_MOTOR_OUTPUT_MAX_COMMAND *
             (int32)SHIP_GPS_ALIGN_DIFF_PERCENT) / 100L;
    if (limit < 0L) {
        limit = 0L;
    }
    if (yaw_output > (int16)limit) {
        return (int16)limit;
    }
    if (yaw_output < (int16)(-limit)) {
        return (int16)(-limit);
    }
    return yaw_output;
}

/**
 * @brief 定速巡航基础速度软启动。
 *
 * @param base_speed 请求的目标基础速度。
 *
 * @return 当前时刻应使用的基础速度。
 */
static int16 ShipControl_ApplyCruiseBaseRamp(int16 base_speed)
{
    u32 now_ms;
    u32 elapsed_ms;
    int16 sign;
    int32 abs_base;
    int32 min_base;
    int32 ramped_base;

    if ((base_speed == 0) || (g_ship_ctrl.cruise_start_ms == 0UL)) {
        return base_speed;
    }

    if ((u32)SHIP_CRUISE_RAMP_MS == 0UL) {
        return base_speed;
    }

    sign = (base_speed >= 0) ? 1 : -1;
    abs_base = (base_speed >= 0) ? (int32)base_speed : -(int32)base_speed;
    min_base = (int32)SHIP_CRUISE_RAMP_MIN_BASE;
    if (min_base < 0L) {
        min_base = 0L;
    }
    if (min_base >= abs_base) {
        return base_speed;
    }

    now_ms = Task_GetTickMs();
    elapsed_ms = now_ms - g_ship_ctrl.cruise_start_ms;
    if (elapsed_ms >= (u32)SHIP_CRUISE_RAMP_MS) {
        return base_speed;
    }

    ramped_base = min_base +
                  (((abs_base - min_base) * (int32)elapsed_ms) /
                   (int32)SHIP_CRUISE_RAMP_MS);
    if (ramped_base > abs_base) {
        ramped_base = abs_base;
    }

    return (int16)(sign * (int16)ramped_base);
}

/**
 * @brief 根据偏航误差降低基础速度。
 *
 * @param base_speed    原始基础速度。
 * @param yaw_error_cd  当前航向误差，单位 0.01 度。
 *
 * @return 降额后的基础速度。
 *
 * @details
 * 误差较小时保持原速度；误差增大后逐步降到最小基础速度，避免船体
 * 一边高速前进一边大角度修正。
 */
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
        (min_base < 0L) || (min_base >= (int32)SHIP_MOTOR_OUTPUT_MAX_COMMAND) ||
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

static int16 ShipControl_ApplyGpsNavBaseDerate(int16 base_speed, int16 yaw_error_cd)
{
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
    start_cd = (int32)SHIP_GPS_NAV_DERATE_START_CD;
    full_cd = (int32)SHIP_GPS_NAV_DERATE_FULL_CD;
    min_base = (int32)SHIP_GPS_NAV_DERATE_MIN_BASE;

    if ((start_cd < 0L) || (full_cd <= start_cd) ||
        (min_base < 0L) || (min_base >= (int32)SHIP_MOTOR_OUTPUT_MAX_COMMAND) ||
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
}

/**
 * @brief 将 yaw 控制量换算为左右电机差速。
 *
 * @param yaw_control 归一化 yaw 控制量。
 * @param base_speed  当前基础速度，用于按速度比例限制最大差速。
 *
 * @return 电机差速量，正负方向由 @ref SHIP_YAW_HOLD_OUTPUT_SIGN 决定。
 */
static int16 ShipControl_YawControlToSpeed(int16 yaw_control,
                                           int16 base_speed,
                                           int16 diff_limit_permille)
{
    int32 scale;
    int32 yaw_limit;
    int32 yaw_speed;
    int32 diff_limit;

    scale = (base_speed >= 0) ? (int32)base_speed : -(int32)base_speed;
    if (scale == 0L) {
        scale = (int32)SHIP_MOTOR_OUTPUT_MAX_COMMAND;
    }

    diff_limit = (int32)diff_limit_permille;
    if (diff_limit < 0L) {
        diff_limit = 0L;
    } else if (diff_limit > 1000L) {
        diff_limit = 1000L;
    }

    yaw_limit = (scale * diff_limit) / 1000L;
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

/**
 * @brief 摇杆输入曲线。
 *
 * @param value       摇杆值，中心为 100。
 * @param deadband    中心死区。
 * @param min_command 离开死区后的最小命令。
 * @param max_command 最大命令。
 *
 * @return 带符号电机命令。
 *
 * @details
 * 死区外使用二次曲线，低速段更细腻，高速段仍可到达最大输出。
 */
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

    range = (int16)(SHIP_RC_AXIS_MAX_DELTA - deadband);
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

/**
 * @brief 更新“有效前进油门”快照。
 *
 * @param left_right 左右摇杆原始值。
 * @param front_back 前后摇杆原始值。
 *
 * @details
 * 该值主要供无线协议层判断 E 键定速巡航等条件，只有前进方向占优时
 * 才记录非零幅度。
 */
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
    ShipControl_LogMotorOutput(0U);
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
    return ShipControl_ApplyYawHoldTargetEx(target_heading_cd,
                                            base_speed,
                                            mode,
                                            0U);
}

/**
 * @brief 航向保持核心输出链路。
 *
 * @param target_heading_cd 目标航向，单位 0.01 度。
 * @param base_speed        基础速度。
 * @param mode              本次输出要进入的控制模式。
 * @param use_align_pid     非 0 时使用 GPS 原地对准专用 PID。
 *
 * @return 1 表示成功输出电机目标，0 表示航向不可用或功能关闭。
 *
 * @details
 * 本函数完成航向误差归一化、PID 更新、陀螺阻尼、基础速度降额/软启动、
 * 差速换算、限斜率和最终左右电机目标写入。
 */
static u8 ShipControl_ApplyYawHoldTargetEx(u16 target_heading_cd,
                                           int16 base_speed,
                                           u8 mode,
                                           u8 use_align_pid)
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
    PID_Controller_t *pid;
    u8 use_gps_nav_pid;

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
        PID_Reset(&g_ship_ctrl_gps_nav_pid);
        PID_Reset(&g_ship_ctrl_align_pid);
        PID_SetTarget(&g_ship_ctrl_yaw_pid, 0);
        PID_SetTarget(&g_ship_ctrl_gps_nav_pid, 0);
        PID_SetTarget(&g_ship_ctrl_align_pid, 0);
    }

    use_gps_nav_pid =
        ((mode == SHIP_CONTROL_MODE_GPS_NAV_HEADING_HOLD) &&
         (use_align_pid == 0U)) ? 1U : 0U;
    pid = (use_align_pid != 0U) ? &g_ship_ctrl_align_pid :
          ((use_gps_nav_pid != 0U) ? &g_ship_ctrl_gps_nav_pid :
           &g_ship_ctrl_yaw_pid);
    g_ship_ctrl.yaw_hold_target_cd = target_heading_cd;
    if ((now_ms - g_ship_ctrl.yaw_hold_last_update_ms) >= SHIP_YAW_HOLD_PERIOD_MS) {
        g_ship_ctrl.yaw_hold_last_update_ms = now_ms;
        yaw_error_cd = ShipControl_WrapSignedCd((int32)target_heading_cd -
                                                (int32)current_heading_cd);
        yaw_error_ctrl = ShipControl_YawErrorToControl(yaw_error_cd);
        g_ship_ctrl.yaw_hold_error_cd = yaw_error_cd;
        g_ship_ctrl.yaw_hold_error_ctrl = yaw_error_ctrl;
        if (yaw_error_ctrl == 0) {
            PID_Reset(pid);
            g_ship_ctrl.yaw_hold_output = ShipControl_ApplyYawHoldDamping(0);
        } else {
            pid_output = PID_UpdateTarget(pid,
                                          yaw_error_ctrl,
                                          0);
            g_ship_ctrl.yaw_hold_output = ShipControl_ApplyYawHoldDamping(pid_output);
        }
    }

    if (mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) {
        yaw_base_speed = ShipControl_ApplyCruiseBaseRamp(base_speed);
    } else if (use_gps_nav_pid != 0U) {
        yaw_base_speed = ShipControl_ApplyGpsNavBaseDerate(
            base_speed,
            g_ship_ctrl.yaw_hold_error_cd);
    } else {
        yaw_base_speed = ShipControl_ApplyYawHoldBaseDerate(base_speed,
                                                            g_ship_ctrl.yaw_hold_error_cd);
    }
    yaw_output = ShipControl_YawControlToSpeed(
        g_ship_ctrl.yaw_hold_output,
        yaw_base_speed,
        (use_gps_nav_pid != 0U) ?
        (int16)SHIP_GPS_NAV_DIFF_LIMIT_PERMILLE :
        (int16)SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE);
#if SHIP_YAW_HOLD_OUTPUT_SIGN < 0
    yaw_output = (int16)(-yaw_output);
#endif
    if (use_align_pid != 0U) {
        yaw_output = ShipControl_LimitGpsAlignYawOutput(yaw_output);
    }
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
    (void)use_align_pid;
    return 0U;
#endif
}

/**
 * @brief 手动控制仲裁入口。
 *
 * @details
 * 先将摇杆转换为开环左右电机目标，再根据差速比例、前进油门和航向
 * 可用性决定是否进入手动航向自稳；不满足门控条件时退回手动开环。
 */
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
                                              manual_input_diff,
                                              manual_diff_gate);
                    goto ship_control_manual_open_loop;
                }
                ShipControl_LogManualGate(SHIP_CTRL_GATE_READY,
                                          throttle_speed,
                                          steering_speed,
                                          manual_input_diff,
                                          manual_diff_gate);
                g_ship_ctrl.yaw_hold_active = 1U;
                g_ship_ctrl.yaw_hold_target_cd = MainLoop_GetHeadingDeg100();
                g_ship_ctrl.yaw_hold_output = 0;
                g_ship_ctrl.yaw_hold_last_yaw_speed = 0;
                g_ship_ctrl.yaw_hold_last_update_ms = Task_GetTickMs() - SHIP_YAW_HOLD_PERIOD_MS;
                PID_Reset(&g_ship_ctrl_yaw_pid);
                PID_Reset(&g_ship_ctrl_gps_nav_pid);
                PID_SetTarget(&g_ship_ctrl_yaw_pid, 0);
                PID_SetTarget(&g_ship_ctrl_gps_nav_pid, 0);
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
        if (g_ship_ctrl.mode == SHIP_CONTROL_MODE_CRUISE_HEADING_HOLD) {
            log_info((u8 *)SHIP_CONTROL_DATA_TAG,
                     (u8 *)"cruise run req=%d base=%d l=%d r=%d err=%d pid=%d diff=%d tgt=%u",
                     g_ship_ctrl.throttle_speed,
                     g_ship_ctrl.base_speed,
                     g_ship_ctrl.left_speed,
                     g_ship_ctrl.right_speed,
                     g_ship_ctrl.yaw_hold_error_cd,
                     g_ship_ctrl.yaw_hold_output,
                     g_ship_ctrl.yaw_diff_speed,
                     g_ship_ctrl.yaw_hold_target_cd);
        }
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

static void ShipControl_LogMotorOutput(u8 force)
{
#if SHIP_MOT_LOG_ENABLE
    u32 now_ms;
    u8 mode_changed;
    u8 motion_changed;

    now_ms = Task_GetTickMs();
    mode_changed = (g_ship_ctrl.mode != g_ship_ctrl.motor_last_log_mode) ? 1U : 0U;
    motion_changed = (g_ship_ctrl.motion != g_ship_ctrl.motor_last_log_motion) ? 1U : 0U;
    if ((force == 0U) &&
        (mode_changed == 0U) &&
        (motion_changed == 0U) &&
        (SHIP_MOT_LOG_PERIOD_MS != 0U) &&
        ((now_ms - g_ship_ctrl.motor_last_log_ms) < SHIP_MOT_LOG_PERIOD_MS)) {
        return;
    }

    g_ship_ctrl.motor_last_log_ms = now_ms;
    g_ship_ctrl.motor_last_log_left = g_ship_ctrl.left_speed;
    g_ship_ctrl.motor_last_log_right = g_ship_ctrl.right_speed;
    g_ship_ctrl.motor_last_log_mode = g_ship_ctrl.mode;
    g_ship_ctrl.motor_last_log_motion = g_ship_ctrl.motion;
    LOGI(SHIP_CONTROL_TAG,
         "out m=%u mo=%u th=%d base=%d st=%d df=%d l=%d r=%d",
         (u16)g_ship_ctrl.mode,
         (u16)g_ship_ctrl.motion,
         g_ship_ctrl.throttle_speed,
         g_ship_ctrl.base_speed,
         g_ship_ctrl.steering_speed,
         g_ship_ctrl.yaw_diff_speed,
         g_ship_ctrl.left_speed,
         g_ship_ctrl.right_speed);
#else
    (void)force;
#endif
}
