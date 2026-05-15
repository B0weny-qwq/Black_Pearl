#include "LT8920_SPI.h"

#include "wirelessProtocal.h"

unsigned char RegH,RegL;


unsigned char RXBusy_Send=0,RXBusy_Rec=0;
unsigned char Rx_TimeOUT=0;			//1mS++ / 或者放在定时器�??�??+1
unsigned char transferKey[2]={0};
unsigned char RF_Receive_Channel[3] = {0};


const unsigned char LDT89xxconfig[CFG_LEN][3]= 
{
	{0, 0x6F, 0xE0},
	{1, 0x56, 0x81},
	{2, 0x66, 0x17},
	{4, 0x9C, 0xC9},
	{5, 0x66, 0x37},
	{7, 0x00, 0x30},	//channel freq and Start/Stop Tx,Rx
	{8, 0x6C, 0x90},	

	{9, 0x48, 0x00},	//0x4800(max:6dBm)
	// {9,RF_Power>>8,RF_Power&0xFF},		//设置功率

	{10, 0x7F, 0xFD},
	{11, 0x00, 0x08},	//RSSI on
	{12, 0x00, 0x00},
	{13, 0x48, 0xBD},

	{22, 0x00, 0xff},
	{23, 0x80, 0x05},
	{24, 0x00, 0x67},
	{25, 0x16, 0x59},
	{26, 0x19, 0xE0},
	
#ifdef Air_rate_1M
	{27, 0x13, 0x01},		//1Mbps
#else
	{27, 0x13, 0x00},		//exclude 1Mbps
#endif
	
	{28, 0x18, 0x00},
	
	{32, 0x48, 0x00},
	{33, 0x3f, 0xC7},
	{34, 0x20, 0x00},
	// {35, 0x03, 0x00},
	{35, 0x00, 0x00},
	{36, 0x03, 0x80},
  {37, 0x03, 0x80},
	// {37, 0x06, 0x8C},
	{38, 0x5A, 0x5A},
	// {39, 0x5A, 0x5A},
  {39, 0x03, 0x80},

	{40, 0x44, 0x02},
	{41, 0xB0, 0x00}, 
	{42, 0xFD, 0xB0},
	{43, 0x00, 0x0F},
	
#ifdef Air_rate_62K5
	//62.5K
	{44, 0x10, 0x00},
	{45, 0x05, 0x52},
#endif
#ifdef Air_rate_125K	
	//125K
	{44, 0x08, 0x00},
	{45, 0x05, 0x52},	
#endif
#ifdef Air_rate_250K	
	//250K
	{44, 0x04, 0x00},
	{45, 0x05, 0x52},		
#endif
#ifdef Air_rate_1M	
	//1Mbps
	{44, 0x01, 0x00},
	{45, 0x01, 0x52},		
#endif
	{50, 0x00, 0x00},
	// {52, 0x80, 0x80},
};


//----------------------------------------------------------
//unsigned char SNDLY;

void SPI_SendByte(unsigned char buf)
{
#if SlowSPI_io	
  unsigned char mcnt;
  //unsigned int SNDLY;
  for(mcnt=0;mcnt<8;mcnt++)
  {
    RF_CLK_L; 	//Bus ready;
    _nop_();_nop_();
    //for(SNDLY=0;SNDLY<500;SNDLY++);
    if(buf&0x80)   	RF_MOSI_H;
    else  	 	RF_MOSI_L;
    
    RF_CLK_H;	//Send bit;	
    _nop_();_nop_();
    //for(SNDLY=0;SNDLY<500;SNDLY++);
    buf<<=1;
    
  }
  RF_CLK_L; 	//Bus ready;
  //RF_MOSI_L;	
#else
  RF_CLK_H;
  if(buf&0x80)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x40)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x20)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x10)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x08)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x04)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x02)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
  
  RF_CLK_H;
  if(buf&0x01)	RF_MOSI_H;
  else			RF_MOSI_L;
  RF_CLK_L;
#endif	  
}

unsigned char SPI_ReadByte(uint8_t role)
{
  unsigned char retvalue=0;	
#if SlowSPI_io	
  unsigned char mcnt;
  //unsigned int SNDLY;
  RF_CLK_L;		//Bus ready
  for (mcnt=0;mcnt<8;mcnt++)
  {
    RF_CLK_H;	//Slaver Chip OutPut Data
    //for(SNDLY=0;SNDLY<1050;SNDLY++);
    retvalue<<=1;
    RF_CLK_L;		//Bus ready	
    //for(SNDLY=0;SNDLY<1050;SNDLY++);
    _nop_();_nop_();_nop_();_nop_();_nop_();_nop_();
    if(RF_MISO_SEND)	retvalue|=1;
    else		retvalue&=0xfe;
    
  }
  RF_CLK_L;		//Bus ready	

#else
if(role==LT8920_SEND){
 RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X80;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X40;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X20;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X10;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X08;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X04;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X02;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_SEND)	retvalue|=0X01;
}else{
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X80;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X40;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X20;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X10;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X08;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X04;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X02;
  
  RF_CLK_H;
  RF_CLK_L;
  if(RF_MISO_REC)	retvalue|=0X01;
}
 
  
#endif  
  return retvalue;
}

void LT_ReadReg(unsigned char reg,uint8_t role)	//数据更新在RegH，RegL
{
  if(role==LT8920_SEND){
    RF_SS_SEND_L;
  }else{
    RF_SS_REC_L;
  }
  
  SPI_SendByte(reg|0x80);	//Read MSB.7=1;	
  RegH=SPI_ReadByte(role); 
  RegL=SPI_ReadByte(role); 
  
  if(role==LT8920_SEND){
    RF_SS_SEND_H;
  }else{
    RF_SS_REC_H;
  }


}

#if 0
uint16_t LT_ReadRegReply(unsigned char reg,uint8_t role)		//返回16位寄存器数据
{
  if(role==LT8920_SEND){
    RF_SS_SEND_L;
  }else{
    RF_SS_REC_L;
  }
  
  SPI_SendByte(reg|0x80);	//Read MSB.7=1;	
  RegH=SPI_ReadByte(role); 
  RegL=SPI_ReadByte(role); 
  if(role==LT8920_SEND){
    RF_SS_SEND_H;
  }else{
    RF_SS_REC_H;
  }
	
  return (RegH*256)+RegL;
}

#endif

void LT_WriteReg(unsigned char reg, unsigned char H, unsigned char L,uint8_t role)
{
  if(role==LT8920_SEND){
    RF_SS_SEND_L;
  }else{
    RF_SS_REC_L;
  }
  
  SPI_SendByte(reg); 	//Read MSB.7=0;
  SPI_SendByte(H);
  SPI_SendByte(L);
  if(role==LT8920_SEND){
    RF_SS_SEND_H;
  }else{
    RF_SS_REC_H;
  }
}

void LT_WriteBUF(unsigned char reg, unsigned char *pBuf, unsigned char len,uint8_t role)
{
  unsigned char i;
  
  if(role==LT8920_SEND){
    RF_SS_SEND_L;
  }else{
    RF_SS_REC_L;
  }
  
  SPI_SendByte(reg); 	//Read MSB.7=0;
  SPI_SendByte(len);	//Lenth
  for(i=0; i<len; i++)
    SPI_SendByte(pBuf[i]);
  if(role==LT8920_SEND){
    RF_SS_SEND_H;
  }else{
    RF_SS_REC_H;
  }
}

unsigned char LT_ReadBUF(unsigned char reg, unsigned char *pBuf,uint8_t role)
{
  unsigned char i,len;
  
  if(role==LT8920_SEND){
    RF_SS_SEND_L;
  }else{
    RF_SS_REC_L;
  }
  
  SPI_SendByte(reg|0X80); 	//Read MSB.7=0;
  len=SPI_ReadByte(role);
  if (len>64)
  {
    len = 64;
  }
  
  for(i=0; i<len; i++)
  {
    pBuf[i] = SPI_ReadByte(role);
  }
  
  if(role==LT8920_SEND){
    RF_SS_SEND_H;
  }else{
    RF_SS_REC_H;
  }
  
  return len;
}

unsigned char LT89xx_INIT(void)
{
  unsigned char CFG_CNT;
  uint8_t DATA[3] = {0};
  uint8_t checkSuccessTimes = 0,checkTimes = 10;
	

  GPIO_InitTypeDef	GPIO_InitStructure;        
	GPIO_InitStructure.Pin  = GPIO_Pin_4|GPIO_Pin_3|GPIO_Pin_5;//sck cs mosi
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);
	GPIO_InitStructure.Pin  = GPIO_Pin_7;//rst
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);
		
	GPIO_InitStructure.Pin  = GPIO_Pin_6|GPIO_Pin_2;//miso pkt
	GPIO_InitStructure.Mode = GPIO_PullUp;         
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);

#ifdef HW_V16
  	GPIO_InitStructure.Pin  = GPIO_Pin_3;//miso
	GPIO_InitStructure.Mode = GPIO_PullUp;         
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure);

  	GPIO_InitStructure.Pin  = GPIO_Pin_2;//cs
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure); 
#else
	GPIO_InitStructure.Pin  = GPIO_Pin_2;//miso
	GPIO_InitStructure.Mode = GPIO_PullUp;         
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure);

  	GPIO_InitStructure.Pin  = GPIO_Pin_3;//cs
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure); 
#endif
	
  while(checkTimes-- > 0)
  {
		RF_RST_H; 	  delay_ms(10);  
    RF_RST_L; 	  delay_ms(100);
    RF_RST_H; 	  delay_ms(100);     
      
    RF_CLK_L;
    for(CFG_CNT=0;CFG_CNT<CFG_LEN;CFG_CNT++)
    {
      LT_WriteReg(LDT89xxconfig[CFG_CNT][0],LDT89xxconfig[CFG_CNT][1],LDT89xxconfig[CFG_CNT][2],LT8920_SEND);
      LT_WriteReg(LDT89xxconfig[CFG_CNT][0],LDT89xxconfig[CFG_CNT][1],LDT89xxconfig[CFG_CNT][2],LT8920_REC);
      DATA[0]=LDT89xxconfig[CFG_CNT][0];
      DATA[1]=LDT89xxconfig[CFG_CNT][1];
      DATA[2]=LDT89xxconfig[CFG_CNT][2];	

    }

		checkSuccessTimes = 0;

    LT_ReadReg(0,LT8920_SEND);
    if(RegH==0x6F&&RegL==0XE0)	
		{
      printf("RF tx init OK!\r\n");
      checkSuccessTimes++;
		}
		else
		{ 
			printf("RF tx init ERROR!\r\n");
			//return 1;	//error
		}

    LT_ReadReg(0,LT8920_REC);
    if(RegH==0x6F&&RegL==0XE0)	
		{
      printf("RF Rx init OK!\r\n");
      checkSuccessTimes++;
		}
		else
		{
			printf("RF tx init ERROR!\r\n");
			//return 1;	//error
		}
    if(checkSuccessTimes == 2){
      break;
    }
  }
	return 0;
}



unsigned char LT8920_GetPKT(uint8_t role)
{

	#if (PKT_readReg == 1)
		LT_ReadReg(48,role);					//读取寄存器的PKT状�?
		if(RegL&0x40)	return 1;
		else					return 0;
	#endif	
	
}

#if 0
void LT8920_Carrier_Wave(unsigned char FreqChannel,uint8_t role)
{
	//单载�??
	LT_WriteReg(32,0x18,0X07,role);
	LT_WriteReg(34,0x83,0X0B,role);
	LT_WriteReg(11,0x80,0X08,role);			
	LT_WriteReg(07,0X01,FreqChannel,role);	
}

void LT8920_Sleep(uint8_t role)
{
	LT_WriteReg(07,0X00,0x00,role);		//IDLE
	LT_WriteReg(35,0X43,0x00,role);		//进入LT89xx休眠	
}

void LT8920_Wakeup(uint8_t role)
{
	if(role==LT8920_SEND){
    RF_SS_SEND_L;
  }else{
    RF_SS_REC_L;
  }
  	
	delay_ms(5);					//SS拉低要大�??2ms
  if(role==LT8920_SEND){
    RF_SS_SEND_H;
  }else{
    RF_SS_REC_H;
  }
}
#endif

void LT8920_TxData(unsigned char FreqChannel,unsigned char *pBuf,unsigned char length,uint8_t role)
{
	LT_WriteReg(7,0,FreqChannel,role);	//IDLE
	LT_WriteReg(52, 0x80, 0x80,role);	//清除指针
	// LT_WriteReg( 8, 0x6c, 0x90);
	LT_WriteBUF(50,pBuf,length,role);	//写入fifo
	
	LT_WriteReg(7,1,FreqChannel,role); //发射数据
	
	delay_us(100);
	while(LT8920_GetPKT(role) ==0)
	{
		#if (PKT_readReg == 0)
			delay_us(100);				//读取PKT引脚
		#endif		
		#if (PKT_readReg == 1)
			delay_us(1000);				//查�?�寄存器的PKT状�?
		#endif			
	}
	
	LT_WriteReg(7,0,FreqChannel,role);//发完数据,进入IDLE

  if(role == LT8920_SEND){
    	RXBusy_Send=0;
  }else{
    	RXBusy_Rec=0;
  }
}

#if 0
unsigned char LT8920_RxData(unsigned char FreqChannel,unsigned char *pBuf,unsigned char length,uint8_t role)
{
  unsigned char Status=0,len,RXBusy;
	#if (PKT_readReg == 1)	//寄存器查�??PKT�??1mS查�??一�??
		delay_us(1000);
	#endif				
	
  if(role == LT8920_SEND){
    RXBusy = RXBusy_Send;
  }else{
    RXBusy = RXBusy_Rec;
  }
	if(RXBusy==0)
	{
		RXBusy=1;
		LT_WriteReg(7,0,FreqChannel,role);
		
		LT_WriteReg(52, 0x80, 0x80,role);
		LT_WriteReg( 8, 0x6c, 0x90,role);
		LT_WriteReg(7,0,FreqChannel|0X80,role); 
		return Status;
	}
	if(LT8920_GetPKT(role) && RXBusy)
	{
		#if (PKT_readReg == 0)
			LT_ReadReg(48);	
		#endif				
		RXBusy=0; 

    if(role == LT8920_SEND){
      RXBusy_Send = RXBusy;
    }else{
      RXBusy_Rec = RXBusy;
    }

  
		Rx_TimeOUT=0;
		if((RegH&0x80)==0)
		{
			len = LT_ReadBUF(50,pBuf,role);
			return len;
		}
	}	
	//Rx_TimeOUT++;			//1mS++ / 或者放在定时器�??�??+1
	if(Rx_TimeOUT>100)
	{
		Rx_TimeOUT=0;
		if(role == LT8920_SEND){
      RXBusy_Send = RXBusy;
    }else{
      RXBusy_Rec = RXBusy;
    }
	}			
	return Status;	
}

#endif

void LT8920_OpenRx(unsigned char FreqChannel,uint8_t role)
{
    if(role == LT8920_SEND){
      RXBusy_Send = 1;
    }else{
      RXBusy_Rec = 1;
    }
		LT_WriteReg(7,0,FreqChannel,role);
		LT_WriteReg(52, 0x80, 0x80,role);
    LT_WriteReg( 8, 0x6c, 0x90,role);
		LT_WriteReg(7,0,FreqChannel|0X80,role); 	
} 




uint8_t RF_DATA[100];


void RF_Receive(uint8_t FreqChannel,uint8_t role){
	
	uint8_t len = 0,RXBusy;
	
	if(role == LT8920_SEND){
		RXBusy = RXBusy_Send;
	}else{
		RXBusy = RXBusy_Rec;
	}
	if(RXBusy == 0){
		LT8920_OpenRx(FreqChannel,role);
	}
	if(LT8920_GetPKT(role) && RXBusy==1){
		#if (PKT_readReg == 0)
		LT_ReadReg(48);	
		#endif	
		RXBusy=0; 
		if(role == LT8920_SEND){
		  RXBusy_Send = RXBusy;
		}else{
		  RXBusy_Rec = RXBusy;
		}

		Rx_TimeOUT = 0;

		// len = LT_ReadBUF(32,RF_DATA); 
		// return array:00 3f c7 20 00 00 00 6e 6e 03 80 5a 5a fc fc 44 02 b0 00 fd b0 00 0f 10 12 05 52 3f 12 12 12 12 0e 12 12 06 65 65 a0 65 65 a0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  end
		len = LT_ReadBUF(50,RF_DATA,role);
		// return array:06 65 65 a0 65 65 a0  end
		// printf("rf len=%bd\r\n",len);
		if(len > 0){
		  WirelessProtocal_Receive_Handle(RF_DATA,len);
		}
	}
	else{
		Rx_TimeOUT++;
	}

	if(Rx_TimeOUT > 10){
		RXBusy = 0;
		if(role == LT8920_SEND){
		  RXBusy_Send = RXBusy;
		}else{
		  RXBusy_Rec = RXBusy;
		}
		Rx_TimeOUT = 0;
	}
}


