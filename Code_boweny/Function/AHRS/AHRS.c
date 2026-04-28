/**
 * @file    AHRS.c
 * @brief   AHRS 姿态融合模块实现
 * @author  boweny
 * @date    2026-04-27
 * @version v1.0
 *
 * @details
 * - 实现 Q8 定点互补滤波姿态估计
 * - 使用陀螺仪角速度积分获得短期响应
 * - 使用加速度计解算 roll/pitch 并做低频修正，抑制陀螺仪零偏漂移
 * - 使用地磁计低频修正 yaw，默认修正较慢，便于抵抗电机磁干扰和瞬态抖动
 * - 所有三角函数均使用整数近似，不引入浮点库
 *
 * @note
 * 输出角度单位统一为 `deg * 100`，内部姿态状态使用 Q8 小数保存，
 * 避免低速角速度积分时被整数截断。
 *
 * @see     Code_boweny/Function/AHRS/AHRS.h
 */

#include "AHRS.h"

#define AHRS_Q_SHIFT                  8
#define AHRS_Q_SCALE                  256L
#define AHRS_ANGLE_FULL_DEG100        36000L
#define AHRS_ANGLE_HALF_DEG100        18000L
#define AHRS_ANGLE_QUARTER_DEG100     9000L
#define AHRS_ATAN_Q                   1024L
#define AHRS_ACC_MIN_BOOT_NORM        512U

typedef struct
{
    int32 x;            /**< X 轴 Q8 低通状态 */
    int32 y;            /**< Y 轴 Q8 低通状态 */
    int32 z;            /**< Z 轴 Q8 低通状态 */
    u8 initialized;     /**< 首帧状态标志，0=未初始化，1=已初始化 */
} AHRS_Lpf3_t;

/**
 * @brief   AHRS 内部运行上下文
 *
 * @details
 * 保存姿态角、传感器低通、加速度 1g 参考、陀螺仪零偏学习和对外状态快照。
 * 该结构体仅在本模块内部维护，对外通过 `AHRS_GetState()` 暴露只读快照。
 */
typedef struct
{
    int32 roll_q8;                  /**< 横滚角内部 Q8 状态，单位 deg * 100 */
    int32 pitch_q8;                 /**< 俯仰角内部 Q8 状态，单位 deg * 100 */
    int32 yaw_q8;                   /**< 航向角内部 Q8 状态，单位 deg * 100 */

    int32 acc_roll_lpf_q8;          /**< 加速度解算横滚角低通状态 */
    int32 acc_pitch_lpf_q8;         /**< 加速度解算俯仰角低通状态 */
    u8 acc_angle_initialized;       /**< 加速度角度低通是否已初始化 */

    int32 mag_yaw_lpf_q8;           /**< 地磁解算航向角低通状态 */
    u8 mag_yaw_initialized;         /**< 地磁航向低通是否已初始化 */

    u32 acc_ref_sum;                /**< 启动阶段加速度模长累计值 */
    u16 acc_ref_count;              /**< 启动阶段加速度参考采样计数 */
    u16 acc_1g_ref;                 /**< 自学习得到的 1g 加速度模长参考值 */

    int32 gyro_bias_sum_x;          /**< X 轴陀螺仪零偏累计值 */
    int32 gyro_bias_sum_y;          /**< Y 轴陀螺仪零偏累计值 */
    int32 gyro_bias_sum_z;          /**< Z 轴陀螺仪零偏累计值 */
    u16 gyro_bias_count;            /**< 陀螺仪零偏学习计数 */
    int16 gyro_bias_x;              /**< X 轴陀螺仪零偏 */
    int16 gyro_bias_y;              /**< Y 轴陀螺仪零偏 */
    int16 gyro_bias_z;              /**< Z 轴陀螺仪零偏 */
    u8 gyro_bias_ready;             /**< 陀螺仪零偏是否已学习完成 */

    AHRS_Lpf3_t gyro_lpf;           /**< 陀螺仪三轴低通状态 */
    AHRS_Lpf3_t mag_lpf;            /**< 地磁计三轴低通状态 */

    AHRS_State_t state;             /**< 对外输出状态快照 */
    u8 mag_valid;                   /**< 最近一次地磁数据是否有效 */
    u8 ready;                       /**< AHRS 是否已完成首帧姿态初始化 */
} AHRS_Context_t;

static AHRS_Context_t ahrs_ctx;

/**
 * @brief      求 int16 绝对值并扩展为 u32
 * @param[in]  value  输入有符号 16 位数
 * @return     输入值的绝对值
 */
static u32 AHRS_Abs16(int16 value)
{
    if (value < 0) {
        return (u32)(-(int32)value);
    }
    return (u32)value;
}

/**
 * @brief      求 int32 绝对值
 * @param[in]  value  输入有符号 32 位数
 * @return     输入值的绝对值
 */
static int32 AHRS_Abs32(int32 value)
{
    if (value < 0) {
        return -value;
    }
    return value;
}

/**
 * @brief      将角度约束到 -18000~17999 附近
 * @param[in]  angle  输入角度，单位 deg * 100
 * @return     约束后的角度，单位 deg * 100
 */
static int16 AHRS_WrapDeg100(int32 angle)
{
    while (angle >= AHRS_ANGLE_HALF_DEG100) {
        angle -= AHRS_ANGLE_FULL_DEG100;
    }
    while (angle < -AHRS_ANGLE_HALF_DEG100) {
        angle += AHRS_ANGLE_FULL_DEG100;
    }
    return (int16)angle;
}

/**
 * @brief      将角度误差约束到最短方向
 * @param[in]  angle  输入角度误差，单位 deg * 100
 * @return     约束后的角度误差，单位 deg * 100
 */
static int16 AHRS_WrapDiffDeg100(int32 angle)
{
    return AHRS_WrapDeg100(angle);
}

/**
 * @brief      将 Q8 角度状态约束到 -180~180 度
 * @param[in,out] angle_q8  指向 Q8 角度状态的指针，单位 deg * 100
 * @return     none
 *
 * @details
 * 只做角度回绕，不丢弃 Q8 小数部分，保证低速积分不会被量化截断。
 */
static void AHRS_WrapAngleQ8(int32 *angle_q8)
{
    int32 half_q8;
    int32 full_q8;

    half_q8 = AHRS_ANGLE_HALF_DEG100 * AHRS_Q_SCALE;
    full_q8 = AHRS_ANGLE_FULL_DEG100 * AHRS_Q_SCALE;

    while (*angle_q8 >= half_q8) {
        *angle_q8 -= full_q8;
    }
    while (*angle_q8 < -half_q8) {
        *angle_q8 += full_q8;
    }
}

/**
 * @brief      根据映射配置选择并翻转原始轴
 * @param[in]  raw_x      原始 X 轴数据
 * @param[in]  raw_y      原始 Y 轴数据
 * @param[in]  raw_z      原始 Z 轴数据
 * @param[in]  from_axis  目标轴来自哪个原始轴
 * @param[in]  sign       目标轴符号，1=同向，-1=反向
 * @return     映射后的单轴数据
 */
static int16 AHRS_SelectMappedAxis(int16 raw_x, int16 raw_y, int16 raw_z,
                                   u8 from_axis, int8 sign)
{
    int16 value;

    value = raw_x;
    if (from_axis == AHRS_AXIS_RAW_Y) {
        value = raw_y;
    } else if (from_axis == AHRS_AXIS_RAW_Z) {
        value = raw_z;
    }

    if (sign < 0) {
        value = (int16)(-value);
    }
    return value;
}

/**
 * @brief      近似计算二维向量模长
 * @param[in]  a  第一轴绝对值
 * @param[in]  b  第二轴绝对值
 * @return     二维模长近似值
 *
 * @details
 * 使用 `max + 3/8 * min` 的整数近似，避免平方根运算。
 */
static u32 AHRS_Norm2Approx(u32 a, u32 b)
{
    u32 max_v;
    u32 min_v;

    if (a >= b) {
        max_v = a;
        min_v = b;
    } else {
        max_v = b;
        min_v = a;
    }

    return max_v + ((min_v * 3U) >> 3);
}

/**
 * @brief      近似计算三维向量模长
 * @param[in]  x  X 轴输入
 * @param[in]  y  Y 轴输入
 * @param[in]  z  Z 轴输入
 * @return     三维模长近似值
 *
 * @details
 * 使用排序后的 `max + 3/8 * mid + 1/4 * min`，用于加速度和地磁有效性判断。
 */
static u32 AHRS_Norm3Approx(int16 x, int16 y, int16 z)
{
    u32 a;
    u32 b;
    u32 c;
    u32 t;

    a = AHRS_Abs16(x);
    b = AHRS_Abs16(y);
    c = AHRS_Abs16(z);

    if (a < b) {
        t = a; a = b; b = t;
    }
    if (a < c) {
        t = a; a = c; c = t;
    }
    if (b < c) {
        t = b; b = c; c = t;
    }

    return a + ((b * 3U) >> 3) + (c >> 2);
}

/**
 * @brief      计算 0~1 范围内 atan 的整数近似
 * @param[in]  z_q10  输入比例，Q10 格式，1024 表示 1.0
 * @return     atan(z) 近似角度，单位 deg * 100
 */
static int16 AHRS_Atan01Q10ToDeg100(u16 z_q10)
{
    int32 z;
    int32 curve;
    int32 angle;

    if (z_q10 > AHRS_ATAN_Q) {
        z_q10 = (u16)AHRS_ATAN_Q;
    }

    z = (int32)z_q10;
    curve = 4500L + ((1564L * (AHRS_ATAN_Q - z)) / AHRS_ATAN_Q);
    angle = (z * curve) / AHRS_ATAN_Q;
    return (int16)angle;
}

/**
 * @brief      整数近似 atan2
 * @param[in]  y  atan2 的 y 输入
 * @param[in]  x  atan2 的 x 输入
 * @return     角度，单位 deg * 100，范围约为 -18000~17999
 *
 * @details
 * 该函数用于加速度姿态角和地磁航向角解算。精度不是数学库级别，
 * 但足够用于低成本船体姿态调试和互补滤波慢修正。
 */
static int16 AHRS_Atan2Deg100(int32 y, int32 x)
{
    u32 ax;
    u32 ay;
    u16 z_q10;
    int32 base;
    int32 angle;

    ax = (u32)AHRS_Abs32(x);
    ay = (u32)AHRS_Abs32(y);

    if ((ax == 0U) && (ay == 0U)) {
        return 0;
    }

    if (ax >= ay) {
        z_q10 = (u16)((ay * AHRS_ATAN_Q) / ax);
        base = AHRS_Atan01Q10ToDeg100(z_q10);
        if (x >= 0) {
            angle = base;
        } else {
            angle = AHRS_ANGLE_HALF_DEG100 - base;
        }
    } else {
        z_q10 = (u16)((ax * AHRS_ATAN_Q) / ay);
        base = AHRS_Atan01Q10ToDeg100(z_q10);
        if (x >= 0) {
            angle = AHRS_ANGLE_QUARTER_DEG100 - base;
        } else {
            angle = AHRS_ANGLE_QUARTER_DEG100 + base;
        }
    }

    if (y < 0) {
        angle = -angle;
    }

    return AHRS_WrapDeg100(angle);
}

/**
 * @brief      复位三轴低通状态
 * @param[out] lpf  指向三轴低通状态的指针
 * @return     none
 */
static void AHRS_Lpf3Reset(AHRS_Lpf3_t *lpf)
{
    if (lpf == 0) {
        return;
    }

    lpf->x = 0;
    lpf->y = 0;
    lpf->z = 0;
    lpf->initialized = 0;
}

/**
 * @brief      更新单轴一阶低通状态
 * @param[in,out] state  指向单轴 Q8 状态的指针
 * @param[in]     input  当前输入值
 * @param[in]     shift  低通强度，值越大响应越慢
 * @return        低通后的 int16 输出
 *
 * @details
 * 滤波公式：`state += ((input << Q) - state) >> shift`。
 */
static int16 AHRS_LpfAxisUpdate(int32 *state, int16 input, u8 shift)
{
    int32 target;

    target = (int32)input * AHRS_Q_SCALE;
    if (shift == 0U) {
        *state = target;
    } else {
        *state += ((target - *state) >> shift);
    }
    return (int16)(*state >> AHRS_Q_SHIFT);
}

/**
 * @brief      对三轴数据执行一阶低通
 * @param[in,out] lpf    指向三轴低通状态的指针
 * @param[in]     shift  低通强度，值越大响应越慢
 * @param[in]     in_x   X 轴输入
 * @param[in]     in_y   Y 轴输入
 * @param[in]     in_z   Z 轴输入
 * @param[out]    out_x  X 轴输出
 * @param[out]    out_y  Y 轴输出
 * @param[out]    out_z  Z 轴输出
 * @return        none
 *
 * @details
 * 首帧有效数据直接灌入状态，避免低通启动瞬间从 0 慢慢爬升造成姿态突跳。
 */
static void AHRS_Lpf3Apply(AHRS_Lpf3_t *lpf, u8 shift,
                           int16 in_x, int16 in_y, int16 in_z,
                           int16 *out_x, int16 *out_y, int16 *out_z)
{
    if (!lpf->initialized) {
        lpf->x = (int32)in_x * AHRS_Q_SCALE;
        lpf->y = (int32)in_y * AHRS_Q_SCALE;
        lpf->z = (int32)in_z * AHRS_Q_SCALE;
        lpf->initialized = 1;
        *out_x = in_x;
        *out_y = in_y;
        *out_z = in_z;
        return;
    }

    *out_x = AHRS_LpfAxisUpdate(&lpf->x, in_x, shift);
    *out_y = AHRS_LpfAxisUpdate(&lpf->y, in_y, shift);
    *out_z = AHRS_LpfAxisUpdate(&lpf->z, in_z, shift);
}

/**
 * @brief      更新加速度 1g 模长参考值
 * @param[in]  acc_norm  当前加速度模长近似值
 * @return     none
 *
 * @details
 * 启动阶段使用前 `AHRS_ACC_REF_SAMPLE_COUNT` 帧建立 1g 参考。建立后仅在模长
 * 接近参考值时做慢速跟踪，避免船体加速或撞击时污染 1g 参考。
 */
static void AHRS_UpdateAccReference(u32 acc_norm)
{
    int32 diff;

    if (acc_norm < AHRS_ACC_MIN_BOOT_NORM) {
        return;
    }

    if (ahrs_ctx.acc_ref_count < AHRS_ACC_REF_SAMPLE_COUNT) {
        ahrs_ctx.acc_ref_sum += acc_norm;
        ahrs_ctx.acc_ref_count++;
        if (ahrs_ctx.acc_ref_count >= AHRS_ACC_REF_SAMPLE_COUNT) {
            ahrs_ctx.acc_1g_ref =
                (u16)(ahrs_ctx.acc_ref_sum / AHRS_ACC_REF_SAMPLE_COUNT);
        }
        return;
    }

    if (ahrs_ctx.acc_1g_ref == 0U) {
        return;
    }

    diff = (int32)acc_norm - (int32)ahrs_ctx.acc_1g_ref;
    if (AHRS_Abs32(diff) <
        (((int32)ahrs_ctx.acc_1g_ref * AHRS_ACC_NORM_TOLERANCE_PERCENT) / 100L)) {
        ahrs_ctx.acc_1g_ref =
            (u16)((((u32)ahrs_ctx.acc_1g_ref * 255U) + acc_norm) >> 8);
    }
}

/**
 * @brief      判断加速度模长是否可信
 * @param[in]  acc_norm  当前加速度模长近似值
 * @return     1=可信，0=不可信
 *
 * @details
 * 当模长明显偏离 1g 参考值时，说明当前加速度中可能包含船体线加速度或冲击，
 * 此时暂停加速度对 roll/pitch 的修正，主要依赖陀螺仪短时积分。
 */
static u8 AHRS_IsAccNormValid(u32 acc_norm)
{
    u32 min_norm;
    u32 max_norm;

    if (acc_norm < AHRS_ACC_MIN_BOOT_NORM) {
        return 0;
    }

    if (ahrs_ctx.acc_1g_ref == 0U) {
        return 1;
    }

    min_norm =
        ((u32)ahrs_ctx.acc_1g_ref * (100U - AHRS_ACC_NORM_TOLERANCE_PERCENT)) /
        100U;
    max_norm =
        ((u32)ahrs_ctx.acc_1g_ref * (100U + AHRS_ACC_NORM_TOLERANCE_PERCENT)) /
        100U;

    if ((acc_norm < min_norm) || (acc_norm > max_norm)) {
        return 0;
    }
    return 1;
}

/**
 * @brief      判断陀螺仪是否处于近似静止状态
 * @param[in]  gx  X 轴陀螺仪原始值
 * @param[in]  gy  Y 轴陀螺仪原始值
 * @param[in]  gz  Z 轴陀螺仪原始值
 * @return     1=近似静止，0=正在运动
 */
static u8 AHRS_IsGyroStill(int16 gx, int16 gy, int16 gz)
{
    int32 threshold;

    threshold =
        (AHRS_GYRO_LSB_PER_DPS * AHRS_GYRO_STILL_DPS100) / 100L;

    if (AHRS_Abs16(gx) > (u32)threshold) {
        return 0;
    }
    if (AHRS_Abs16(gy) > (u32)threshold) {
        return 0;
    }
    if (AHRS_Abs16(gz) > (u32)threshold) {
        return 0;
    }
    return 1;
}

/**
 * @brief   清空陀螺仪零偏累计状态
 * @return  none
 */
static void AHRS_ResetGyroBiasAccum(void)
{
    ahrs_ctx.gyro_bias_sum_x = 0;
    ahrs_ctx.gyro_bias_sum_y = 0;
    ahrs_ctx.gyro_bias_sum_z = 0;
    ahrs_ctx.gyro_bias_count = 0;
}

/**
 * @brief      更新陀螺仪静止零偏学习状态
 * @param[in]  gx         X 轴陀螺仪原始值
 * @param[in]  gy         Y 轴陀螺仪原始值
 * @param[in]  gz         Z 轴陀螺仪原始值
 * @param[in]  acc_valid  当前加速度是否可信
 * @return     none
 *
 * @details
 * 只有在加速度可信且陀螺仪近似静止时才累计零偏。若中途检测到运动，
 * 立即清空累计，等待下一段静止窗口。
 */
static void AHRS_UpdateGyroBias(int16 gx, int16 gy, int16 gz, u8 acc_valid)
{
    if (ahrs_ctx.gyro_bias_ready) {
        return;
    }

    if ((!acc_valid) || (!AHRS_IsGyroStill(gx, gy, gz))) {
        AHRS_ResetGyroBiasAccum();
        return;
    }

    ahrs_ctx.gyro_bias_sum_x += gx;
    ahrs_ctx.gyro_bias_sum_y += gy;
    ahrs_ctx.gyro_bias_sum_z += gz;
    ahrs_ctx.gyro_bias_count++;

    if (ahrs_ctx.gyro_bias_count >= AHRS_GYRO_BIAS_SAMPLE_COUNT) {
        ahrs_ctx.gyro_bias_x =
            (int16)(ahrs_ctx.gyro_bias_sum_x / AHRS_GYRO_BIAS_SAMPLE_COUNT);
        ahrs_ctx.gyro_bias_y =
            (int16)(ahrs_ctx.gyro_bias_sum_y / AHRS_GYRO_BIAS_SAMPLE_COUNT);
        ahrs_ctx.gyro_bias_z =
            (int16)(ahrs_ctx.gyro_bias_sum_z / AHRS_GYRO_BIAS_SAMPLE_COUNT);
        ahrs_ctx.gyro_bias_ready = 1;
    }
}

/**
 * @brief      对陀螺仪原始值应用死区
 * @param[in]  gyro  陀螺仪原始输入
 * @return     应用死区后的陀螺仪值
 */
static int16 AHRS_ApplyGyroDeadband(int16 gyro)
{
    int32 deadband;
    int32 value;

    deadband =
        (AHRS_GYRO_LSB_PER_DPS * AHRS_GYRO_DEADBAND_DPS100) / 100L;
    if (deadband < 1L) {
        deadband = 1L;
    }

    value = gyro;
    if (AHRS_Abs32(value) <= deadband) {
        return 0;
    }

    if (value > 0) {
        value -= deadband;
    } else {
        value += deadband;
    }

    return (int16)value;
}

/**
 * @brief      将陀螺仪原始值换算为 deg/s * 100
 * @param[in]  gyro  陀螺仪原始输入
 * @return     角速度，单位 deg/s * 100
 */
static int16 AHRS_GyroToDps100(int16 gyro)
{
    return (int16)(((int32)gyro * 100L) / AHRS_GYRO_LSB_PER_DPS);
}

/**
 * @brief      计算单轴陀螺仪在 dt 内的角度增量
 * @param[in]  gyro   陀螺仪原始输入
 * @param[in]  dt_ms  时间间隔，单位 ms
 * @return     Q8 角度增量，单位 deg * 100
 */
static int32 AHRS_GyroDeltaQ8(int16 gyro, u16 dt_ms)
{
    return ((int32)gyro * (int32)dt_ms * AHRS_Q_SCALE) /
           (AHRS_GYRO_LSB_PER_DPS * 10L);
}

/**
 * @brief      对角度状态执行互补修正
 * @param[in,out] angle_q8       待修正的 Q8 角度状态
 * @param[in]     target_deg100  修正目标角，单位 deg * 100
 * @param[in]     shift          修正强度，值越大修正越慢
 * @return        none
 *
 * @details
 * 使用最短角度误差进行修正，避免跨越 -180/180 度时出现大角度跳变。
 */
static void AHRS_BlendAngleQ8(int32 *angle_q8, int16 target_deg100, u8 shift)
{
    int16 current_deg100;
    int16 diff_deg100;

    current_deg100 = AHRS_WrapDeg100(*angle_q8 >> AHRS_Q_SHIFT);
    diff_deg100 =
        AHRS_WrapDiffDeg100((int32)target_deg100 - (int32)current_deg100);

    if (shift == 0U) {
        *angle_q8 += (int32)diff_deg100 * AHRS_Q_SCALE;
    } else {
        *angle_q8 += (((int32)diff_deg100 * AHRS_Q_SCALE) >> shift);
    }
}

/**
 * @brief      刷新对外 AHRS 状态快照
 * @param[in]  dt_ms     最近一次融合使用的时间间隔
 * @param[in]  acc_norm  当前加速度模长近似值
 * @param[in]  flags     当前帧状态标志
 * @return     none
 */
static void AHRS_UpdateOutput(u16 dt_ms, u32 acc_norm, u8 flags)
{
    if (ahrs_ctx.mag_valid) {
        flags |= AHRS_FLAG_MAG_VALID;
    }

    AHRS_WrapAngleQ8(&ahrs_ctx.roll_q8);
    AHRS_WrapAngleQ8(&ahrs_ctx.pitch_q8);
    AHRS_WrapAngleQ8(&ahrs_ctx.yaw_q8);

    ahrs_ctx.state.roll_deg100 =
        AHRS_WrapDeg100(ahrs_ctx.roll_q8 >> AHRS_Q_SHIFT);
    ahrs_ctx.state.pitch_deg100 =
        AHRS_WrapDeg100(ahrs_ctx.pitch_q8 >> AHRS_Q_SHIFT);
    ahrs_ctx.state.yaw_deg100 =
        AHRS_WrapDeg100(ahrs_ctx.yaw_q8 >> AHRS_Q_SHIFT);

    ahrs_ctx.state.acc_norm = (acc_norm > 65535U) ? 65535U : (u16)acc_norm;
    ahrs_ctx.state.dt_ms = dt_ms;
    ahrs_ctx.state.flags = flags;
    ahrs_ctx.state.update_count++;
}

/**
 * @brief   复位 AHRS 姿态融合器
 * @return  none
 *
 * @details
 * 清空全部内部状态，下一帧有效加速度数据会重新初始化 roll/pitch，
 * yaw 默认从 0 开始，随后由陀螺积分和地磁慢修正更新。
 */
void AHRS_Reset(void)
{
    ahrs_ctx.roll_q8 = 0;
    ahrs_ctx.pitch_q8 = 0;
    ahrs_ctx.yaw_q8 = 0;
    ahrs_ctx.acc_roll_lpf_q8 = 0;
    ahrs_ctx.acc_pitch_lpf_q8 = 0;
    ahrs_ctx.acc_angle_initialized = 0;
    ahrs_ctx.mag_yaw_lpf_q8 = 0;
    ahrs_ctx.mag_yaw_initialized = 0;
    ahrs_ctx.acc_ref_sum = 0;
    ahrs_ctx.acc_ref_count = 0;
    ahrs_ctx.acc_1g_ref = 0;
    ahrs_ctx.gyro_bias_x = 0;
    ahrs_ctx.gyro_bias_y = 0;
    ahrs_ctx.gyro_bias_z = 0;
    ahrs_ctx.gyro_bias_ready = 0;
    AHRS_ResetGyroBiasAccum();
    AHRS_Lpf3Reset(&ahrs_ctx.gyro_lpf);
    AHRS_Lpf3Reset(&ahrs_ctx.mag_lpf);
    ahrs_ctx.state.roll_deg100 = 0;
    ahrs_ctx.state.pitch_deg100 = 0;
    ahrs_ctx.state.yaw_deg100 = 0;
    ahrs_ctx.state.gyro_x_dps100 = 0;
    ahrs_ctx.state.gyro_y_dps100 = 0;
    ahrs_ctx.state.gyro_z_dps100 = 0;
    ahrs_ctx.state.acc_norm = 0;
    ahrs_ctx.state.dt_ms = 0;
    ahrs_ctx.state.update_count = 0;
    ahrs_ctx.state.flags = 0;
    ahrs_ctx.mag_valid = 0;
    ahrs_ctx.ready = 0;
}

/**
 * @brief      使用船体系 6 轴数据执行一次姿态融合
 * @param[in]  ax     船体系 X 轴加速度原始值
 * @param[in]  ay     船体系 Y 轴加速度原始值
 * @param[in]  az     船体系 Z 轴加速度原始值
 * @param[in]  gx     船体系 X 轴陀螺仪原始值
 * @param[in]  gy     船体系 Y 轴陀螺仪原始值
 * @param[in]  gz     船体系 Z 轴陀螺仪原始值
 * @param[in]  dt_ms  与上次融合间隔，单位 ms
 * @return     0=融合成功，-1=参数无效或首帧尚未建立姿态
 *
 * @details
 * 处理顺序：
 * 1. 估算加速度模长并判断当前加速度是否可信
 * 2. 在静止窗口内学习陀螺仪零偏
 * 3. 对陀螺仪做零偏扣除、死区和低通
 * 4. 陀螺仪积分预测姿态
 * 5. 加速度慢速修正 roll/pitch
 */
s8 AHRS_Update6Axis(int16 ax, int16 ay, int16 az,
                    int16 gx, int16 gy, int16 gz,
                    u16 dt_ms)
{
    u32 acc_norm;
    u32 pitch_den;
    int16 acc_roll;
    int16 acc_pitch;
    int16 fgx;
    int16 fgy;
    int16 fgz;
    u8 flags;
    u8 acc_valid;

    if (dt_ms == 0U) {
        return -1;
    }

    flags = 0;
    if (dt_ms > AHRS_DT_MAX_MS) {
        dt_ms = AHRS_DT_MAX_MS;
        flags |= AHRS_FLAG_DT_CLAMPED;
    }

    acc_norm = AHRS_Norm3Approx(ax, ay, az);
    AHRS_UpdateAccReference(acc_norm);
    acc_valid = AHRS_IsAccNormValid(acc_norm);
    if (acc_valid) {
        flags |= AHRS_FLAG_ACC_VALID;
    }
    if (ahrs_ctx.acc_1g_ref != 0U) {
        flags |= AHRS_FLAG_ACC_REF_READY;
    }

    AHRS_UpdateGyroBias(gx, gy, gz, acc_valid);
    if (ahrs_ctx.gyro_bias_ready) {
        flags |= AHRS_FLAG_GYRO_BIAS_READY;
    }

    gx = (int16)(gx - ahrs_ctx.gyro_bias_x);
    gy = (int16)(gy - ahrs_ctx.gyro_bias_y);
    gz = (int16)(gz - ahrs_ctx.gyro_bias_z);

    gx = AHRS_ApplyGyroDeadband(gx);
    gy = AHRS_ApplyGyroDeadband(gy);
    gz = AHRS_ApplyGyroDeadband(gz);

    AHRS_Lpf3Apply(&ahrs_ctx.gyro_lpf, AHRS_GYRO_LPF_SHIFT,
                   gx, gy, gz, &fgx, &fgy, &fgz);

    ahrs_ctx.state.gyro_x_dps100 = AHRS_GyroToDps100(fgx);
    ahrs_ctx.state.gyro_y_dps100 = AHRS_GyroToDps100(fgy);
    ahrs_ctx.state.gyro_z_dps100 = AHRS_GyroToDps100(fgz);

    pitch_den = AHRS_Norm2Approx(AHRS_Abs16(ay), AHRS_Abs16(az));
    acc_roll = AHRS_Atan2Deg100(-(int32)ay, (int32)az);
    acc_pitch = AHRS_Atan2Deg100((int32)ax, (int32)pitch_den);

    if (acc_valid) {
        if (!ahrs_ctx.acc_angle_initialized) {
            ahrs_ctx.acc_roll_lpf_q8 = (int32)acc_roll * AHRS_Q_SCALE;
            ahrs_ctx.acc_pitch_lpf_q8 = (int32)acc_pitch * AHRS_Q_SCALE;
            ahrs_ctx.acc_angle_initialized = 1;
        } else {
            AHRS_BlendAngleQ8(&ahrs_ctx.acc_roll_lpf_q8,
                              acc_roll, AHRS_ACC_ANGLE_LPF_SHIFT);
            AHRS_BlendAngleQ8(&ahrs_ctx.acc_pitch_lpf_q8,
                              acc_pitch, AHRS_ACC_ANGLE_LPF_SHIFT);
        }
    }

    if (!ahrs_ctx.ready) {
        if (!ahrs_ctx.acc_angle_initialized) {
            AHRS_UpdateOutput(dt_ms, acc_norm, flags);
            return -1;
        }
        ahrs_ctx.roll_q8 = ahrs_ctx.acc_roll_lpf_q8;
        ahrs_ctx.pitch_q8 = ahrs_ctx.acc_pitch_lpf_q8;
        ahrs_ctx.yaw_q8 = 0;
        ahrs_ctx.ready = 1;
    } else {
        ahrs_ctx.roll_q8 += AHRS_GyroDeltaQ8(fgx, dt_ms);
        ahrs_ctx.pitch_q8 += AHRS_GyroDeltaQ8(fgy, dt_ms);
        ahrs_ctx.yaw_q8 += AHRS_GyroDeltaQ8(fgz, dt_ms);
    }

    if (acc_valid && ahrs_ctx.acc_angle_initialized) {
        AHRS_BlendAngleQ8(&ahrs_ctx.roll_q8,
                          AHRS_WrapDeg100(ahrs_ctx.acc_roll_lpf_q8 >>
                                          AHRS_Q_SHIFT),
                          AHRS_ACC_BLEND_SHIFT);
        AHRS_BlendAngleQ8(&ahrs_ctx.pitch_q8,
                          AHRS_WrapDeg100(ahrs_ctx.acc_pitch_lpf_q8 >>
                                          AHRS_Q_SHIFT),
                          AHRS_ACC_BLEND_SHIFT);
    }

    flags |= AHRS_FLAG_READY;
    AHRS_UpdateOutput(dt_ms, acc_norm, flags);
    return 0;
}

/**
 * @brief      使用 IMU 原始 6 轴数据执行一次姿态融合
 * @param[in]  raw_ax  IMU 原始 X 轴加速度
 * @param[in]  raw_ay  IMU 原始 Y 轴加速度
 * @param[in]  raw_az  IMU 原始 Z 轴加速度
 * @param[in]  raw_gx  IMU 原始 X 轴陀螺仪
 * @param[in]  raw_gy  IMU 原始 Y 轴陀螺仪
 * @param[in]  raw_gz  IMU 原始 Z 轴陀螺仪
 * @param[in]  dt_ms   与上次融合间隔，单位 ms
 * @return     0=融合成功，-1=参数无效或首帧尚未建立姿态
 *
 * @details
 * 本函数会先调用 `AHRS_MapRawToBody()` 完成原始传感器轴到船体系轴的转换。
 */
s8 AHRS_UpdateRaw6Axis(int16 raw_ax, int16 raw_ay, int16 raw_az,
                       int16 raw_gx, int16 raw_gy, int16 raw_gz,
                       u16 dt_ms)
{
    int16 ax;
    int16 ay;
    int16 az;
    int16 gx;
    int16 gy;
    int16 gz;

    AHRS_MapRawToBody(raw_ax, raw_ay, raw_az, &ax, &ay, &az);
    AHRS_MapRawToBody(raw_gx, raw_gy, raw_gz, &gx, &gy, &gz);

    return AHRS_Update6Axis(ax, ay, az, gx, gy, gz, dt_ms);
}

/**
 * @brief      使用船体系地磁数据执行航向慢修正
 * @param[in]  mx  船体系 X 轴地磁原始值
 * @param[in]  my  船体系 Y 轴地磁原始值
 * @param[in]  mz  船体系 Z 轴地磁原始值
 * @return     0=修正成功，-1=地磁数据无效
 *
 * @details
 * 当前实现只做水平面 `atan2(my, mx)` 航向修正，未做倾斜补偿。
 * 对小船调试而言，这能保持参数简单；后续若需要大倾角航向，可在此处扩展倾斜补偿。
 */
s8 AHRS_UpdateMag(int16 mx, int16 my, int16 mz)
{
    u32 mag_norm;
    int16 fmx;
    int16 fmy;
    int16 fmz;
    int16 mag_yaw;

    mag_norm = AHRS_Norm3Approx(mx, my, mz);
    if (mag_norm < AHRS_MAG_MIN_NORM) {
        ahrs_ctx.mag_valid = 0;
        ahrs_ctx.state.flags &= (u8)(~AHRS_FLAG_MAG_VALID);
        return -1;
    }

    AHRS_Lpf3Apply(&ahrs_ctx.mag_lpf, AHRS_MAG_LPF_SHIFT,
                   mx, my, mz, &fmx, &fmy, &fmz);
    mag_yaw = AHRS_Atan2Deg100((int32)fmy, (int32)fmx);

    if (!ahrs_ctx.mag_yaw_initialized) {
        ahrs_ctx.mag_yaw_lpf_q8 = (int32)mag_yaw * AHRS_Q_SCALE;
        ahrs_ctx.mag_yaw_initialized = 1;
    } else {
        AHRS_BlendAngleQ8(&ahrs_ctx.mag_yaw_lpf_q8,
                          mag_yaw, AHRS_MAG_LPF_SHIFT);
    }

#if AHRS_MAG_ENABLE
    if (ahrs_ctx.ready) {
        AHRS_BlendAngleQ8(&ahrs_ctx.yaw_q8,
                          AHRS_WrapDeg100(ahrs_ctx.mag_yaw_lpf_q8 >>
                                          AHRS_Q_SHIFT),
                          AHRS_MAG_BLEND_SHIFT);
        AHRS_WrapAngleQ8(&ahrs_ctx.yaw_q8);
        ahrs_ctx.state.yaw_deg100 =
            AHRS_WrapDeg100(ahrs_ctx.yaw_q8 >> AHRS_Q_SHIFT);
    }
#endif

    ahrs_ctx.mag_valid = 1;
    ahrs_ctx.state.flags |= AHRS_FLAG_MAG_VALID;
    return 0;
}

/**
 * @brief      使用地磁计原始数据执行航向慢修正
 * @param[in]  raw_mx  地磁计原始 X 轴数据
 * @param[in]  raw_my  地磁计原始 Y 轴数据
 * @param[in]  raw_mz  地磁计原始 Z 轴数据
 * @return     0=修正成功，-1=地磁数据无效
 */
s8 AHRS_UpdateRawMag(int16 raw_mx, int16 raw_my, int16 raw_mz)
{
    int16 mx;
    int16 my;
    int16 mz;

    AHRS_MapRawMagToBody(raw_mx, raw_my, raw_mz, &mx, &my, &mz);
    return AHRS_UpdateMag(mx, my, mz);
}

/**
 * @brief   获取 AHRS 当前状态
 * @return  指向内部 `AHRS_State_t` 状态快照的指针
 */
const AHRS_State_t *AHRS_GetState(void)
{
    return &ahrs_ctx.state;
}

/**
 * @brief   判断 AHRS 是否就绪
 * @return  1=已就绪，0=未就绪
 */
u8 AHRS_IsReady(void)
{
    return ahrs_ctx.ready;
}

/**
 * @brief      将 IMU 原始三轴数据映射到船体系
 * @param[in]  raw_x   原始 X 轴数据
 * @param[in]  raw_y   原始 Y 轴数据
 * @param[in]  raw_z   原始 Z 轴数据
 * @param[out] body_x  船体系 X 轴输出
 * @param[out] body_y  船体系 Y 轴输出
 * @param[out] body_z  船体系 Z 轴输出
 * @return     none
 */
void AHRS_MapRawToBody(int16 raw_x, int16 raw_y, int16 raw_z,
                       int16 *body_x, int16 *body_y, int16 *body_z)
{
    if ((body_x == 0) || (body_y == 0) || (body_z == 0)) {
        return;
    }

    *body_x = AHRS_SelectMappedAxis(raw_x, raw_y, raw_z,
                                    AHRS_IMU_BODY_X_FROM,
                                    AHRS_IMU_BODY_X_SIGN);
    *body_y = AHRS_SelectMappedAxis(raw_x, raw_y, raw_z,
                                    AHRS_IMU_BODY_Y_FROM,
                                    AHRS_IMU_BODY_Y_SIGN);
    *body_z = AHRS_SelectMappedAxis(raw_x, raw_y, raw_z,
                                    AHRS_IMU_BODY_Z_FROM,
                                    AHRS_IMU_BODY_Z_SIGN);
}

/**
 * @brief      将地磁计原始三轴数据映射到船体系
 * @param[in]  raw_x   原始 X 轴地磁数据
 * @param[in]  raw_y   原始 Y 轴地磁数据
 * @param[in]  raw_z   原始 Z 轴地磁数据
 * @param[out] body_x  船体系 X 轴输出
 * @param[out] body_y  船体系 Y 轴输出
 * @param[out] body_z  船体系 Z 轴输出
 * @return     none
 */
void AHRS_MapRawMagToBody(int16 raw_x, int16 raw_y, int16 raw_z,
                          int16 *body_x, int16 *body_y, int16 *body_z)
{
    if ((body_x == 0) || (body_y == 0) || (body_z == 0)) {
        return;
    }

    *body_x = AHRS_SelectMappedAxis(raw_x, raw_y, raw_z,
                                    AHRS_MAG_BODY_X_FROM,
                                    AHRS_MAG_BODY_X_SIGN);
    *body_y = AHRS_SelectMappedAxis(raw_x, raw_y, raw_z,
                                    AHRS_MAG_BODY_Y_FROM,
                                    AHRS_MAG_BODY_Y_SIGN);
    *body_z = AHRS_SelectMappedAxis(raw_x, raw_y, raw_z,
                                    AHRS_MAG_BODY_Z_FROM,
                                    AHRS_MAG_BODY_Z_SIGN);
}
