/**
 * @file    AHRS.h
 * @brief   AHRS 姿态解算模块公开接口。
 * @author  boweny
 * @date    2026-05-11
 * @version v1.2
 *
 * 该模块使用四元数保存内部姿态，外部保持现有的 deg*100 /
 * deg/s*100 整数输出接口，便于主循环、日志和上位机查看。
 *
 * 更新流程分为：
 * - 6 轴更新：加速度计 + 陀螺仪。
 * - 磁力计更新：独立更新磁场观测，供 Mahony 修正和诊断航向使用。
 *
 * @note 公开状态中的角度单位均为 0.01 度。
 * @note 原始传感器坐标到船体坐标的映射由本头文件中的轴映射宏控制。
 */

/**
 * @note 当前架构职责边界：
 * - AHRS 对外输出原始融合后的横滚/俯仰/偏航与陀螺状态。
 * - 磁力计在此处只参与原始融合航向的修正。
 * - MainLoop + NorthCalib 会在更上层决定是否叠加导航北向偏移。
 * - 传感器原始坐标到船体坐标的映射仍由本头文件负责。
 */
#ifndef __AHRS_H__
#define __AHRS_H__

#include "config.h"

/** @brief IMU 推荐更新周期，单位：ms。 */
#define AHRS_IMU_PERIOD_MS               17U

/** @brief 磁力计推荐更新周期，单位：ms。 */
#define AHRS_MAG_PERIOD_MS               100U

/** @brief AHRS 单次更新允许的最大 dt，超过后会被钳制，单位：ms。 */
#define AHRS_DT_MAX_MS                   50U

/** @brief 陀螺仪原始值到 deg/s 的比例，表示每 1 deg/s 对应的 LSB。 */
#define AHRS_GYRO_LSB_PER_DPS            128L

/** @brief 判断静止状态的陀螺阈值，单位：0.01 deg/s。 */
#define AHRS_GYRO_STILL_DPS100           200L

/** @brief 陀螺零偏跟踪低通滤波右移位数。 */
#define AHRS_GYRO_BIAS_TRACK_SHIFT       6U

/** @brief 陀螺死区阈值，单位：0.01 deg/s。 */
#define AHRS_GYRO_DEADBAND_DPS100        12L

/** @brief 陀螺输入低通滤波右移位数。 */
#define AHRS_GYRO_LPF_SHIFT              2U

/** @brief 建立 1g 加速度参考所需的静止采样数量。 */
#define AHRS_ACC_REF_SAMPLE_COUNT        32U

/** @brief 加速度模长相对 1g 参考的允许偏差百分比。 */
#define AHRS_ACC_NORM_TOLERANCE_PERCENT  35U

/** @brief 建立初始陀螺零偏所需的静止采样数量。 */
#define AHRS_GYRO_BIAS_SAMPLE_COUNT      128U

/**
 * @def AHRS_MAG_ENABLE
 * @brief 是否允许 AHRS 内部 Mahony 更新使用磁力计。
 *
 * 为 1 时，磁力计有效且加速度有效时可参与姿态修正。
 * 为 0 时，磁力计只可作为外部诊断输入，不参与 Mahony 修正。
 */
#ifndef AHRS_MAG_ENABLE
#define AHRS_MAG_ENABLE                  1
#endif

/** @brief 磁力计输入低通滤波右移位数。 */
#define AHRS_MAG_LPF_SHIFT               3U

/** @brief 磁力计模长最小有效阈值，低于该值认为磁力计无效。 */
#define AHRS_MAG_MIN_NORM                50U

/** @brief Mahony 算法中加速度计比例修正增益。 */
#define AHRS_MAHONY_KP_ACC               1.00f

/** @brief Mahony 算法中磁力计比例修正增益。 */
#define AHRS_MAHONY_KP_MAG               0.15f

/** @brief Mahony 算法积分修正增益；当前为 0 表示关闭积分项。 */
#define AHRS_MAHONY_KI                   0.00f

/** @brief 原始传感器 X 轴编号。 */
#define AHRS_AXIS_RAW_X                  0U

/** @brief 原始传感器 Y 轴编号。 */
#define AHRS_AXIS_RAW_Y                  1U

/** @brief 原始传感器 Z 轴编号。 */
#define AHRS_AXIS_RAW_Z                  2U

/** @brief IMU 船体 X 轴来源原始轴。 */
#define AHRS_IMU_BODY_X_FROM             AHRS_AXIS_RAW_X

/** @brief IMU 船体 X 轴方向，1 表示同向，-1 表示反向。 */
#define AHRS_IMU_BODY_X_SIGN             1

/** @brief IMU 船体 Y 轴来源原始轴。 */
#define AHRS_IMU_BODY_Y_FROM             AHRS_AXIS_RAW_Y

/** @brief IMU 船体 Y 轴方向，1 表示同向，-1 表示反向。 */
#define AHRS_IMU_BODY_Y_SIGN             1

/** @brief IMU 船体 Z 轴来源原始轴。 */
#define AHRS_IMU_BODY_Z_FROM             AHRS_AXIS_RAW_Z

/** @brief IMU 船体 Z 轴方向，1 表示同向，-1 表示反向。 */
#define AHRS_IMU_BODY_Z_SIGN             1

/** @brief 磁力计船体 X 轴来源原始轴。 */
#define AHRS_MAG_BODY_X_FROM             AHRS_AXIS_RAW_X

/** @brief 磁力计船体 X 轴方向，1 表示同向，-1 表示反向。 */
#define AHRS_MAG_BODY_X_SIGN             1

/** @brief 磁力计船体 Y 轴来源原始轴。 */
#define AHRS_MAG_BODY_Y_FROM             AHRS_AXIS_RAW_Y

/** @brief 磁力计船体 Y 轴方向，1 表示同向，-1 表示反向。 */
#define AHRS_MAG_BODY_Y_SIGN             1

/** @brief 磁力计船体 Z 轴来源原始轴。 */
#define AHRS_MAG_BODY_Z_FROM             AHRS_AXIS_RAW_Z

/** @brief 磁力计船体 Z 轴方向，1 表示同向，-1 表示反向。 */
#define AHRS_MAG_BODY_Z_SIGN             1

/** @brief AHRS 已完成初始化并可输出有效姿态。 */
#define AHRS_FLAG_READY                  0x01U

/** @brief 本次更新中加速度计模长有效。 */
#define AHRS_FLAG_ACC_VALID              0x02U

/** @brief 当前磁力计观测有效。 */
#define AHRS_FLAG_MAG_VALID              0x04U

/** @brief 陀螺零偏已经建立。 */
#define AHRS_FLAG_GYRO_BIAS_READY        0x08U

/** @brief 1g 加速度参考已经建立。 */
#define AHRS_FLAG_ACC_REF_READY          0x10U

/** @brief 本次更新 dt 被钳制到 AHRS_DT_MAX_MS。 */
#define AHRS_FLAG_DT_CLAMPED             0x20U

/**
 * @brief AHRS 对外输出状态。
 *
 * 该结构体由 AHRS 模块内部维护，上层通过 AHRS_GetState() 获取只读指针。
 */
typedef struct
{
    /** 横滚角，单位：0.01 deg，范围约束为 [-18000, 18000)。 */
    int16 roll_deg100;       /**< 横滚角，单位 0.01 deg，范围约束为 [-18000, 18000)。 */

    /** 俯仰角，单位：0.01 deg，范围约束为 [-18000, 18000)。 */
    int16 pitch_deg100;      /**< 俯仰角，单位 0.01 deg，范围约束为 [-18000, 18000)。 */

    /** 四元数解算得到的偏航角，单位：0.01 deg，范围约束为 [-18000, 18000)。 */
    int16 yaw_deg100;        /**< 四元数解算偏航角，单位 0.01 deg，范围约束为 [-18000, 18000)。 */

    /** 仅由 Z 轴陀螺积分得到的偏航角，单位：0.01 deg。 */
    int16 yaw_gyro_deg100;   /**< 仅由 Z 轴陀螺积分得到的偏航角，单位 0.01 deg。 */

    /** 由磁力计诊断计算得到的偏航角，单位：0.01 deg。 */
    int16 yaw_mag_deg100;    /**< 由磁力计诊断计算得到的偏航角，单位 0.01 deg。 */

    /** X 轴陀螺角速度，单位：0.01 deg/s，已做零偏、死区和低通处理。 */
    int16 gyro_x_dps100;     /**< X 轴陀螺角速度，单位 0.01 deg/s，已做零偏、死区和低通处理。 */

    /** Y 轴陀螺角速度，单位：0.01 deg/s，已做零偏、死区和低通处理。 */
    int16 gyro_y_dps100;     /**< Y 轴陀螺角速度，单位 0.01 deg/s，已做零偏、死区和低通处理。 */

    /** Z 轴陀螺角速度，单位：0.01 deg/s，已做零偏、死区和低通处理。 */
    int16 gyro_z_dps100;     /**< Z 轴陀螺角速度，单位 0.01 deg/s，已做零偏、死区和低通处理。 */

    /** 加速度三轴模长近似值，使用原始计数单位。 */
    u16   acc_norm;          /**< 加速度三轴模长近似值，使用原始计数单位。 */

    /** 最近一次 AHRS 更新使用的 dt，单位：ms。 */
    u16   dt_ms;             /**< 最近一次 AHRS 更新使用的 dt，单位 ms。 */

    /** AHRS 更新计数，每次输出状态刷新后递增。 */
    u16   update_count;      /**< AHRS 更新计数，每次输出状态刷新后递增。 */

    /** 状态标志位，取值为 AHRS_FLAG_* 的按位组合。 */
    u8    flags;             /**< 状态标志位，取值为 AHRS_FLAG_* 的按位组合。 */
} AHRS_State_t;

/**
 * @brief 复位 AHRS 内部状态。
 *
 * 会清空四元数、积分项、磁力计状态、陀螺零偏、滤波器和对外状态。
 * 上电初始化或传感器状态异常恢复时调用。
 */
void AHRS_Reset(void);

/**
 * @brief 使用船体坐标系下的 6 轴 IMU 数据更新 AHRS。
 *
 * 输入数据应已经完成传感器原始坐标到船体坐标的映射。
 *
 * @param ax 船体 X 轴加速度原始计数。
 * @param ay 船体 Y 轴加速度原始计数。
 * @param az 船体 Z 轴加速度原始计数。
 * @param gx 船体 X 轴陀螺原始计数。
 * @param gy 船体 Y 轴陀螺原始计数。
 * @param gz 船体 Z 轴陀螺原始计数。
 * @param dt_ms 本次更新周期，单位：ms；为 0 时返回失败。
 * @return 0 表示更新成功，-1 表示数据不足或尚未 ready。
 */
s8 AHRS_Update6Axis(int16 ax, int16 ay, int16 az,
                    int16 gx, int16 gy, int16 gz,
                    u16 dt_ms);

/**
 * @brief 使用原始 6 轴 IMU 数据更新 AHRS。
 *
 * 函数内部会先调用 AHRS_MapRawToBody() 将原始坐标映射到船体坐标，
 * 再调用 AHRS_Update6Axis()。
 *
 * @param raw_ax 原始 X 轴加速度计数。
 * @param raw_ay 原始 Y 轴加速度计数。
 * @param raw_az 原始 Z 轴加速度计数。
 * @param raw_gx 原始 X 轴陀螺计数。
 * @param raw_gy 原始 Y 轴陀螺计数。
 * @param raw_gz 原始 Z 轴陀螺计数。
 * @param dt_ms 本次更新周期，单位：ms。
 * @return 0 表示更新成功，-1 表示数据不足或尚未 ready。
 */
s8 AHRS_UpdateRaw6Axis(int16 raw_ax, int16 raw_ay, int16 raw_az,
                       int16 raw_gx, int16 raw_gy, int16 raw_gz,
                       u16 dt_ms);

/**
 * @brief 更新船体坐标系下的磁力计数据。
 *
 * 输入数据应已经完成传感器原始坐标到船体坐标的映射。函数内部会进行
 * 模长有效性检查、低通滤波和单位化处理。
 *
 * @param mx 船体 X 轴磁力计原始计数。
 * @param my 船体 Y 轴磁力计原始计数。
 * @param mz 船体 Z 轴磁力计原始计数。
 * @return 0 表示磁力计数据有效，-1 表示数据无效。
 */
s8 AHRS_UpdateMag(int16 mx, int16 my, int16 mz);

/**
 * @brief 更新原始磁力计数据。
 *
 * 函数内部会先调用 AHRS_MapRawMagToBody() 将原始磁力计坐标映射到
 * 船体坐标，再调用 AHRS_UpdateMag()。
 *
 * @param raw_mx 原始 X 轴磁力计计数。
 * @param raw_my 原始 Y 轴磁力计计数。
 * @param raw_mz 原始 Z 轴磁力计计数。
 * @return 0 表示磁力计数据有效，-1 表示数据无效。
 */
s8 AHRS_UpdateRawMag(int16 raw_mx, int16 raw_my, int16 raw_mz);

/**
 * @brief 获取 AHRS 当前对外状态。
 *
 * @return AHRS_State_t 只读指针，指向模块内部静态状态。
 */
const AHRS_State_t *AHRS_GetState(void);

/**
 * @brief 判断 AHRS 是否已经 ready。
 *
 * @return 非 0 表示已经完成初始姿态建立，0 表示尚未 ready。
 */
u8 AHRS_IsReady(void);

/**
 * @brief 将 IMU 原始坐标映射到船体坐标。
 *
 * 映射关系由 AHRS_IMU_BODY_*_FROM 和 AHRS_IMU_BODY_*_SIGN 宏控制。
 * 任一输出指针为空时函数直接返回。
 *
 * @param raw_x 原始 X 轴输入。
 * @param raw_y 原始 Y 轴输入。
 * @param raw_z 原始 Z 轴输入。
 * @param body_x 输出船体 X 轴。
 * @param body_y 输出船体 Y 轴。
 * @param body_z 输出船体 Z 轴。
 */
void AHRS_MapRawToBody(int16 raw_x, int16 raw_y, int16 raw_z,
                       int16 *body_x, int16 *body_y, int16 *body_z);

/**
 * @brief 将磁力计原始坐标映射到船体坐标。
 *
 * 映射关系由 AHRS_MAG_BODY_*_FROM 和 AHRS_MAG_BODY_*_SIGN 宏控制。
 * 任一输出指针为空时函数直接返回。
 *
 * @param raw_x 原始 X 轴磁力计输入。
 * @param raw_y 原始 Y 轴磁力计输入。
 * @param raw_z 原始 Z 轴磁力计输入。
 * @param body_x 输出船体 X 轴磁力计。
 * @param body_y 输出船体 Y 轴磁力计。
 * @param body_z 输出船体 Z 轴磁力计。
 */
void AHRS_MapRawMagToBody(int16 raw_x, int16 raw_y, int16 raw_z,
                          int16 *body_x, int16 *body_y, int16 *body_z);

#endif
