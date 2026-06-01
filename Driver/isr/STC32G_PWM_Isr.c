/**
 * @file    STC32G_PWM_Isr.c
 * @brief   STC32G PWM 中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件承载 PWM 相关中断入口。当前项目电机输出主要依赖 PWM 波形本身，
 * 并不在这里实现上层混控或航向保持逻辑。
 */

#include "STC32G_PWM.h"

//========================================================================
// 函数: PWMA_ISR_Handler
// 描述: PWMA中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2023-04-15
//========================================================================
void PWMA_ISR_Handler (void) interrupt PWMA_VECTOR
{
	// TODO: 在此处添加用户代码
    if (PWMA_SR1 & 0x01)    //UIFA 更新中断
    {
        PWMA_SR1 &= ~0x01;

    }
    if (PWMA_SR1 & 0x02)    //CC1IF 捕获/比较中断
    {
        PWMA_SR1 &= ~0x02;

    }
    if (PWMA_SR1 & 0x04)    //CC2IF 捕获/比较中断
    {
        PWMA_SR1 &= ~0x04;

    }
    if (PWMA_SR1 & 0x08)    //CC3IF 捕获/比较中断
    {
        PWMA_SR1 &= ~0x08;

    }
    if (PWMA_SR1 & 0x10)    //CC4IF 捕获/比较中断
    {
        PWMA_SR1 &= ~0x10;

    }
    if (PWMA_SR1 & 0x20)    //COMIFA 中断
    {
        PWMA_SR1 &= ~0x20;

    }
    if (PWMA_SR1 & 0x40)    //TIFA 触发器中断
    {
        PWMA_SR1 &= ~0x40;

    }
    if (PWMA_SR1 & 0x80)    //BIFA 刹车中断
    {
        PWMA_SR1 &= ~0x80;

    }
}

//========================================================================
// 函数: PWMB_ISR_Handler
// 描述: PWMB中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2023-04-15
//========================================================================
void PWMB_ISR_Handler (void) interrupt PWMB_VECTOR
{
	// TODO: 在此处添加用户代码
    if (PWMB_SR1 & 0x01)    //UIFB 更新中断
    {
        PWMB_SR1 &= ~0x01;

    }
    if (PWMB_SR1 & 0x02)    //CC5IF 捕获/比较中断
    {
        PWMB_SR1 &= ~0x02;

    }
    if (PWMB_SR1 & 0x04)    //CC6IF 捕获/比较中断
    {
        PWMB_SR1 &= ~0x04;

    }
    if (PWMB_SR1 & 0x08)    //CC7IF 捕获/比较中断
    {
        PWMB_SR1 &= ~0x08;

    }
    if (PWMB_SR1 & 0x10)    //CC8IF 捕获/比较中断
    {
        PWMB_SR1 &= ~0x10;

    }
    if (PWMB_SR1 & 0x20)    //COMIFB 中断
    {
        PWMB_SR1 &= ~0x20;

    }
    if (PWMB_SR1 & 0x40)    //TIFB 触发器中断
    {
        PWMB_SR1 &= ~0x40;

    }
    if (PWMB_SR1 & 0x80)    //BIFB 刹车中断
    {
        PWMB_SR1 &= ~0x80;

    }
}


