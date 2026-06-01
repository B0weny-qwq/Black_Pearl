/**
 * @file    STC32G_LIN_Isr.c
 * @brief   STC32G LIN 中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件承载 LIN 外设相关中断入口。当前项目主链路默认不依赖 LIN。
 */

#include	"STC32G_LIN.h"

bit LinRxFlag;

//========================================================================
// 函数: LIN_ISR_Handler
// 描述: LIN中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2022-03-28
//========================================================================
void LIN_ISR_Handler (void) interrupt LIN_VECTOR
{
	u8 isr;
	u8 arTemp;
	arTemp = LINAR;     //LINAR 现场保存，避免主循环里写完 LINAR 后产生中断，在中断里修改了 LINAR 内容
	
	isr = LinReadReg(LSR);		//读取寄存器清除状态标志位
	if((isr & 0x03) == 0x03)
	{
		isr = LinReadReg(LER);
		if(isr == 0x00)		//No Error
		{
			LinRxFlag = 1;
		}
	}
	else
	{
		isr = LinReadReg(LER);	//读取清除错误寄存器
	}

	LINAR = arTemp;    //LINAR 现场恢复
}


