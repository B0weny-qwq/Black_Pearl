/**
 * @file STC32G_LCM.c
 * @brief STC32G LCM 并口控制器初始化驱动。
 * @details 根据 App 层传入的 LCM_InitTypeDef 配置接口模式、数据位宽和时序参数；
 *          本文件只负责 LCM 外设寄存器初始化，显示缓冲、DMA 搬运和中断处理分别由 App/ISR 层维护。
 * @note Graphify 关系：LCM_Inilize() 被 APP_DMA_LCM 的 DMA_LCM_init() 初始化链路调用。
 */
/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#include "STC32G_LCM.h"

//========================================================================
// 函数: void LCM_Inilize(LCM_InitTypeDef *LCM)
// 描述: LCM 初始化程序.
// 参数: LCM: 结构参数,请参考LCM.h里的定义.
// 返回: none.
// 版本: V1.0, 2021-06-02
//========================================================================
void LCM_Inilize(LCM_InitTypeDef *LCM)
{
	LCMIFSTA = 0x00;
	if(LCM->LCM_Mode == MODE_M6800)		LCMIFCFG |= MODE_M6800;	//LCM接口模式：M6800
	else LCMIFCFG &= ~MODE_M6800;	//LCM接口模式：I8080
	
	if(LCM->LCM_Bit_Wide == BIT_WIDE_16)		LCMIFCFG |= BIT_WIDE_16;	//LCM数据宽度：16位
	else LCMIFCFG &= ~BIT_WIDE_16;	//LCM数据宽度：8位
	
	if(LCM->LCM_Setup_Time <= 7) LCMIFCFG2 = (LCMIFCFG2 & ~0x1c) | (LCM->LCM_Setup_Time << 2);	//LCM通信数据建立时间：0~7
	if(LCM->LCM_Hold_Time <= 3) LCMIFCFG2 = (LCMIFCFG2 & ~0x03) | LCM->LCM_Hold_Time;	//LCM通信数据建立时间：0~7
	
	if(LCM->LCM_Enable == ENABLE)		LCMIFCR |= 0x80;	//使能LCM接口功能
	else LCMIFCR &= ~0x80;	//禁止LCM接口功能
}
