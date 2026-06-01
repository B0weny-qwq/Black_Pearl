/**
 * @file    STC32G_Compare_Isr.c
 * @brief   STC32G 比较器中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件承载片上比较器中断入口，当前项目默认未把它纳入主业务链路。
 */

#include "STC32G_Compare.h"

//========================================================================
// 函数: CMP_ISR_Handler
// 描述: 比较器中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2020-09-23
//========================================================================
void CMP_ISR_Handler (void) interrupt CMP_VECTOR
{
	CMPIF = 0;			//清除中断标志
	
	// TODO: 在此处添加用户代码
	P47 = CMPRES;	//中断方式读取比较器比较结果
}
