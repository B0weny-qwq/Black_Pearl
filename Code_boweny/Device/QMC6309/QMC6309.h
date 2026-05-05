/**
 * @file    QMC6309.h
 * @brief   QMC6309 三轴地磁计驱动接口。
 * @author  boweny
 * @date    2026-05-05
 * @version v1.1
 *
 * @details
 * 提供 QMC6309 地磁计初始化、芯片 ID 读取、原始三轴磁场数据读取、
 * 软件低通滤波读取、输出数据速率配置和诊断寄存器打印接口。
 *
 * @hardware
 * - I2C: P1.4(SDA) / P1.5(SCL)
 * - 系统时钟: Fosc = 24MHz
 *
 * @note    调用前必须先初始化 I2C 总线。
 * @note    原始地磁数据仍需在应用层完成硬铁/软铁校准。
 *
 * @see     Code_boweny/Device/QMC6309/QMC6309.c
 */

#ifndef __QMC6309_H__
#define __QMC6309_H__

#include "config.h"

#define QMC6309_I2C_ADDR_PRIMARY   0x7C  /**< 当前优先尝试的 QMC6309 I2C 地址。 */
#define QMC6309_I2C_ADDR_ALT       0x0C  /**< QMC6309 备用 I2C 地址。 */
#define QMC6309_CHIP_ID_VALUE      0x90  /**< QMC6309 CHIP_ID 期望值。 */
#define QMC6309_I2C_SPEED_CFG      58    /**< QMC6309 I2C 速度配置值。 */

/**
 * @brief   初始化 QMC6309 地磁计。
 * @return  0=成功，-1=失败。
 *
 * @details
 * 完成地址探测、上电就绪轮询、CHIP_ID 校验、软复位、寄存器配置写入和回读验证。
 */
s8 QMC6309_Init(void);

/**
 * @brief      读取 QMC6309 原始三轴磁场数据。
 * @param[out] x  X 轴原始磁场数据输出指针。
 * @param[out] y  Y 轴原始磁场数据输出指针。
 * @param[out] z  Z 轴原始磁场数据输出指针。
 * @return     0=成功，-1=空指针、数据无效或读取失败。
 */
s8 QMC6309_ReadXYZ(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取低通滤波后的三轴磁场数据。
 * @param[out] x  X 轴滤波后磁场数据输出指针。
 * @param[out] y  Y 轴滤波后磁场数据输出指针。
 * @param[out] z  Z 轴滤波后磁场数据输出指针。
 * @return     0=成功，-1=原始读取失败、滤波失败或参数无效。
 */
s8 QMC6309_ReadXYZFiltered(int16 *x, int16 *y, int16 *z);

/**
 * @brief   读取 QMC6309 芯片 ID。
 * @return  CHIP_ID 值；0xFF 表示读取失败。
 */
u8 QMC6309_ReadID(void);

/**
 * @brief      配置 QMC6309 输出数据速率。
 * @param[in]  odr  CONTROL_2 寄存器配置值。
 * @return     0=成功，-1=写入失败。
 */
s8 QMC6309_SetODR(u8 odr);

/**
 * @brief      轮询等待 QMC6309 就绪。
 * @param[in]  timeout_ms  超时时间，单位 ms。
 * @return     0=就绪，-1=超时。
 */
s8 QMC6309_Wait_Ready(u16 timeout_ms);

/**
 * @brief      打印目标 I2C 地址下的关键寄存器。
 * @param[in]  target_addr  目标 7 位 I2C 地址。
 * @return     none
 *
 * @details
 * 读取并通过日志输出 CHIP_ID、CONTROL_1、CONTROL_2 等寄存器，便于排查地址和配置问题。
 */
void QMC6309_DumpRegs(u8 target_addr);

#endif
