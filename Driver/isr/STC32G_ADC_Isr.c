/**
 * @file    STC32G_ADC_Isr.c
 * @brief   STC32G ADC 中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件承载 ADC 中断入口，主要服务于官方示例或后续扩展采样场景。
 */

#include	"STC32G_ADC.h"

//========================================================================
// 函数: ADC_ISR_Handler
// 描述: ADC中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2020-09-23
//========================================================================
void ADC_ISR_Handler (void) interrupt ADC_VECTOR
{
	ADC_FLAG = 0;			//清除中断标志
	
	// TODO: 在此处添加用户代码
	
}


