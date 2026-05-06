/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#ifndef		__CONFIG_H
#define		__CONFIG_H

//========================================================================
//                               主时钟定义
//========================================================================

//#define MAIN_Fosc		22118400L	//定义主时钟
//#define MAIN_Fosc		12000000L	//定义主时钟
//#define MAIN_Fosc		11059200L	//定义主时钟
//#define MAIN_Fosc		 5529600L	//定义主时钟
#define MAIN_Fosc		24000000L	//定义主时钟

//========================================================================
//                                头文件
//========================================================================

#include "type_def.h"
#include "stc32g.h"
#include <stdlib.h>
#include <stdio.h>

/*
 * AHRS bring-up mode:
 * 1 = keep only IMU + MAG fusion path running and only print AHRS logs.
 * 0 = normal firmware runtime with GPS, wireless, protocol and sensor tests.
 */
#define AHRS_TEST_ONLY  0

/* Legacy LT8920 pairing/runtime protocol switches */
#define SHIP_PROTOCOL_POLL_ENABLE      1
#define SHIP_PROTOCOL_COMPAT_ENABLE    0

/* Pairing channel and fixed seed (compile-time constants). */
#define PAIR_CHANNEL                   0x7F
#define SHIP_PAIR_SEED_USE_CHIPID      0
#define SHIP_PAIR_SEED0                0x65
#define SHIP_PAIR_SEED1                0x65
#define SHIP_PAIR_SEED2                0xA0
#define SHIP_PAIR_SEED3                0x65
/* 10ms tick based response window: 500 = 5s */
#define SHIP_PAIR_WAIT_RSP_TICKS       500

/*
 * 遥控器油门 PWM 输出总开关：
 * 0 = 当前测试模式，只打印 lr/ud/key，绝不驱动 Motor PWM。
 * 1 = 允许把 cmd=0x11 的油门数据输出到 Motor_SetBothSpeed()。
 */
#define SHIP_THROTTLE_PWM_ENABLE       0

/*
 * P0.0 / ADC_CH8 battery sampling print parameters.
 * SHIP_ADC_REF_MV is the ADC reference voltage in mV.
 * If the battery is divided before P0.0, set:
 *   battery_mv = adc_mv * SHIP_BAT_DIV_NUM / SHIP_BAT_DIV_DEN
 * Keep 1/1 when P0.0 is connected directly to the measured voltage.
 */
#define SHIP_ADC_REF_MV                3300UL
#define SHIP_BAT_DIV_NUM               1UL
#define SHIP_BAT_DIV_DEN               1UL
#define SHIP_ADC_LOG_ENABLE            1

/*
 * 无线最小业务测试模式：
 * 1 = 关闭 IMU/MAG/GPS 测试路径，只保留无线配对、接收和回包流程。
 * 0 = 启用正常运行路径。
 */
#define WIRELESS_MINIMAL_TEST_ONLY     1
#define WIRELESS_TX_ONLY_TEST          0
#define WIRELESS_PAIR_TX_ONLY_TEST     0
#define WIRELESS_FRONTEND_BYPASS_TEST  0
#define WIRELESS_CONTINUOUS_TX_TEST    0
#define WIRELESS_CARRIER_WAVE_TEST     0
#define WIRELESS_SOFT_SPI_TEST         0
#define WIRELESS_SOFT_SPI_DELAY_US     0
#define LT8920_FIFO_DELAY_TEST         0
#define LT8920_FORCE_TX_ORDER_TEST     0

//========================================================================
//                             外部函数和变量声明
//========================================================================


#endif
