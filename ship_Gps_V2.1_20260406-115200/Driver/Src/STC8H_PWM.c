/*---------------------------------------------------------------------*/
/* --- STC MCU Limited ------------------------------------------------*/
/* --- STC 1T Series MCU Demo Programme -------------------------------*/
/* --- Mobile: (86)13922805190 ----------------------------------------*/
/* --- Fax: 86-0513-55012956,55012947,55012969 ------------------------*/
/* --- Tel: 86-0513-55012928,55012929,55012966 ------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/* --- BBS: www.STCAIMCU.com  -----------------------------------------*/
/* --- QQ:  800003751 -------------------------------------------------*/
/* 如果要在程序中使用此代码,请在程序中注明使用了STC的资料及程序            */
/*---------------------------------------------------------------------*/

#include "STC8H_PWM.h"

//========================================================================
// 函数: PWM_Configuration
// 描述: PWM初始化程序.
// 参数: PWMx: 结构参数,请参考PWM.h里的定义.
// 返回: 成功返回 SUCCESS, 错误返回 FAIL.
// 版本: V1.0, 2012-10-22
//========================================================================
u8 PWM_Configuration(u8 PWM, PWMx_InitDefine *PWMx)
{
	if(PWM == PWM4)
	{
		PWMA_CC4E_Disable();		//关闭输入捕获/比较输出
		PWMA_CC4NE_Disable();		//关闭比较输出
		PWMA_CC4S_Direction(CCAS_OUTPUT);		//CCnS仅在通道关闭时才是可写的
		PWMA_OC4ModeSet(PWMx->PWM_Mode);		//设置输出比较模式

		if(PWMx->PWM_EnoSelect & ENO4P)
		{
			PWMA_CC4E_Enable();			//开启输入捕获/比较输出
			PWMA_ENO |= ENO4P;
		}
		else
		{
			PWMA_CC4E_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO4P;
		}
		if(PWMx->PWM_EnoSelect & ENO4N)
		{
			PWMA_CC4NE_Enable();		//开启输入捕获/比较输出
			PWMA_ENO |= ENO4N;
		}
		else
		{
			PWMA_CC4NE_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO4N;
		}
		
		PWMA_Duty4(PWMx->PWM_Duty);
		
		return	SUCCESS;
	}
	
	if(PWM == PWM3)
	{
		PWMA_CC3E_Disable();		//关闭输入捕获/比较输出
		PWMA_CC3NE_Disable();		//关闭比较输出
		PWMA_CC3S_Direction(CCAS_OUTPUT);		//CCnS仅在通道关闭时才是可写的
		PWMA_OC3ModeSet(PWMx->PWM_Mode);		//设置输出比较模式

		if(PWMx->PWM_EnoSelect & ENO3P)
		{
			PWMA_CC3E_Enable();			//开启输入捕获/比较输出
			PWMA_ENO |= ENO3P;
		}
		else
		{
			PWMA_CC3E_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO3P;
		}
		if(PWMx->PWM_EnoSelect & ENO3N)
		{
			PWMA_CC3NE_Enable();		//开启输入捕获/比较输出
			PWMA_ENO |= ENO3N;
		}
		else
		{
			PWMA_CC3NE_Disable();		//关闭输入捕获/比较输出
			PWMA_ENO &= ~ENO3N;
		}
		
		PWMA_Duty3(PWMx->PWM_Duty);
		
		return	SUCCESS;
	}
	
	if(PWM == PWMA)
	{
//		PWMA_OC1_ReloadEnable(PWMx->PWM_Reload);	//??????????????
//		PWMA_OC1_FastEnable(PWMx->PWM_Fast);		//???????????????
//		PWMA_CCPCAPreloaded(PWMx->PWM_PreLoad);	//????/??????????λ(??λ?????л?????????????????)
//		PWMA_BrakeEnable(PWMx->PWM_BrakeEnable);	//????/??????????

		PWMA_DeadTime(PWMx->PWM_DeadTime);	//??????????????
		PWMA_AutoReload(PWMx->PWM_Period);	//????????
		PWMA_BrakeOutputEnable(PWMx->PWM_MainOutEnable);	//????????
		PWMA_CEN_Enable(PWMx->PWM_CEN_Enable);	//????????
		
		return	0;
	}

	return	FAIL;	//错误
}

//========================================================================
// 函数: UpdatePwm
// 描述: PWM占空比更新程序.
// 参数: PWM: PWM通道/组号, PWMx结构参数,请参考PWM.h里的定义.
// 返回: none.
// 版本: V1.0, 2012-10-22
//========================================================================
void UpdatePwm(u8 PWM, PWMx_Duty *PWMx)
{
	switch(PWM)
	{
		case PWM4:
			PWMA_Duty4(PWMx->PWM1_Duty);
		break;

		case PWM3:
			PWMA_Duty3(PWMx->PWM3_Duty);
		break;

		case PWMA:
			PWMA_Duty4(PWMx->PWM1_Duty);
			PWMA_Duty3(PWMx->PWM3_Duty);
		break;

	}
}

/*********************************************************/
