/**
 * @file    STC32G_UART_Isr.c
 * @brief   STC32G UART 中断服务程序
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件在 UART1~UART4 中断里完成最底层的字节收发搬运：
 * - 接收方向：将寄存器中的新字节写入 `RXn_Buffer`，并维护 `COMn.RX_Cnt/RX_TimeOut`
 * - 发送方向：按队列模式从 `TXn_Buffer` 继续发下一个字节
 *
 * 当前工程中最重要的是 UART2 ISR，它只负责把 GPS 原始字节送入 `RX2_Buffer`；
 * `GPS.c` 后续在主循环中自行消费这些字节并做 NMEA 解析。
 *
 * @note 当前职责边界：
 * - ISR 只搬运字节，不做业务级解析；
 * - 不要在此处加入 GPS、无线协议或控制逻辑；
 * - 上层若读取 `RX2_Buffer`，必须自行处理回卷与增量读取。
 */

#include "STC32G_UART.h"

bit B_ULinRX1_Flag;
bit B_ULinRX2_Flag;

//========================================================================
// 函数: UART1_ISR_Handler
// 描述: UART1中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2020-09-23
//========================================================================
#ifdef UART1
void UART1_ISR_Handler (void) interrupt UART1_VECTOR
{
	u8 Status;

	if(RI)
	{
		RI = 0;

		//--------USART LIN---------------
		Status = USARTCR5;
		if(Status & 0x02)     //if LIN header is detected
		{
			B_ULinRX1_Flag = 1;
		}

		if(Status & 0xc0)     //if LIN break is detected / LIN header error is detected
		{
			COM1.RX_Cnt = 0;
		}
		USARTCR5 &= ~0xcb;    //Clear flag
		//--------------------------------
		
        if(COM1.RX_Cnt >= COM_RX1_Lenth)	COM1.RX_Cnt = 0;
        RX1_Buffer[COM1.RX_Cnt++] = SBUF;
        COM1.RX_TimeOut = TimeOutSet1;
	}

	if(TI)
	{
		TI = 0;
		
        #if(UART_QUEUE_MODE == 1)   //判断是否使用队列模式
		if(COM1.TX_send != COM1.TX_write)
		{
		 	SBUF = TX1_Buffer[COM1.TX_send];
			if(++COM1.TX_send >= COM_TX1_Lenth)		COM1.TX_send = 0;
		}
		else	COM1.B_TX_busy = 0;
        #else
        COM1.B_TX_busy = 0;     //使用阻塞方式发送直接清除繁忙标志
        #endif
	}
}
#endif

//========================================================================
// 函数: UART2_ISR_Handler
// 描述: UART2中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2020-09-23
//========================================================================
#ifdef UART2
void UART2_ISR_Handler (void) interrupt UART2_VECTOR
{
	u8 Status;

	if(S2RI)
	{
		CLR_RI2();

		//--------USART LIN---------------
		Status = USART2CR5;
		if(Status & 0x02)     //if LIN header is detected
		{
			B_ULinRX2_Flag = 1;
		}

		if(Status & 0xc0)     //if LIN break is detected / LIN header error is detected
		{
			COM2.RX_Cnt = 0;
		}
		USART2CR5 &= ~0xcb;   //Clear flag
		//--------------------------------
		
        if(COM2.RX_Cnt >= COM_RX2_Lenth)	COM2.RX_Cnt = 0;
        RX2_Buffer[COM2.RX_Cnt++] = S2BUF;
        COM2.RX_TimeOut = TimeOutSet2;
	}

	if(S2TI)
	{
		CLR_TI2();
		
        #if(UART_QUEUE_MODE == 1)   //判断是否使用队列模式
		if(COM2.TX_send != COM2.TX_write)
		{
		 	S2BUF = TX2_Buffer[COM2.TX_send];
			if(++COM2.TX_send >= COM_TX2_Lenth)		COM2.TX_send = 0;
		}
		else	COM2.B_TX_busy = 0;
        #else
        COM2.B_TX_busy = 0;     //使用阻塞方式发送直接清除繁忙标志
        #endif
	}
}
#endif

//========================================================================
// 函数: UART3_ISR_Handler
// 描述: UART3中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2020-09-23
//========================================================================
#ifdef UART3
void UART3_ISR_Handler (void) interrupt UART3_VECTOR
{
	if(S3RI)
	{
		CLR_RI3();

        if(COM3.RX_Cnt >= COM_RX3_Lenth)	COM3.RX_Cnt = 0;
        RX3_Buffer[COM3.RX_Cnt++] = S3BUF;
        COM3.RX_TimeOut = TimeOutSet3;
	}

	if(S3TI)
	{
		CLR_TI3();
		
        #if(UART_QUEUE_MODE == 1)   //判断是否使用队列模式
		if(COM3.TX_send != COM3.TX_write)
		{
		 	S3BUF = TX3_Buffer[COM3.TX_send];
			if(++COM3.TX_send >= COM_TX3_Lenth)		COM3.TX_send = 0;
		}
		else	COM3.B_TX_busy = 0;
        #else
        COM3.B_TX_busy = 0;     //使用阻塞方式发送直接清除繁忙标志
        #endif
	}
}
#endif

//========================================================================
// 函数: UART4_ISR_Handler
// 描述: UART4中断函数.
// 参数: none.
// 返回: none.
// 版本: V1.0, 2020-09-23
//========================================================================
#ifdef UART4
void UART4_ISR_Handler (void) interrupt UART4_VECTOR
{
	if(S4RI)
	{
		CLR_RI4();

        if(COM4.RX_Cnt >= COM_RX4_Lenth)	COM4.RX_Cnt = 0;
        RX4_Buffer[COM4.RX_Cnt++] = S4BUF;
        COM4.RX_TimeOut = TimeOutSet4;
	}

	if(S4TI)
	{
		CLR_TI4();
		
        #if(UART_QUEUE_MODE == 1)   //判断是否使用队列模式
		if(COM4.TX_send != COM4.TX_write)
		{
		 	S4BUF = TX4_Buffer[COM4.TX_send];
			if(++COM4.TX_send >= COM_TX4_Lenth)		COM4.TX_send = 0;
		}
		else	COM4.B_TX_busy = 0;
        #else
        COM4.B_TX_busy = 0;     //使用阻塞方式发送直接清除繁忙标志
        #endif
	}
}
#endif
