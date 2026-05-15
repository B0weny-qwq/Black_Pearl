#ifndef _LT8910drv_SPI_H__
#define _LT8910drv_SPI_H__

    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_Delay.h"
    
    #include "utils_printf.h"

    #define CFG_LEN  34
    

    #define LT89xx_6dBm			0x4800
    #define LT89xx_2dBm			0X1840
    #define LT89xx_1_7dBm		0X18C0
    #define LT89xx_1dBm			0X1940
    #define LT89xx_n0_3dBm	    0X19C0
    #define LT89xx_n1_4dBm	    0X1A40	
    #define LT89xx_n2_2dBm	    0X1AC0
    #define LT89xx_n3dBm		0X1B40
    #define LT89xx_n4dBm		0X1BC0	
    #define LT89xx_n6_5dBm	    0X1C40
    #define LT89xx_n7_3dBm	    0X1CC0	
    #define LT89xx_n8_2dBm	    0X1D40
    #define LT89xx_n9_5dBm	    0X1DC0	
    #define LT89xx_n10_7dBm	    0X1E40   	
    #define LT89xx_n12_2dBm	    0X1EC0	
    #define LT89xx_n14_2dBm	    0X1F40
    #define LT89xx_n17dBm		0X1FC0	




    //LT8920�??件脚位连接和控制
    ///////////////////////////////////////////////////////////////////////////////

    #define RF_MOSI_H	P35 = 1
    #define RF_MOSI_L	P35 = 0



    #define RF_CLK_H	P34 = 1
    #define RF_CLK_L	P34 = 0		
#if 1
    #define RF_SS_SEND_H	P33 = 1
    #define RF_SS_SEND_L	P33 = 0			

    #define RF_SS_REC_H		P03 = 1
    #define RF_SS_REC_L		P03 = 0	

    #define	RF_MISO_SEND 	P36	
    #define	RF_MISO_REC 	P02
#else
    #define RF_SS_SEND_H	P03 = 1
    #define RF_SS_SEND_L	P03 = 0			

    #define RF_SS_REC_H		P33 = 1
    #define RF_SS_REC_L		P33 = 0	

    #define	RF_MISO_SEND 	P02	
    #define	RF_MISO_REC 	P36	


#endif



    #define RF_RST_H	P37 = 1
    #define RF_RST_L	P37 = 0

    #define RF_PKT		P32	

    #define SlowSPI_io	0	//1=LT8920与MCU低速通�??   0=�??速SPI

    #define PKT_readReg	1 //PKT状态�?�取方式  0=读取�??件�??�??   1=读取寄存器状�??

    ///////////////////////////////////////////////////////////////////////////////
    //                  根据用户应用，以下部分可能需要修�??                       //
    ///////////////////////////////////////////////////////////////////////////////
    #define RF_Power	LT89xx_6dBm


    //通�??频点	
    #define RadioFrequency_user	2450
    #define Test_Channel	    RadioFrequency_user-2402

    #define Pair_Channel	    0x7F
    //发包长度 最�??63Byte
    #define Packet_Length		12   //Byte

    //发包间隔
    #define Tx_Interval_mS	10	 //mS

    //模式设置
    #define Work_Type		0	//1=Tx 0=Rx 2=单载�?? 3=休眠与唤�?? 4=认证测试

    //速率设置
    //#define Air_rate_1M
    //#define Air_rate_250K
    //#define Air_rate_125K
    #define Air_rate_62K5

    //通�??同�?�字
    // #define SyncPairWord	0x03,0x80,0x5A,0x5A
    #define SyncPairWord	0xE4,0xE4,0xE0,0xE0

    #define SyncTransferWord	0x6E,0x6E,0xFC,0xFC

    typedef enum{
        LT8920_SEND = 0,
        LT8920_REC,
    }LT8920_ROLE;

void LT_ReadReg(unsigned char reg,uint8_t role);
uint16_t LT_ReadRegReply(unsigned char reg,uint8_t role);
void LT_WriteReg(unsigned char reg, unsigned char H, unsigned char L,uint8_t role);
void LT_WriteBUF(unsigned char reg, unsigned char *pBuf, unsigned char len,uint8_t role);
unsigned char LT_ReadBUF(unsigned char reg, unsigned char *pBuf,uint8_t role);
unsigned char LT89xx_INIT(void);
unsigned char LT8920_GetPKT(uint8_t role);
void LT8920_Carrier_Wave(unsigned char FreqChannel,uint8_t role);
void LT8920_Sleep(uint8_t role);
void LT8920_Wakeup(uint8_t role);
void LT8920_TxData(unsigned char FreqChannel,unsigned char *pBuf,unsigned char length,uint8_t role);
unsigned char LT8920_RxData(unsigned char FreqChannel,unsigned char *pBuf,unsigned char length,uint8_t role);
void LT8920_OpenRx(unsigned char FreqChannel,uint8_t role);
void RF_Receive(uint8_t FreqChannel,uint8_t role);

