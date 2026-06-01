/**
 * @file    STC32G_Soft_I2C.h
 * @brief   STC32G 软件 I2C 接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本头文件提供软件模拟 I2C 的基础接口。当前项目传感器优先使用硬件 I2C/专用端口层，
 * 软件 I2C 作为兼容与实验路径保留。
 */

#ifndef	__STC32G_SOFT_I2C_H
#define	__STC32G_SOFT_I2C_H

#include	"config.h"

#define SLAW    0x5A
#define SLAR    0x5B

void SI2C_WriteNbyte(u8 dev_addr, u8 mem_addr, u8 *p, u8 number);
void SI2C_ReadNbyte(u8 dev_addr, u8 mem_addr, u8 *p, u8 number);

#endif

