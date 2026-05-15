#include "gps_uart.h"
#include "utils_printf.h"
#include "STC8G_H_NVIC.h"
#include "utils_encodingTransfer.h"
#include "nmea41_protocal.h"

UART_GPS_CACHE uart_gps_rcv;
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
	
	COMx_InitStructure.UART_Mode      = UART_8bit_BRTx;		//模式,   UART_ShiftRight,UART_8bit_BRTx,UART_9bit,UART_9bit_BRTx
//	COMx_InitStructure.UART_BRT_Use   = BRT_Timer2;			//选择波特率发生器, BRT_Timer2 (注意: 串口2固定使用BRT_Timer2, 所以不用选择)
	COMx_InitStructure.UART_BaudRate  = 9600ul;			//波特�??,     110 ~ 115200
	COMx_InitStructure.UART_RxEnable  = ENABLE;				//接收允�??,   ENABLE或DISABLE
	UART_Configuration(UART2, &COMx_InitStructure);		//初�?�化串口2 UART1,UART2,UART3,UART4
	NVIC_UART2_Init(ENABLE,Priority_1);		//�??�??使能, ENABLE/DISABLE; 优先�??(低到�??) Priority_0,Priority_1,Priority_2,Priority_3
	UART2_SW(UART2_SW_P10_P11);		//UART2_SW_P10_P11,UART2_SW_P46_P47
}

#if 0
void Gps_Uart_Send_Buff(uint8_t *data_m,uint8_t len){
    uint8_t i = 0;
    for(;i<len;i++){
        TX2_write2buff(data_m[i]);
    }
}


void Gps_Uart_Dma_OutTime_Handle(void){
	UART_GPS_CACHE *int_rx = &uart_gps_rcv;
	uart_gps_Dma_Out_Times++;
	if(uart_gps_Dma_Out_Times >= 3 && int_rx->save_index > 100){
		int_rx->resolveFlag = TRUE;
		// uprintf_array(int_rx->rx_buffer,int_rx->save_index);
	}
}

void Gps_Uart_Dma_OutTime_Clear(void){
	uart_gps_Dma_Out_Times = 0;
}


// ----
uint8_t nmea41_get_checkNum(uint8_t *data_m,uint16_t data_length){

    uint16_t i = 1,calLen = data_length-3;
    uint8_t check_cal_result = 0;
    for(;i<calLen;i++){
        check_cal_result ^= data_m[i];
    }
    return check_cal_result;
}
#endif

#if 1
void nmea41_UART_RX_ISR(uint8_t rx_data){
    
    UART_GPS_CACHE *int_rx = &uart_gps_rcv;
    /* save character */
	int_rx->rx_buffer[int_rx->wline][int_rx->save_index] =  rx_data; 
	int_rx->save_index ++;
	if (rx_data == '\n')
	{	
		int_rx->rx_buffer[int_rx->wline][int_rx->save_index] = 0;
		
		int_rx->save_index = 0;
		
		int_rx->wline += 1;
		if (int_rx->wline >= 8)
			int_rx->wline = 0;
	}
	if (int_rx->save_index >= GPS_UART_RX_BUF_SIZE ){
		int_rx->save_index = 0;
	}
	//Gps_Uart_Dma_OutTime_Clear();
	// printf("%b02x ",rx_data);
    // if(rx_data == '\n') {
    //     printf("end1 \r\n");
	// 	uprintf_array(int_rx->rx_buffer,int_rx->save_index);
    //     if(strstr(int_rx->rx_buffer, "$GP") != NULL) {
	// 		int_rx->resolveFlag = TRUE;
	// 		printf("one nmea \r\n");
    //     }
    // }
	
}

void Gps_Uart_Data_Resolve(void){
	UART_GPS_CACHE *int_rx = &uart_gps_rcv;
	uint16_t i=0,startIndex = 0,endIndex = 0;
	uint8_t wline,rline;
	uint8_t *data_ptr;
#if 0
	memcpy(int_rx->rx_buffer,"$GPRMC,104230.00,A,3728.99359,N,12202.36349,E,0.946,,190524,,,A*7D\n\
$GPVTG,,T,,M,0.946,N,1.753,K,A*28\n\
$GPGGA,104230.00,3728.99359,N,12202.36349,E,1,04,2.36,110.6,M,8.2,M,,*6E\n\
$GPGSA,A,3,13,20,29,30,,,,,,,,,3.30,2.36,2.31*0D\n\
$GPGSV,3,1,10,05,58,058,08,11,17,142,,13,71,045,12,15,70,263,*74\n\
$GPGSV,3,2,10,18,37,314,13,20,36,090,22,23,09,287,19,24,19,181,*75\n\
$GPGSV,3,3,10,29,30,240,08,30,21,050,28*71\n\
$GPGLL,3728.99359,N,12202.36349,E,104230.00,A,A*64\n",449);
uprintf_array(int_rx->rx_buffer,449);
int_rx->resolveFlag = TRUE;

int_rx->save_index = 449;
#endif
	wline = int_rx->wline;
	rline = int_rx->rline;

	if (wline == rline)
		return ;
	data_ptr = &int_rx->rx_buffer[rline][0];

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
	if (int_rx->rline >= 8)
		int_rx->rline = 0;
	
	#if 0
	if(int_rx->resolveFlag == TRUE){
		int_rx->resolveFlag = FALSE;
	}else{
		// printf("not gps dma==%b02x\r\n",int_rx->save_index);
		return;
	}
	// printf(" gps dma start resolve\r\n");
	for(i=0;i<int_rx->save_index;i++){
		if(int_rx->rx_buffer[i] == '$'){
			startIndex = i;
		}else if(int_rx->rx_buffer[i] == '\n'){
			endIndex = i;
			nmea41_protocal_Resolve_Handle(&int_rx->rx_buffer[startIndex],endIndex-startIndex+1);
		}

	}
	int_rx->save_index = 0;
	#endif
	
	
}

#else
uint16_t Nmea41_data_Index = 0;
uint16_t Nmea41_valid_len = 0;
uint8_t  Nmea41_Uart_Rec_Buf[GPS_RX_CACHE_SIZE] = {0};
void nmea41_UART_RX_ISR(uint8_t rx_data){
    
    UART_GPS_CACHE *int_rx = &uart_gps_rcv;
    /* save character */
	int_rx->rx_buffer[int_rx->save_index] =  rx_data; 
	int_rx->save_index ++;
	if (int_rx->save_index >= GPS_UART_RX_BUF_SIZE)
		int_rx->save_index = 0;

	/* if the next position is read index, discard this 'read char' */
	if (int_rx->save_index == int_rx->read_index)
	{
		int_rx->read_index ++;
		if (int_rx->read_index >= GPS_UART_RX_BUF_SIZE)
			int_rx->read_index = 0;
	}
}

uint16_t Nmea41_Uart_Buffer_Read (uint8_t* buffer, uint16_t size)
{
	uint8_t* ptr;
	UART_GPS_CACHE* int_rx = &uart_gps_rcv;

	if (int_rx->read_index == int_rx->save_index)
		return 0;
	
	ptr = buffer;
	
	/* interrupt mode Rx */
	while (size)
	{
        if (int_rx->read_index != int_rx->save_index)
		{
			/* read a character */
			*ptr++ = int_rx->rx_buffer[int_rx->read_index];
			size--;

			/* move to next position */
			int_rx->read_index ++;
			if (int_rx->read_index >= GPS_UART_RX_BUF_SIZE)
				int_rx->read_index = 0;
            
		}
		else
		{
			/* set error code */
			// int_rx->read_index = 0; 
			// int_rx->save_index = 0;
            break;
		}
    }
	return (uint32_t)ptr - (uint32_t)buffer;
}

void Gps_Uart_Data_Resolve(void){
    UART_GPS_CACHE *int_rx = &uart_gps_rcv;

    uint8_t protocalData[GPS_RX_CACHE_SIZE] = {0};
	uint8_t is_data_valid = 1;
	uint8_t is_proto_ok = 0;
    uint8_t checkNum_cal = 0,checkNum_Protocal = 0;
	uint16_t i = 0;

    uint16_t com_data_len = Nmea41_Uart_Buffer_Read((uint8_t *)&protocalData,GPS_RX_CACHE_SIZE);

// $GPVTG,284.53,T,,M,2.440,N,4.518,K,A*3F
// $GPGLL,3731.52627,N,12205.44154,E,073213.00,A,A*6B
	memcpy((uint8_t *)&protocalData,(uint8_t *)&"$GPGLL,3731.52627,N,12205.44154,E,073213.00,A,A*6B\
	$GPVTG,284.53,T,,M,2.440,N,4.518,K,A*3F",50);
	com_data_len = 91;

	if(com_data_len == 0) return;

	for(i=0;i<15;i++){
		// is_data_valid = 1;
		
		switch(Nmea41_data_Index){
			case 0:
				if(protocalData[i]!='$'){
					Nmea41_data_Index = 0;
					is_data_valid = 0;
				}
			break;

			default:
                if(Nmea41_valid_len>0){
                    Nmea41_valid_len --;
                    if(Nmea41_valid_len==0 && Nmea41_data_Index > 0){
                        is_proto_ok = 1;
                    }
                }
                if(protocalData[i]=='*')
                {
                    Nmea41_valid_len  = 2; 				
                }

			break;
		}
		if(is_data_valid){
			Nmea41_Uart_Rec_Buf[Nmea41_data_Index++] = protocalData[i];
			if(is_proto_ok){
                is_proto_ok = 0; 
                checkNum_cal = nmea41_get_checkNum((uint8_t *)&Nmea41_Uart_Rec_Buf,Nmea41_data_Index);
                checkNum_Protocal = charsToHex((uint8_t *)&Nmea41_Uart_Rec_Buf+Nmea41_data_Index-2);
				printf("checkNum_cal==%b02x,%b02x\r",checkNum_cal,checkNum_Protocal);

				uprintf_array((uint8_t *)&Nmea41_Uart_Rec_Buf+1,Nmea41_data_Index-3);
                if(checkNum_cal == checkNum_Protocal){
                    nmea41_protocal_Resolve_Handle((uint8_t *)&Nmea41_Uart_Rec_Buf+1,Nmea41_data_Index-3);
                    memset((uint8_t *)&Nmea41_Uart_Rec_Buf,0,sizeof(Nmea41_Uart_Rec_Buf));
                }
				Nmea41_data_Index = 0;
                Nmea41_valid_len = 0;
			}
		}else{
            Nmea41_valid_len = 0;
		}
	}
}


#endif


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
 