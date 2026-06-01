/**
 * @file    STC32G_Delay.c
 * @brief   STC32G 阻塞延时实现
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件实现微秒/毫秒级阻塞延时。
 * 当前项目仅应在初始化、复位等待等少量场景使用，主循环实时路径不应依赖大量阻塞延时。
 */

#include	"STC32G_Delay.h"

//========================================================================
// 函数: void delay_ms(unsigned int ms)
// 描述: 延时函数。
// 参数: ms,要延时的ms数, 这里只支持1~65535ms. 自动适应主时钟.
// 返回: none.
// 版本: VER1.0
// 日期: 2021-3-9
// 备注: 
//========================================================================
void delay_ms(unsigned int ms)
{
	unsigned int i;
	do{
		i = MAIN_Fosc / 6030;
		while(--i);
	}while(--ms);
}
