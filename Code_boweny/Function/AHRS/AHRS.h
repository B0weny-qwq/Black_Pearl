/**
 * @file    AHRS.h
 * @brief   AHRS 姿态融合模块接口。
 * @author  boweny
 * @date    2026-05-05
 * @version v1.0
 *
 * @details
 * 面向 Black Pearl 船体控制工程，使用 QMI8658 六轴 IMU 与 QMC6309
 * 地磁计完成轻量级定点姿态估计。输出角度单位为 deg * 100，
 * 角速度单位为 deg/s * 100，不依赖 FPU。
 *
 * @note    若修改 QMI8658 陀螺仪量程，必须同步更新 AHRS_GYRO_LSB_PER_DPS。
 * @note    轴向映射统一由本文件宏配置，便于按 PCB 安装方向调整。
 *
 * @see     Code_boweny/Function/AHRS/AHRS.c
 */

#ifndef __AHRS_H__
#define __AHRS_H__

#include "config.h"

#define AHRS_IMU_PERIOD_MS               9U      /**< IMU 融合更新周期，单位 ms。 */
#define AHRS_MAG_PERIOD_MS               100U    /**< 地磁航向修正周期，单位 ms。 */
#define AHRS_DT_MAX_MS                   50U     /**< 单次积分最大 dt，超过后钳位以避免角度突跳。 */
#define AHRS_GYRO_LSB_PER_DPS            256L    /**< 陀螺仪灵敏度，单位 LSB/(deg/s)。 */
#define AHRS_GYRO_STILL_DPS100           800L    /**< 静止零偏学习阈值，单位 deg/s * 100。 */
#define AHRS_GYRO_DEADBAND_DPS100        12L     /**< 陀螺仪死区，单位 deg/s * 100。 */
#define AHRS_GYRO_LPF_SHIFT              2U      /**< 陀螺仪一阶低通强度，值越大响应越慢。 */
#define AHRS_ACC_REF_SAMPLE_COUNT        32U     /**< 启动阶段 1g 参考值采样帧数。 */
#define AHRS_ACC_NORM_TOLERANCE_PERCENT  35U     /**< 加速度模长有效窗口，单位百分比。 */
#define AHRS_ACC_ANGLE_LPF_SHIFT         3U      /**< 加速度解算角低通强度。 */
#define AHRS_ACC_BLEND_SHIFT             5U      /**< 加速度对 roll/pitch 的互补修正强度。 */
#define AHRS_GYRO_BIAS_SAMPLE_COUNT      128U    /**< 陀螺仪静止零偏学习帧数。 */
#define AHRS_MAG_ENABLE                  1       /**< 地磁航向修正开关，1=启用，0=禁用。 */
#define AHRS_MAG_LPF_SHIFT               3U      /**< 地磁数据和航向角低通强度。 */
#define AHRS_MAG_BLEND_SHIFT             5U      /**< 地磁对 yaw 的互补修正强度。 */
#define AHRS_MAG_MIN_NORM                50U     /**< 地磁模长最小有效值，低于该值认为无效。 */

#define AHRS_AXIS_RAW_X                  0U      /**< 原始传感器 X 轴索引。 */
#define AHRS_AXIS_RAW_Y                  1U      /**< 原始传感器 Y 轴索引。 */
#define AHRS_AXIS_RAW_Z                  2U      /**< 原始传感器 Z 轴索引。 */
#define AHRS_IMU_BODY_X_FROM             AHRS_AXIS_RAW_X  /**< 船体系 X 轴来源 IMU 原始轴。 */
#define AHRS_IMU_BODY_X_SIGN             1                /**< 船体系 X 轴符号，1=同向，-1=反向。 */
#define AHRS_IMU_BODY_Y_FROM             AHRS_AXIS_RAW_Y  /**< 船体系 Y 轴来源 IMU 原始轴。 */
#define AHRS_IMU_BODY_Y_SIGN             1                /**< 船体系 Y 轴符号，1=同向，-1=反向。 */
#define AHRS_IMU_BODY_Z_FROM             AHRS_AXIS_RAW_Z  /**< 船体系 Z 轴来源 IMU 原始轴。 */
#define AHRS_IMU_BODY_Z_SIGN             1                /**< 船体系 Z 轴符号，1=同向，-1=反向。 */
#define AHRS_MAG_BODY_X_FROM             AHRS_AXIS_RAW_X  /**< 船体系 X 轴来源地磁原始轴。 */
#define AHRS_MAG_BODY_X_SIGN             1                /**< 地磁船体系 X 轴符号，1=同向，-1=反向。 */
#define AHRS_MAG_BODY_Y_FROM             AHRS_AXIS_RAW_Y  /**< 船体系 Y 轴来源地磁原始轴。 */
#define AHRS_MAG_BODY_Y_SIGN             1                /**< 地磁船体系 Y 轴符号，1=同向，-1=反向。 */
#define AHRS_MAG_BODY_Z_FROM             AHRS_AXIS_RAW_Z  /**< 船体系 Z 轴来源地磁原始轴。 */
#define AHRS_MAG_BODY_Z_SIGN             1                /**< 地磁船体系 Z 轴符号，1=同向，-1=反向。 */

#define AHRS_FLAG_READY                  0x01U   /**< 姿态估计器已完成初始化并输出有效姿态。 */
#define AHRS_FLAG_ACC_VALID              0x02U   /**< 当前帧加速度模长在有效范围内。 */
#define AHRS_FLAG_MAG_VALID              0x04U   /**< 最近一次地磁数据有效。 */
#define AHRS_FLAG_GYRO_BIAS_READY        0x08U   /**< 陀螺仪静止零偏学习完成。 */
#define AHRS_FLAG_ACC_REF_READY          0x10U   /**< 加速度 1g 参考值已建立。 */
#define AHRS_FLAG_DT_CLAMPED             0x20U   /**< 当前帧 dt 被钳位过。 */

/**
 * @brief   AHRS 对外姿态状态快照。
 */
typedef struct
{
    int16 roll_deg100;     /**< 横滚角，单位 deg * 100。 */
    int16 pitch_deg100;    /**< 俯仰角，单位 deg * 100。 */
    int16 yaw_deg100;      /**< 航向角，单位 deg * 100，范围约 -18000~17999。 */

    int16 gyro_x_dps100;   /**< 船体系 X 轴角速度，单位 deg/s * 100。 */
    int16 gyro_y_dps100;   /**< 船体系 Y 轴角速度，单位 deg/s * 100。 */
    int16 gyro_z_dps100;   /**< 船体系 Z 轴角速度，单位 deg/s * 100。 */

    u16   acc_norm;        /**< 当前加速度模长近似值，原始 LSB 量纲。 */
    u16   dt_ms;           /**< 最近一次 IMU 融合使用的时间间隔，单位 ms。 */
    u16   update_count;    /**< 姿态融合更新计数，溢出后自然回绕。 */
    u8    flags;           /**< 状态标志位，取值见 AHRS_FLAG_*。 */
} AHRS_State_t;

/**
 * @brief   复位 AHRS 内部状态。
 * @return  none
 */
void AHRS_Reset(void);

/**
 * @brief      使用船体系六轴数据更新姿态。
 * @param[in]  ax     船体系 X 轴加速度原始值。
 * @param[in]  ay     船体系 Y 轴加速度原始值。
 * @param[in]  az     船体系 Z 轴加速度原始值。
 * @param[in]  gx     船体系 X 轴陀螺仪原始值。
 * @param[in]  gy     船体系 Y 轴陀螺仪原始值。
 * @param[in]  gz     船体系 Z 轴陀螺仪原始值。
 * @param[in]  dt_ms  本次更新与上次更新间隔，单位 ms。
 * @return     0=更新成功，-1=参数无效或尚未满足初始化条件。
 */
s8 AHRS_Update6Axis(int16 ax, int16 ay, int16 az,
                    int16 gx, int16 gy, int16 gz,
                    u16 dt_ms);

/**
 * @brief      使用 IMU 原始六轴数据更新姿态。
 * @param[in]  raw_ax  IMU 原始 X 轴加速度。
 * @param[in]  raw_ay  IMU 原始 Y 轴加速度。
 * @param[in]  raw_az  IMU 原始 Z 轴加速度。
 * @param[in]  raw_gx  IMU 原始 X 轴陀螺仪。
 * @param[in]  raw_gy  IMU 原始 Y 轴陀螺仪。
 * @param[in]  raw_gz  IMU 原始 Z 轴陀螺仪。
 * @param[in]  dt_ms   本次更新与上次更新间隔，单位 ms。
 * @return     0=更新成功，-1=参数无效或尚未满足初始化条件。
 */
s8 AHRS_UpdateRaw6Axis(int16 raw_ax, int16 raw_ay, int16 raw_az,
                       int16 raw_gx, int16 raw_gy, int16 raw_gz,
                       u16 dt_ms);

/**
 * @brief      使用船体系地磁数据修正航向。
 * @param[in]  mx  船体系 X 轴地磁原始值。
 * @param[in]  my  船体系 Y 轴地磁原始值。
 * @param[in]  mz  船体系 Z 轴地磁原始值。
 * @return     0=修正成功，-1=地磁数据无效或参数无效。
 */
s8 AHRS_UpdateMag(int16 mx, int16 my, int16 mz);

/**
 * @brief      使用地磁计原始数据修正航向。
 * @param[in]  raw_mx  地磁计原始 X 轴数据。
 * @param[in]  raw_my  地磁计原始 Y 轴数据。
 * @param[in]  raw_mz  地磁计原始 Z 轴数据。
 * @return     0=修正成功，-1=地磁数据无效或参数无效。
 */
s8 AHRS_UpdateRawMag(int16 raw_mx, int16 raw_my, int16 raw_mz);

/**
 * @brief   获取 AHRS 当前状态快照。
 * @return  指向内部只读 AHRS_State_t 状态的指针。
 */
const AHRS_State_t *AHRS_GetState(void);

/**
 * @brief   查询 AHRS 是否已经输出有效姿态。
 * @return  1=已就绪，0=尚未就绪。
 */
u8 AHRS_IsReady(void);

/**
 * @brief      将 IMU 原始三轴数据映射到船体系。
 * @param[in]  raw_x   原始 X 轴数据。
 * @param[in]  raw_y   原始 Y 轴数据。
 * @param[in]  raw_z   原始 Z 轴数据。
 * @param[out] body_x  船体系 X 轴输出指针。
 * @param[out] body_y  船体系 Y 轴输出指针。
 * @param[out] body_z  船体系 Z 轴输出指针。
 * @return     none
 */
void AHRS_MapRawToBody(int16 raw_x, int16 raw_y, int16 raw_z,
                       int16 *body_x, int16 *body_y, int16 *body_z);

/**
 * @brief      将地磁计原始三轴数据映射到船体系。
 * @param[in]  raw_x   地磁原始 X 轴数据。
 * @param[in]  raw_y   地磁原始 Y 轴数据。
 * @param[in]  raw_z   地磁原始 Z 轴数据。
 * @param[out] body_x  船体系 X 轴输出指针。
 * @param[out] body_y  船体系 Y 轴输出指针。
 * @param[out] body_z  船体系 Z 轴输出指针。
 * @return     none
 */
void AHRS_MapRawMagToBody(int16 raw_x, int16 raw_y, int16 raw_z,
                          int16 *body_x, int16 *body_y, int16 *body_z);

#endif
