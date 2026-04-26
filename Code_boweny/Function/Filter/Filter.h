/**
 * @file    Filter.h
 * @brief   三轴传感器低通滤波模块 — 陀螺仪与地磁数据平滑处理
 * @author  boweny
 * @date    2026-04-23
 * @version v1.0
 *
 * @details
 * - 提供面向三轴陀螺仪与三轴地磁数据的软件低通滤波接口
 * - 使用一阶 IIR 低通与 Q8 定点状态，不依赖浮点运算
 * - 导出接口：Filter_ResetGyroLowPass() / Filter_ResetMagLowPass() /
 *             Filter_GyroLowPass() / Filter_MagLowPass()
 *
 * @note    输入输出均为 int16 原始传感器数据
 * @note    空指针或无效帧直接返回 -1，且不会推进内部滤波状态
 *
 * @see     Code_boweny/Function/Filter/Filter.c
 */

#ifndef __FILTER_H__
#define __FILTER_H__

#include "config.h"

/*==============================================================
 *                      编译期配置宏
 *==============================================================*/
#define FILTER_LPF_STATE_Q       8    /**< 内部状态 Q 格式小数位数 */
#define FILTER_GYRO_LPF_SHIFT    2    /**< 陀螺仪低通平滑强度，值越大响应越慢 */
#define FILTER_MAG_LPF_SHIFT     2    /**< 地磁低通平滑强度，值越大响应越慢 */

/*==============================================================
 *                      函数声明
 *==============================================================*/

/**
 * @brief   复位陀螺仪低通滤波器状态
 * @return  none
 *
 * @details
 * 清空陀螺仪三轴低通的内部状态。
 * 下一帧有效数据会作为新的首帧直接灌入状态。
 */
void Filter_ResetGyroLowPass(void);

/**
 * @brief   复位地磁低通滤波器状态
 * @return  none
 *
 * @details
 * 清空地磁三轴低通的内部状态。
 * 下一帧有效数据会作为新的首帧直接灌入状态。
 */
void Filter_ResetMagLowPass(void);

/**
 * @brief      对陀螺仪三轴数据执行低通滤波
 * @param[in]  in_x   原始 X 轴角速度
 * @param[in]  in_y   原始 Y 轴角速度
 * @param[in]  in_z   原始 Z 轴角速度
 * @param[out] out_x  指向滤波后 X 轴角速度的指针
 * @param[out] out_y  指向滤波后 Y 轴角速度的指针
 * @param[out] out_z  指向滤波后 Z 轴角速度的指针
 * @return     0=成功，-1=空指针或输入帧无效
 *
 * @details
 * 使用一阶 IIR 低通滤波器：
 *   state += (((input << Q) - state) >> shift)
 * 首帧有效数据直接写入状态，保证首次输出等于输入值。
 */
s8 Filter_GyroLowPass(int16 in_x, int16 in_y, int16 in_z,
                      int16 *out_x, int16 *out_y, int16 *out_z);

/**
 * @brief      对地磁三轴数据执行低通滤波
 * @param[in]  in_x   原始 X 轴磁场数据
 * @param[in]  in_y   原始 Y 轴磁场数据
 * @param[in]  in_z   原始 Z 轴磁场数据
 * @param[out] out_x  指向滤波后 X 轴磁场数据的指针
 * @param[out] out_y  指向滤波后 Y 轴磁场数据的指针
 * @param[out] out_z  指向滤波后 Z 轴磁场数据的指针
 * @return     0=成功，-1=空指针或输入帧无效
 *
 * @details
 * 与陀螺仪低通相同，使用独立的三轴状态和编译期平滑系数。
 * 首帧有效数据直接写入状态，避免启动瞬态拉偏输出。
 */
s8 Filter_MagLowPass(int16 in_x, int16 in_y, int16 in_z,
                     int16 *out_x, int16 *out_y, int16 *out_z);

#endif
