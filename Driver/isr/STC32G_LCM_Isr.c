/**
 * @file    STC32G_LCM_Isr.c
 * @brief   STC32G LCM 中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件承载液晶控制模块相关中断入口，当前项目默认不依赖该显示链路。
 */

#include "STC32G_LCM.h"

bit LcmFlag;

//========================================================================
// 函数: LCM_ISR_Handler
// 描述: LCM 中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2022-03-23
//========================================================================
void LCM_ISR_Handler (void) interrupt LCM_VECTOR
{
	// TODO: 在此处添加用户代码
	if(LCMIFSTA & 0x01)
	{
		LCMIFSTA = 0x00;
		LcmFlag = 0;
	}
}


