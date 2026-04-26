/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

/**
 * @file    Task.c
 * @brief   任务调度系统 - 定时轮询任务调度器
 * @author  boweny
 * @date    2026-04-22
 * @version v1.0
 *
 * @details
 * - 基于 Timer0 1ms 中断的简易任务调度器（轮询方式，非抢占式）
 * - Task_Comps[] 任务表定义各任务的运行周期
 * - Task_Marks_Handler_Callback() 在 Timer0 ISR 中递减计数器，归零则置 Run=1
 * - Task_Pro_Handler_Callback()  在主循环中轮询，Run=1 时执行任务函数
 *
 * @task-list
 *   - Sample_Lamp       (250ms)  [启用] LED跑马灯
 *   - Sample_ADtoUART  (200ms)  [停用] 为释放 Timer2 给 GPS UART2
 *   - 其他任务全部注释禁用（RTC/SPI/CAN/DMA等）
 *
 * @note    新增任务：参照 Task_Comps[] 已有格式添加，勿修改 Driver 层
 * @note    调度周期建议大于10ms，避免频繁任务切换占用过多CPU
 *
 * @see     Task.h        任务结构体定义
 * @see     APP.c         应用层入口
 */

#include	"Task.h"
#include	"app.h"

//========================================================================
//                               本地变量声明
//========================================================================

static TASK_COMPONENTS Task_Comps[]=
{
//状态  计数  周期  函数
	{0, 250, 250, Sample_Lamp},				/* task 1 Period： 250ms */
//	{0, 200, 200, Sample_ADtoUART},		/* task 2 Period： 200ms */
//	{0, 20, 20, Sample_INTtoUART},		/* task 3 Period： 20ms */
//	{0, 1, 1, Sample_RTC},						/* task 4 Period： 1ms */
//	{0, 1, 1, Sample_I2C_PS},					/* task 5 Period： 1ms */
//	{0, 1, 1, Sample_SPI_PS},					/* task 6 Period： 1ms */
//	{0, 1, 1, Sample_EEPROM},					/* task 7 Period： 1ms */
//	{0, 100, 100, Sample_WDT},				/* task 8 Period： 100ms */
//	{0, 1, 1, Sample_PWMA_Output},		/* task 9 Period： 1ms */
//	{0, 1, 1, Sample_PWMB_Output},		/* task 10 Period： 1ms */
//	{0, 500, 500, Sample_DMA_AD},			/* task 12 Period： 500ms */
//	{0, 500, 500, Sample_DMA_M2M},		/* task 13 Period： 100ms */
//	{0, 1, 1, Sample_DMA_UART},				/* task 14 Period： 1ms */
//	{0, 1, 1, Sample_DMA_SPI_PS},			/* task 15 Period： 1ms */
//	{0, 1, 1, Sample_DMA_LCM},				/* task 16 Period： 1ms */
//	{0, 1, 1, Sample_DMA_I2C},				/* task 17 Period： 1ms */
//	{0, 1, 1, Sample_CAN},						/* task 18 Period： 1ms */
//	{0, 1, 1, Sample_LIN},						/* task 19 Period： 1ms */
//	{0, 1, 1, Sample_USART_LIN},			/* task 20 Period： 1ms */
//	{0, 1, 1, Sample_USART2_LIN},			/* task 21 Period： 1ms */
//	{0, 1, 1, Sample_HSSPI},					/* task 22 Period： 1ms */
//	{0, 1, 1, Sample_HSPWM},					/* task 22 Period： 1ms */
	/* Add new task here */
};

u8 Tasks_Max = sizeof(Task_Comps)/sizeof(Task_Comps[0]);

//========================================================================
// 函数: Task_Handler_Callback
// 描述: 任务标记回调函数.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2012-10-22
//========================================================================
void Task_Marks_Handler_Callback(void)
{
	u8 i;
	for(i=0; i<Tasks_Max; i++)
	{
		if(Task_Comps[i].TIMCount)    /* If the time is not 0 */
		{
			Task_Comps[i].TIMCount--;  /* Time counter decrement */
			if(Task_Comps[i].TIMCount == 0)  /* If time arrives */
			{
				/*Resume the timer value and try again */
				Task_Comps[i].TIMCount = Task_Comps[i].TRITime;  
				Task_Comps[i].Run = 1;    /* The task can be run */
			}
		}
	}
}

//========================================================================
// 函数: Task_Pro_Handler_Callback
// 描述: 任务处理回调函数.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2012-10-22
//========================================================================
void Task_Pro_Handler_Callback(void)
{
	u8 i;
	for(i=0; i<Tasks_Max; i++)
	{
		if(Task_Comps[i].Run) /* If task can be run */
		{
			Task_Comps[i].Run = 0;    /* Flag clear 0 */
			Task_Comps[i].TaskHook();  /* Run task */
		}
	}
}


