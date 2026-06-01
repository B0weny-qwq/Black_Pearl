/**
 * @file    app.h
 * @brief   STC 官方 App 示例聚合头文件
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本层保留官方 `App/` 示例模块的头文件入口，便于选择性启用板级演示功能。
 * 在当前 Black Pearl 工程中，`APP_config()` 仅保留与板载灯/显示相关的最小初始化，
 * 其余 UART、DMA、EEPROM、LIN、SPI 等示例默认不参与主业务链路。
 *
 * @note 当前职责边界：
 * - `App/` 层主要提供官方外设示例和板级演示初始化；
 * - `User/`、`Code_boweny/` 才是当前船控、无线、AHRS、GPS 的真实业务入口；
 * - 若某个示例会与当前业务抢占引脚、Timer 或串口资源，应保持关闭。
 */

#ifndef __APP_H_
#define __APP_H_

//========================================================================
//                                头文件
//========================================================================

#include	"config.h"
#include	"APP_Lamp.h"
#include	"APP_AD_UART.h"
#include	"APP_INT_UART.h"
#include	"APP_RTC.h"
#include	"APP_I2C_PS.h"
#include	"APP_SPI_PS.h"
#include	"APP_WDT.h"
#include	"APP_PWM.h"
#include	"APP_EEPROM.h"
#include	"APP_DMA_AD.h"
#include	"APP_DMA_M2M.h"
#include	"APP_DMA_UART.h"
#include	"APP_DMA_SPI_PS.h"
#include	"APP_DMA_LCM.h"
#include	"APP_DMA_I2C.h"
#include	"APP_CAN.h"
#include	"APP_LIN.h"
#include	"APP_USART_LIN.h"
#include	"APP_USART2_LIN.h"
#include	"APP_HSSPI.h"
#include	"APP_HSPWM.h"

//========================================================================
//                               本地常量声明	
//========================================================================

#define DIS_DOT     0x20
#define DIS_BLACK   0x10
#define DIS_        0x11

extern u8 code t_display[];

extern u8 code T_COM[];      //位码

extern u8 code T_KeyTable[16];

//========================================================================
//                            外部函数和变量声明
//========================================================================

extern u8  LED8[8];        //显示缓冲
extern u8  display_index;  //显示位索引

extern u8  IO_KeyState, IO_KeyState1, IO_KeyHoldCnt;    //行列键盘变量
extern u8  KeyHoldCnt; //键按下计时
extern u8  KeyCode;    //给用户使用的键码
extern u8  cnt50ms;

extern u8  hour,minute,second; //RTC变量
extern u16 msecond;

void APP_config(void);
void DisplayScan(void);

#endif
