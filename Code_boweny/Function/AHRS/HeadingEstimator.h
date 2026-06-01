#ifndef __HEADING_ESTIMATOR_H__
#define __HEADING_ESTIMATOR_H__

/**
 * @file HeadingEstimator.h
 * @brief 航向估计器接口。
 *
 * 该模块使用 Z 轴陀螺积分作为主要航向来源，并在船体静止且磁力计
 * 样本稳定时，以很小权重对航向进行磁力计修正。船体运动期间默认
 * 不融合磁力计，只记录磁力计观测值，避免水面运动和磁干扰导致
 * 航向突跳。
 *
 * 当前边界：
 * - 本模块输出的是原始融合航向；
 * - `NorthCalib` 的 `north_offset_cd` 不在此处保存或叠加；
 * - 导航统一航向由 `MainLoop_GetHeadingDeg100()` 在外层完成修正。
 *
 * @note 角度单位默认使用度，`Deg100` 系列接口返回 0.01 度单位。
 * @note 当前接口使用浮点数，调用频率和计算开销由上层任务调度控制。
 */

#include "config.h"

/**
 * @note 当前职责边界：
 * - `HeadingEstimator` 只输出原始融合航向与相关调试状态；
 * - `NorthCalib` 的导航北向偏移不在本模块内保存或叠加；
 * - 系统统一导航航向由 `MainLoop_GetHeadingDeg100()` 在上层组合生成。
 */

/**
 * @def HEADING_USE_INTERNAL_BIAS
 * @brief 是否启用模块内部陀螺 Z 轴零偏补偿。
 *
 * 为 0 时，模块不使用内部零偏，`gyro_z_bias_dps` 保持为 0。
 * 为 1 时，更新过程会从输入角速度中减去 `gyro_z_bias_dps`。
 *
 * @note 当前实现没有在线估计零偏的闭环逻辑，启用后仍需要外部维护
 *       `gyro_z_bias_dps` 或后续补齐零偏估计逻辑。
 */
#ifndef HEADING_USE_INTERNAL_BIAS
#define HEADING_USE_INTERNAL_BIAS      0
#endif

/**
 * @def HEADING_DBG_FORCE_MAG_OFF
 * @brief 调试开关：强制关闭磁力计融合。
 *
 * 为 1 时，即使磁力计样本有效且船体静止，也不使用磁力计修正航向。
 */
#ifndef HEADING_DBG_FORCE_MAG_OFF
#define HEADING_DBG_FORCE_MAG_OFF      0
#endif

/** @brief 静止状态下 Z 轴陀螺角速度死区，单位：deg/s。 */
#define HEADING_GYRO_DEADZONE_DPS      0.05f

/** @brief 内部陀螺零偏限幅，单位：deg/s。 */
#define HEADING_BIAS_LIMIT_DPS         10.0f

/** @brief 磁力计航向与预测航向允许的最大误差门限，单位：deg。 */
#define HEADING_MAG_ERR_GATE_DEG       25.0f

/** @brief 相邻磁力计航向样本允许的最大跳变门限，单位：deg。 */
#define HEADING_MAG_JUMP_GATE_DEG      10.0f

/** @brief 静止参考磁航向允许的稳定范围，单位：deg。 */
#define HEADING_STATIC_MAG_STABLE_DEG  1.00f

/** @brief 预留静止磁航向误差门限，单位：deg。 */
#define HEADING_STATIC_MAG_ERR_GATE_DEG 3.00f

/** @brief 预留磁力计融合所需最小 Z 轴角速度，单位：deg/s。 */
#define HEADING_MAG_FUSE_MIN_GZ_DPS    0.30f

/** @brief 静止状态下磁力计修正增益。 */
#define HEADING_KMAG_STATIC            0.0008f

/** @brief 运动状态下磁力计修正增益；当前设置为 0，表示运动时不融合磁力计。 */
#define HEADING_KMAG_MOVE              0.0f

/**
 * @brief 航向估计器状态。
 *
 * 该结构体保存航向估计、磁力计观测、陀螺积分和调试状态。
 * 上层应将该结构体作为模块实例传入所有 `Heading_*` 接口。
 */
typedef struct
{
    /** 当前融合后的绝对航向，范围约束为 [0, 360)，单位：deg。 */
    float heading_deg;             /**< 当前融合后的绝对航向，范围 [0, 360)，单位 deg。 */

    /** 相对航向零点，调用 Heading_ResetZero() 时记录，单位：deg。 */
    float heading_zero_deg;        /**< 相对航向零点，调用 Heading_ResetZero() 时记录，单位 deg。 */

    /** Z 轴陀螺零偏，单位：deg/s。 */
    float gyro_z_bias_dps;         /**< Z 轴陀螺零偏，单位 deg/s。 */

    /** 仅由陀螺积分得到的航向，单位：deg。 */
    float yaw_gyro_deg;            /**< 仅由陀螺积分得到的航向，单位 deg。 */

    /** 最近一次归一化后的磁力计航向观测，单位：deg。 */
    float yaw_mag_deg;             /**< 最近一次归一化后的磁力计航向观测，单位 deg。 */

    /** 本次更新中由陀螺预测得到的航向，单位：deg。 */
    float heading_pred_deg;        /**< 本次更新中由陀螺预测得到的航向，单位 deg。 */

    /** 磁力计观测与预测航向之间的误差，范围约束为 [-180, 180)，单位：deg。 */
    float heading_err_deg;         /**< 磁力计观测与预测航向之间的误差，范围 [-180, 180)，单位 deg。 */

    /** 静止状态下建立的磁力计参考航向，单位：deg。 */
    float static_mag_ref_deg;      /**< 静止状态下建立的磁力计参考航向，单位 deg。 */

    /** 磁力计融合置信度；当前实现中使用 0 或 1 表示本次是否融合。 */
    float mag_confidence;          /**< 磁力计融合置信度，当前使用 0/1 表示本次是否融合。 */

    /** 上一次被接受的磁力计航向样本，单位：deg。 */
    float last_yaw_mag_deg;        /**< 上一次被接受的磁力计航向样本，单位 deg。 */

    /** 原始磁力计输入是否有效。 */
    u8 mag_valid;                  /**< 原始磁力计输入是否有效。 */

    /** 本次更新是否实际使用磁力计修正航向。 */
    u8 mag_used;                   /**< 本次更新是否实际使用磁力计修正航向。 */

    /** 上层传入的静止标志。 */
    u8 static_flag;                /**< 上层传入的静止标志。 */

    /** 静止磁力计参考航向是否已经建立。 */
    u8 static_mag_ref_valid;       /**< 静止磁力计参考航向是否已经建立。 */

    /** 本次更新传入的原始磁力计有效标志。 */
    u8 raw_mag_valid;              /**< 本次更新传入的原始磁力计有效标志。 */

    /** 上一次磁力计样本是否通过跳变门限检查。 */
    u8 last_mag_sample_valid;      /**< 上一次磁力计样本是否通过跳变门限检查。 */
} HeadingEstimator_t;

/**
 * @brief 初始化航向估计器。
 *
 * 会清零所有航向、零偏、磁力计状态和标志位。
 *
 * @param h 航向估计器实例指针，允许为空；为空时函数直接返回。
 */
void Heading_Init(HeadingEstimator_t *h);

/**
 * @brief 将当前绝对航向设置为相对航向零点。
 *
 * 后续 Heading_GetRelativeDeg100() 会以该零点输出相对角度。
 *
 * @param h 航向估计器实例指针，允许为空；为空时函数直接返回。
 */
void Heading_ResetZero(HeadingEstimator_t *h);

/**
 * @brief 强制设置当前航向。
 *
 * 常用于磁力计航向稳定后对估计器进行初始定向。该函数会同步更新
 * 陀螺航向、磁力计航向、预测航向和静止参考航向，并清除磁力计采样状态。
 *
 * @param h 航向估计器实例指针，允许为空；为空时函数直接返回。
 * @param heading_deg 目标航向，单位：deg，函数内部会归一化到 [0, 360)。
 */
void Heading_SetHeadingDeg(HeadingEstimator_t *h, float heading_deg);

/**
 * @brief 更新航向估计器。
 *
 * 更新流程：
 * 1. 使用 Z 轴陀螺角速度积分预测航向。
 * 2. 对磁力计航向做归一化和跳变门限检查。
 * 3. 仅在静止状态、磁力计稳定且误差未超限时，小权重融合磁力计。
 * 4. 运动状态下默认不融合磁力计，以陀螺积分维持航向连续性。
 *
 * @param h 航向估计器实例指针，允许为空；为空时函数直接返回。
 * @param gyro_z_dps Z 轴陀螺角速度，单位：deg/s。
 * @param yaw_mag_deg 磁力计计算得到的绝对航向，单位：deg。
 * @param mag_valid 磁力计航向是否有效，非 0 表示有效。
 * @param static_flag 船体是否处于静止状态，非 0 表示静止。
 * @param dt 本次更新周期，单位：s；小于等于 0 时函数直接返回。
 */
void Heading_Update(HeadingEstimator_t *h,
                    float gyro_z_dps,
                    float yaw_mag_deg,
                    u8 mag_valid,
                    u8 static_flag,
                    float dt);

/**
 * @brief 获取当前绝对航向。
 *
 * @param h 航向估计器实例指针，允许为空。
 * @return 当前绝对航向，单位：deg，范围 [0, 360)；实例为空时返回 0。
 */
float Heading_GetDeg(const HeadingEstimator_t *h);

/**
 * @brief 获取当前绝对航向，0.01 度单位。
 *
 * @param h 航向估计器实例指针，允许为空。
 * @return 当前绝对航向乘以 100，范围约为 [0, 36000)；实例为空时返回 0。
 */
int32 Heading_GetDeg100(const HeadingEstimator_t *h);

/**
 * @brief 获取相对零点的航向误差，0.01 度单位。
 *
 * 相对角度范围约束为 [-18000, 18000)，适合控制器计算最短转向误差。
 *
 * @param h 航向估计器实例指针，允许为空。
 * @return 当前航向相对零点的角度差，单位：0.01 deg；实例为空时返回 0。
 */
int32 Heading_GetRelativeDeg100(const HeadingEstimator_t *h);

/**
 * @brief 获取陀螺积分航向，0.01 度单位。
 *
 * @param h 航向估计器实例指针，允许为空。
 * @return 陀螺积分航向乘以 100，范围约为 [0, 36000)；实例为空时返回 0。
 */
int32 Heading_GetGyroDeg100(const HeadingEstimator_t *h);

/**
 * @brief 获取最近一次磁力计航向观测，0.01 度单位。
 *
 * @param h 航向估计器实例指针，允许为空。
 * @return 磁力计航向乘以 100，范围约为 [0, 36000)；实例为空时返回 0。
 */
int32 Heading_GetMagDeg100(const HeadingEstimator_t *h);

/**
 * @brief 获取当前陀螺 Z 轴零偏，0.01 deg/s 单位。
 *
 * 当 HEADING_USE_INTERNAL_BIAS 为 0 时固定返回 0。
 *
 * @param h 航向估计器实例指针，允许为空。
 * @return 陀螺 Z 轴零偏乘以 100，单位：0.01 deg/s；实例为空时返回 0。
 */
int32 Heading_GetBiasDps100(const HeadingEstimator_t *h);

#endif
