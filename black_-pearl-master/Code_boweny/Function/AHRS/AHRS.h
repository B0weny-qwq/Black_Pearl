/**
 * @file    AHRS.h
 * @brief   AHRS 姿态融合模块 — QMI8658 + QMC6309 定点互补滤波
 * @author  boweny
 * @date    2026-04-27
 * @version v1.0
 *
 * @details
 * - 面向 Black Pearl 船体控制工程，提供轻量级姿态估计接口
 * - 参考 PX4 底层姿态估计思路：陀螺仪负责短期角速度积分，加速度计负责横滚/俯仰长期修正，
 *   地磁计负责航向低频慢修正
 * - 全部使用整数定点运算，不依赖 FPU，不使用 `%f`
 * - 输出姿态角单位为 `deg * 100`，角速度单位为 `deg/s * 100`
 * - 机体系定义：`+X=船尾`，`+Y=船右/右舷`，`+Z=上`
 *
 * @note
 * - 当前默认 IMU 更新周期为 17ms，对应旧版 QMI8658 bring-up 复测配置 `CTRL2/CTRL3=0x07/0x07`
 * - 若后续修改 QMI8658 陀螺仪量程，必须同步修改 `AHRS_GYRO_LSB_PER_DPS`
 * - 轴向映射统一在本文件宏中配置，便于根据芯片丝印和实测结果快速调整
 *
 * @see     Code_boweny/Function/AHRS/AHRS.c
 */

#ifndef __AHRS_H__
#define __AHRS_H__

#include "config.h"

/*==============================================================
 *                        调度周期配置
 *==============================================================*/

#define AHRS_IMU_PERIOD_MS               17U     /**< IMU 融合更新周期，单位 ms */
#define AHRS_MAG_PERIOD_MS               100U    /**< 地磁航向修正周期，单位 ms */
#define AHRS_DT_MAX_MS                   50U     /**< 单次积分最大 dt，超过则钳位，避免堵塞后角度突跳 */

/*==============================================================
 *                        融合参数配置
 *==============================================================*/

/**
 * @brief   陀螺仪灵敏度，单位 LSB/(deg/s)
 *
 * @details
 * 当前 `QMI8658_CTRL3_INIT=0x07`，使用旧版实测可出数的 bring-up 配置，
 * 先按 ±16dps 档位的 2048 LSB/(deg/s) 处理。
 */
#define AHRS_GYRO_LSB_PER_DPS            2048L

#define AHRS_GYRO_STILL_DPS100           800L    /**< 静止零偏学习阈值，单位 deg/s * 100 */
#define AHRS_GYRO_DEADBAND_DPS100        12L     /**< 陀螺仪死区，单位 deg/s * 100 */
#define AHRS_GYRO_LPF_SHIFT              2U      /**< 陀螺仪一阶低通强度，值越大响应越慢 */

#define AHRS_ACC_REF_SAMPLE_COUNT        32U     /**< 启动阶段 1g 参考值采样帧数 */
#define AHRS_ACC_NORM_TOLERANCE_PERCENT  35U     /**< 加速度模长有效窗口，百分比 */
#define AHRS_ACC_ANGLE_LPF_SHIFT         3U      /**< 加速度解算角低通强度 */
#define AHRS_ACC_BLEND_SHIFT             5U      /**< 加速度对 roll/pitch 的互补修正强度 */

#define AHRS_GYRO_BIAS_SAMPLE_COUNT      128U    /**< 陀螺仪静止零偏学习帧数 */

/* Keep yaw gyro-relative by default until QMC6309 calibration is complete. */
#ifndef AHRS_MAG_ENABLE
#define AHRS_MAG_ENABLE                  0       /**< 是否启用地磁航向慢修正：1=启用，0=仅保留陀螺积分航向 */
#endif
#define AHRS_MAG_LPF_SHIFT               3U      /**< 地磁数据和航向角低通强度 */
#define AHRS_MAG_BLEND_SHIFT             5U      /**< 地磁对 yaw 的互补修正强度 */
#define AHRS_MAG_MIN_NORM                50U     /**< 地磁模长最小有效值，低于该值认为无效 */

/*==============================================================
 *                        轴向映射配置
 *==============================================================*/

#define AHRS_AXIS_RAW_X                  0U      /**< 原始传感器 X 轴 */
#define AHRS_AXIS_RAW_Y                  1U      /**< 原始传感器 Y 轴 */
#define AHRS_AXIS_RAW_Z                  2U      /**< 原始传感器 Z 轴 */

/**
 * @brief   IMU 原始轴到船体轴的映射
 *
 * @details
 * - 默认使用原始轴直通，即 raw X/Y/Z -> body X/Y/Z
 * - 若实测发现芯片 raw +X 指向船头，而工程定义 body +X 为船尾，
 *   则将 `AHRS_IMU_BODY_X_SIGN` 改为 `-1`
 * - 若芯片安装旋转 90 度，可通过 `*_FROM` 交换轴，通过 `*_SIGN` 调整正负号
 */
#define AHRS_IMU_BODY_X_FROM             AHRS_AXIS_RAW_X
#define AHRS_IMU_BODY_X_SIGN             1
#define AHRS_IMU_BODY_Y_FROM             AHRS_AXIS_RAW_Y
#define AHRS_IMU_BODY_Y_SIGN             1
#define AHRS_IMU_BODY_Z_FROM             AHRS_AXIS_RAW_Z
#define AHRS_IMU_BODY_Z_SIGN             1

/**
 * @brief   地磁计原始轴到船体轴的映射
 *
 * @details
 * QMC6309 与 QMI8658 可能不是同一安装方向，地磁轴向单独配置，避免误用 IMU 映射。
 */
#define AHRS_MAG_BODY_X_FROM             AHRS_AXIS_RAW_X
#define AHRS_MAG_BODY_X_SIGN             1
#define AHRS_MAG_BODY_Y_FROM             AHRS_AXIS_RAW_Y
#define AHRS_MAG_BODY_Y_SIGN             1
#define AHRS_MAG_BODY_Z_FROM             AHRS_AXIS_RAW_Z
#define AHRS_MAG_BODY_Z_SIGN             1

/*==============================================================
 *                        状态标志位
 *==============================================================*/

#define AHRS_FLAG_READY                  0x01U   /**< 姿态估计器已完成初始化并输出有效姿态 */
#define AHRS_FLAG_ACC_VALID              0x02U   /**< 当前帧加速度模长在有效范围内 */
#define AHRS_FLAG_MAG_VALID              0x04U   /**< 最近一次地磁数据有效 */
#define AHRS_FLAG_GYRO_BIAS_READY        0x08U   /**< 陀螺仪静止零偏学习完成 */
#define AHRS_FLAG_ACC_REF_READY          0x10U   /**< 加速度 1g 参考值已建立 */
#define AHRS_FLAG_DT_CLAMPED             0x20U   /**< 当前帧 dt 曾被钳位 */

/**
 * @brief   AHRS 对外姿态状态快照
 *
 * @details
 * 调用者通过 `AHRS_GetState()` 获取只读指针。所有角度输出均为 `deg * 100`，
 * 例如 `1234` 表示 `12.34 deg`。
 */
typedef struct
{
    int16 roll_deg100;     /**< 横滚角，单位 deg * 100 */
    int16 pitch_deg100;    /**< 俯仰角，单位 deg * 100 */
    int16 yaw_deg100;      /**< 航向角，单位 deg * 100，范围约为 -18000~17999 */

    int16 gyro_x_dps100;   /**< 船体系 X 轴角速度，单位 deg/s * 100 */
    int16 gyro_y_dps100;   /**< 船体系 Y 轴角速度，单位 deg/s * 100 */
    int16 gyro_z_dps100;   /**< 船体系 Z 轴角速度，单位 deg/s * 100 */

    u16   acc_norm;        /**< 当前加速度模长近似值，原始 LSB 量纲 */
    u16   dt_ms;           /**< 最近一次 IMU 融合使用的时间间隔，单位 ms */
    u16   update_count;    /**< 姿态融合更新计数，溢出后自然回绕 */
    u8    flags;           /**< 状态标志位，见 `AHRS_FLAG_*` */
} AHRS_State_t;

/**
 * @brief   复位 AHRS 内部状态
 * @return  none
 *
 * @details
 * 清空姿态角、低通滤波器、加速度 1g 参考值、陀螺仪零偏学习状态和输出快照。
 * 建议在传感器初始化成功前后调用一次。
 */
void AHRS_Reset(void);

/**
 * @brief      使用船体系 6 轴数据更新姿态
 * @param[in]  ax     船体系 X 轴加速度原始值
 * @param[in]  ay     船体系 Y 轴加速度原始值
 * @param[in]  az     船体系 Z 轴加速度原始值
 * @param[in]  gx     船体系 X 轴陀螺仪原始值
 * @param[in]  gy     船体系 Y 轴陀螺仪原始值
 * @param[in]  gz     船体系 Z 轴陀螺仪原始值
 * @param[in]  dt_ms  本次更新与上次更新间隔，单位 ms
 * @return     0=姿态更新成功，-1=尚未完成初始化或参数无效
 */
s8 AHRS_Update6Axis(int16 ax, int16 ay, int16 az,
                    int16 gx, int16 gy, int16 gz,
                    u16 dt_ms);

/**
 * @brief      使用传感器原始 6 轴数据更新姿态
 * @param[in]  raw_ax   IMU 原始 X 轴加速度
 * @param[in]  raw_ay   IMU 原始 Y 轴加速度
 * @param[in]  raw_az   IMU 原始 Z 轴加速度
 * @param[in]  raw_gx   IMU 原始 X 轴陀螺仪
 * @param[in]  raw_gy   IMU 原始 Y 轴陀螺仪
 * @param[in]  raw_gz   IMU 原始 Z 轴陀螺仪
 * @param[in]  dt_ms    本次更新与上次更新间隔，单位 ms
 * @return     0=姿态更新成功，-1=尚未完成初始化或参数无效
 *
 * @details
 * 函数内部会先按 `AHRS_IMU_BODY_*` 宏完成原始轴到船体轴的映射。
 */
s8 AHRS_UpdateRaw6Axis(int16 raw_ax, int16 raw_ay, int16 raw_az,
                       int16 raw_gx, int16 raw_gy, int16 raw_gz,
                       u16 dt_ms);

/**
 * @brief      使用船体系地磁数据修正航向
 * @param[in]  mx  船体系 X 轴地磁原始值
 * @param[in]  my  船体系 Y 轴地磁原始值
 * @param[in]  mz  船体系 Z 轴地磁原始值
 * @return     0=地磁修正成功，-1=地磁数据无效
 */
s8 AHRS_UpdateMag(int16 mx, int16 my, int16 mz);

/**
 * @brief      使用地磁计原始数据修正航向
 * @param[in]  raw_mx  地磁计原始 X 轴数据
 * @param[in]  raw_my  地磁计原始 Y 轴数据
 * @param[in]  raw_mz  地磁计原始 Z 轴数据
 * @return     0=地磁修正成功，-1=地磁数据无效
 *
 * @details
 * 函数内部会先按 `AHRS_MAG_BODY_*` 宏完成原始轴到船体轴的映射。
 */
s8 AHRS_UpdateRawMag(int16 raw_mx, int16 raw_my, int16 raw_mz);

/**
 * @brief   获取 AHRS 当前状态快照
 * @return  指向内部只读 `AHRS_State_t` 状态的指针
 *
 * @note    调用者不得修改返回指针指向的数据。
 */
const AHRS_State_t *AHRS_GetState(void);

/**
 * @brief   查询 AHRS 是否已输出有效姿态
 * @return  1=已就绪，0=尚未完成初始化
 */
u8 AHRS_IsReady(void);

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
                       int16 *body_x, int16 *body_y, int16 *body_z);

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
                          int16 *body_x, int16 *body_y, int16 *body_z);

#endif
