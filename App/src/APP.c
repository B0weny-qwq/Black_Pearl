/**
 * @file    APP.c
 * @brief   STC 官方 App 示例聚合初始化
 * @author  STCAI / boweny
 * @date    2026-05-31
 *
 * @details
 * 本文件原本用于统一启动官方示例工程中的 Lamp、DMA、UART、EEPROM、LIN、SPI 等演示模块。
 * 当前 Black Pearl 工程仅保留 `Lamp_init()` 作为板级显示/按键基础能力，其余示例初始化默认关闭，
 * 避免与 GPS 的 UART2/Timer2、日志 UART1 以及当前控制链路发生资源冲突。
 *
 * @note 当前职责边界：
 * - `APP_config()` 只是“可选示例初始化入口”，不是当前项目主循环入口；
 * - 真实运行链路为 `SYS_Init() -> MainLoop_Bootstrap() -> MainLoop_RunOnce()`；
 * - 若重新启用某个官方示例，需先确认其不会占用当前业务已使用的串口、定时器或 DMA 资源。
 */

#include	"APP.h"

//========================================================================
//                               本地常量声明	
//========================================================================

u8 code t_display[]={                       //标准字库
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71,
//black  -     H    J    K    L    N    o   P    U     t    G    Q    r   M    y
    0x00,0x40,0x76,0x1E,0x70,0x38,0x37,0x5C,0x73,0x3E,0x78,0x3d,0x67,0x50,0x37,0x6e,
    0xBF,0x86,0xDB,0xCF,0xE6,0xED,0xFD,0x87,0xFF,0xEF,0x46};    //0. 1. 2. 3. 4. 5. 6. 7. 8. 9. -1

u8 code T_COM[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};      //位码

u8 code T_KeyTable[16] = {0,1,2,0,3,0,0,0,4,0,0,0,0,0,0,0};

//========================================================================
//                               本地变量声明
//========================================================================

u8  LED8[8];        //显示缓冲
u8  display_index;  //显示位索引

u8  IO_KeyState, IO_KeyState1, IO_KeyHoldCnt;    //行列键盘变量
u8  KeyHoldCnt; //键按下计时
u8  KeyCode;    //给用户使用的键码
u8  cnt50ms;

u8  hour,minute,second; //RTC变量
u16 msecond;

u8 Key1_cnt;
u8 Key2_cnt;
bit Key1_Flag;
bit Key2_Flag;

//========================================================================
// 函数: APP_config
// 描述: 用户应用程序初始化.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-24
//========================================================================
/**
 * @brief   初始化当前启用的 App 示例模块
 * @return  无
 *
 * @details
 * 当前版本仅执行 `Lamp_init()`，保留板级灯光/显示基础能力。
 * 其它示例初始化默认注释掉，其中 `ADtoUART_init()` 尤其不能直接恢复，
 * 因为 Timer2 已被当前 GPS UART2 链路占用。
 */
void APP_config(void)
{
	Lamp_init();
//	ADtoUART_init();   /* Disabled: Timer2 is reserved for GPS UART2. */
//	INTtoUART_init();
//	RTC_init();
//	I2C_PS_init();
//	SPI_PS_init();
//	EEPROM_init();
//	WDT_init();
//	PWMA_Output_init();
//	PWMB_Output_init();
//	DMA_AD_init();
//	DMA_M2M_init();
//	DMA_UART_init();
//	DMA_SPI_PS_init();
//	DMA_LCM_init();
//	DMA_I2C_init();
//	CAN_init();
//	LIN_init();
//	USART_LIN_init();
//	USART2_LIN_init();
//	HSSPI_init();
//	HSPWM_init();
}

//========================================================================
// 函数: DisplayScan
// 描述: 显示扫描函数.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-25
//========================================================================
/**
 * @brief   扫描 8 位数码管显示缓冲
 * @return  无
 *
 * @details
 * 本函数只做板级显示复用扫描，使用 `LED8[]` 中的段码数据与 `display_index`
 * 循环驱动数码管。它与船控主业务无直接关系，仅保留作板级调试显示能力。
 */
void DisplayScan(void)
{   
    P7 = ~T_COM[7-display_index];
    P6 = ~t_display[LED8[display_index]];
    if(++display_index >= 8)    display_index = 0;  //8位结束回0
}
