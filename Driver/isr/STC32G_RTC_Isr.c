/**
 * @file    STC32G_RTC_Isr.c
 * @brief   STC32G RTC 中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件承载 RTC 相关中断入口，当前项目默认不把 RTC 中断作为主业务链路的一部分。
 */

#include	"STC32G_RTC.h"

bit B_1S;
bit B_Alarm;

//========================================================================
// 函数: RTC_ISR_Handler
// 描述: RTC中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2022-03-21
//========================================================================
void RTC_ISR_Handler (void) interrupt RTC_VECTOR
{
	// TODO: 在此处添加用户代码
	if(RTCIF & 0x80)    //闹钟中断
	{
		P01 = !P01;
		RTCIF &= ~0x80;
		B_Alarm = 1;
	}

	if(RTCIF & 0x08)    //秒中断
	{
		P00 = !P00;
		RTCIF &= ~0x08;
		B_1S = 1;
	}
}


