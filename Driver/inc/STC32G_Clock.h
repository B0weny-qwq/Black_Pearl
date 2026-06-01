/**
 * @file    STC32G_Clock.h
 * @brief   STC32G 系统时钟与 PLL 配置接口
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本头文件定义主时钟、PLL、高速 IO 时钟的寄存器宏和配置函数声明。
 * 当前工程约定系统时钟为 `Fosc = 24MHz`，许多 UART、Timer、I2C 的波特率和分频计算
 * 都依赖这一时钟假设，因此任何时钟调整都必须联动复核底层外设配置。
 *
 * @note 当前职责边界：
 * - 本文件只定义时钟配置接口，不负责业务初始化顺序；
 * - `SYS_Init()` 负责在系统启动时按项目约束调用相关配置；
 * - 修改时钟后应同步检查 GPS、日志、PWM、I2C 等模块的实际时序。
 */

#ifndef	__STC32G_CLOCK_H
#define	__STC32G_CLOCK_H

#include	"config.h"

//========================================================================
//                              时钟设置
//========================================================================

#define		MainClockSel(n)	CLKSEL = (CLKSEL & ~0x0f) | (n)				/* 系统时钟选择 */
#define		PLLClockSel(n)	CLKSEL = (CLKSEL & ~0x80) | (n<<7)		/* PLL时钟选择 */
#define		HSIOClockSel(n)	CLKSEL = (CLKSEL & ~0x40) | (n<<6)		/* 高速IO时钟选择 */
#define		PLLClockIn(n)		USBCLK = (USBCLK & ~0x60) | (n<<4)		/* 系统时钟 n 分频作为PLL时钟源,确保分频后为12M */
#define		PLLEnable(n)		USBCLK = (USBCLK & ~0x80) | (n<<7)		/* PLL倍频使能 */
#define		HSClockDiv(n)		HSCLKDIV = (n)		/* 高速IO时钟分频系数 */

//========================================================================
//                              定义声明
//========================================================================

/* 系统时钟选择参数 */
#define MCLKSEL_HIRC       0x00
#define MCLKSEL_XIRC       0x01
#define MCLKSEL_X32K       0x02
#define MCLKSEL_I32K       0x03
#define MCLKSEL_PLL        0x04
#define MCLKSEL_PLL2       0x08
#define MCLKSEL_I48M       0x0c

/* PLL时钟选择参数 */
#define PLL_96M         0
#define PLL_144M        1

/* 高速IO时钟选择参数 */
#define HSCK_MCLK       0
#define HSCK_PLL        1

/* 系统时钟 n 分频作为PLL时钟源参数,确保分频后为12M */
#define ENCKM           0x80
#define PCKI_MSK        0x60
#define PCKI_D1         0x00
#define PCKI_D2         0x20
#define PCKI_D4         0x40
#define PCKI_D8         0x60

//========================================================================
//                              外部声明
//========================================================================

void HIRCClkConfig(u8 div);
void XOSCClkConfig(u8 div);
void IRC32KClkConfig(u8 div);
void HSPllClkConfig(u8 clksrc, u8 pllsel, u8 div);

#endif
