/**
 * @file    QMC6309.h
 * @brief   QMC6309 3轴地磁计驱动 — 常量定义与API声明
 * @author  boweny
 * @date    2026-04-22
 * @version v1.1
 *
 * @details
 * - 实现 QMC6309 3轴地磁计的初始化、原始数据读取与 ODR 配置
 * - 保留原始 XYZ 读取接口，并新增软件低通后的滤波读取接口
 * - 导出函数：QMC6309_Init() / QMC6309_ReadXYZ() / QMC6309_ReadXYZFiltered() /
 *             QMC6309_ReadID() / QMC6309_SetODR() / QMC6309_Wait_Ready() /
 *             QMC6309_DumpRegs()
 *
 * @hardware
 *   - I2C接口: P1.4(SDA) / P1.5(SCL)，硬件I2C
 *   - 系统时钟: Fosc = 24MHz
 *
 * @note    调用前必须先初始化 I2C 总线
 * @note    原始地磁数据需在应用层完成硬铁/软铁校准
 *
 * @see     Code_boweny/Device/QMC6309/QMC6309.c
 */

#ifndef __QMC6309_H__
#define __QMC6309_H__

/*==============================================================
 *                           头文件
 *==============================================================*/
#include "config.h"

#define QMC6309_I2C_ADDR_PRIMARY   0x7C
#define QMC6309_I2C_ADDR_ALT       0x0C
#define QMC6309_CHIP_ID_VALUE      0x90
#define QMC6309_I2C_SPEED_CFG      58

/*==============================================================
 *                          外部函数声明
 *==============================================================*/

/**
 * @brief   初始化 QMC6309 地磁计
 * @return  0=成功，-1=失败
 *
 * @details
 * 完成地址探测、上电就绪轮询、CHIP_ID 校验、软复位、
 * 寄存器配置写入与读回验证。
 */
s8 QMC6309_Init(void);

/**
 * @brief      读取地磁计原始 XYZ 数据
 * @param[out] x  指向 X 轴原始磁场数据的指针
 * @param[out] y  指向 Y 轴原始磁场数据的指针
 * @param[out] z  指向 Z 轴原始磁场数据的指针
 * @return      0=成功，-1=数据无效或读取失败
 *
 * @details 连续读取 6 字节地磁数据，小端序组装为 int16。
 */
s8 QMC6309_ReadXYZ(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取低通滤波后的地磁 XYZ 数据
 * @param[out] x  指向 X 轴滤波后磁场数据的指针
 * @param[out] y  指向 Y 轴滤波后磁场数据的指针
 * @param[out] z  指向 Z 轴滤波后磁场数据的指针
 * @return      0=成功，-1=原始数据无效、滤波失败或读取失败
 *
 * @details
 * 先调用 QMC6309_ReadXYZ() 获取原始三轴磁场数据，
 * 再通过 Function/Filter 模块执行软件低通滤波。
 */
s8 QMC6309_ReadXYZFiltered(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取 QMC6309 芯片ID
 * @return     CHIP_ID 值，0xFF 表示读取失败
 */
u8 QMC6309_ReadID(void);

/**
 * @brief      配置 QMC6309 输出数据速率
 * @param[in]  odr  CONTROL_2 寄存器配置值
 * @return     0=成功，-1=失败
 *
 * @details 写入 CONTROL_2 寄存器，具体值由外部按芯片手册填写。
 */
s8 QMC6309_SetODR(u8 odr);

/**
 * @brief      轮询等待地磁计就绪
 * @param[in]  timeout_ms  超时时间 (ms)
 * @return     0=就绪，-1=超时
 *
 * @details 周期性读取 CHIP_ID，直到返回期望值 0x90。
 */
s8 QMC6309_Wait_Ready(u16 timeout_ms);

/**
 * @brief      打印目标地址下的关键寄存器值
 * @param[in]  target_addr  目标 7 位 I2C 地址
 * @return     none
 *
 * @details 读取并输出 CHIP_ID / CONTROL_1 / CONTROL_2，便于排查地址与配置问题。
 */
void QMC6309_DumpRegs(u8 target_addr);

#endif
