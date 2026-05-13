/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#include "Task.h"

static volatile u32 g_task_tick_ms = 0;

void Task_Marks_Handler_Callback(void)
{
    g_task_tick_ms++;
}

u32 Task_GetTickMs(void)
{
    u32 tick;
    u8 ea_bak;

    ea_bak = EA;
    EA = 0;
    tick = g_task_tick_ms;
    EA = ea_bak;

    return tick;
}

void Task_Pro_Handler_Callback(void)
{
}
