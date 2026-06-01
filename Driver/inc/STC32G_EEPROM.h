/**
 * @file    STC32G_EEPROM.h
 * @brief   STC32G 片上 EEPROM / IAP 操作接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本头文件定义片上 EEPROM 的 IAP 读写、擦除相关宏和函数声明。
 * 当前工程中校准数据持久化会依赖底层 EEPROM 能力，但数据布局、双槽策略和版本兼容
 * 由上层业务模块负责。
 *
 * @note 当前职责边界：
 * - 只提供原始读写/擦除能力；
 * - 不维护项目级参数结构体与恢复策略。
 */

#ifndef	__STC32G_EEPROM_H
#define	__STC32G_EEPROM_H

#include	"config.h"

//========================================================================
//                              定义声明
//========================================================================


//========================================================================
//                               IAP设置
//========================================================================

#define		IAP_STANDBY()	IAP_CMD = 0		//IAP空闲命令（禁止）
#define		IAP_READ()		IAP_CMD = 1		//IAP读出命令
#define		IAP_WRITE()		IAP_CMD = 2		//IAP写入命令
#define		IAP_ERASE()		IAP_CMD = 3		//IAP擦除命令

#define	IAP_ENABLE()		IAPEN = 1; IAP_TPS = MAIN_Fosc / 1000000
#define	IAP_DISABLE()		IAP_CONTR = 0; IAP_CMD = 0; IAP_TRIG = 0; IAP_ADDRH = 0xff; IAP_ADDRL = 0xff


void	DisableEEPROM(void);
void 	EEPROM_read_n(u32 EE_address,u8 *DataAddress,u16 number);
void 	EEPROM_write_n(u32 EE_address,u8 *DataAddress,u16 number);
void	EEPROM_SectorErase(u32 EE_address);


#endif
