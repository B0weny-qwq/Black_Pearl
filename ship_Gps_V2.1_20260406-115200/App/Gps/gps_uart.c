#include "gps_uart.h"
#include "utils_printf.h"
#include "STC8G_H_NVIC.h"
#include "utils_encodingTransfer.h"
#include "nmea41_protocal.h"

UART_GPS_CACHE idata uart_gps_rcv;
UART_BUFF s_gps_buff;
uint8_t uart_gps_Dma_Out_Times = 0;


void Gps_Uart_Gpio_Init(void){
	GPIO_InitTypeDef	GPIO_InitStructure;		//结构定义

	GPIO_InitStructure.Pin  = GPIO_Pin_0 | GPIO_Pin_1;		//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7
	GPIO_InitStructure.Mode = GPIO_PullUp;	//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);	//初�?�化
}

void Gps_Uart_Init(void){
	COMx_InitDefine		COMx_InitStructure;					//结构定义

	memset(&uart_gps_rcv,0,sizeof(UART_GPS_CACHE));
	memset(&s_gps_buff,0,sizeof(UART_BUFF));
	
	COMx_InitStructure.UART_Mode      = UART_8bit_BRTx;		//模式,   UART_ShiftRight,UART_8bit_BRTx,UART_9bit,UART_9bit_BRTx
//	COMx_InitStructure.UART_BRT_Use   = BRT_Timer2;			//选择波特率发生器, BRT_Timer2 (注意: 串口2固定使用BRT_Timer2, 所以不用选择)
	#if GPS_COM_BPS==115200
	COMx_InitStructure.UART_BaudRate  = 115200ul;	
	#else
	COMx_InitStructure.UART_BaudRate  = 9600ul;			//波特�??,     110 ~ 115200
	#endif
	COMx_InitStructure.UART_RxEnable  = ENABLE;				//接收允�??,   ENABLE或DISABLE
	UART_Configuration(UART2, &COMx_InitStructure);		//初�?�化串口2 UART1,UART2,UART3,UART4
	NVIC_UART2_Init(ENABLE,Priority_1);		//�??�??使能, ENABLE/DISABLE; 优先�??(低到�??) Priority_0,Priority_1,Priority_2,Priority_3
	UART2_SW(UART2_SW_P10_P11);		//UART2_SW_P10_P11,UART2_SW_P46_P47
}


void nmea41_UART_RX_ISR(uint8_t rx_data){
    
    UART_GPS_CACHE *int_rx = &uart_gps_rcv;
    /* save character */
	s_gps_buff.rx_buffer[int_rx->wline][int_rx->save_index] =  rx_data; 
	int_rx->save_index ++;
	if (rx_data == '\n')
	{	
		s_gps_buff.rx_buffer[int_rx->wline][int_rx->save_index] = 0;
		
		int_rx->save_index = 0;
		
		int_rx->wline += 1;
		if (int_rx->wline >= 15)
			int_rx->wline = 0;
	}
	if (int_rx->save_index >= GPS_UART_RX_BUF_SIZE ){
		int_rx->save_index = 0;
	}
	
}

void Gps_Uart_Data_Resolve(void){
	UART_GPS_CACHE *int_rx = &uart_gps_rcv;
	uint8_t i=0,startIndex = 0,endIndex = 0;
	uint8_t wline,rline;
	uint8_t *data_ptr;

	wline = int_rx->wline;
	rline = int_rx->rline;

	if (wline == rline)
		return ;
	data_ptr = &s_gps_buff.rx_buffer[rline][0];

	for(i = 0;i < GPS_UART_RX_BUF_SIZE;i ++){
		
		if(data_ptr[i] == '$'){
			startIndex = i;
		}else if(data_ptr[i] == '\n'){
			endIndex = i;
			nmea41_protocal_Resolve_Handle(&data_ptr[startIndex],endIndex-startIndex+1);
			break;
		}
	}
	int_rx->rline ++;
	if (int_rx->rline >= 15)
		int_rx->rline = 0;
	
}



void UART2_ISR_Handler (void) interrupt UART2_VECTOR
{
	if(RI2)
	{
		CLR_RI2();
        nmea41_UART_RX_ISR(S2BUF);
	}

	if(TI2)
	{
		CLR_TI2();		
        COM2.B_TX_busy = 0;     
	}
}
 