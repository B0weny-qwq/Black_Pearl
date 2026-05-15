#include "gyro_i2c.h"

void MyI2C_Init(void)    //SCL和SDA初始�?
{
	GPIO_InitTypeDef	GPIO_InitStructure;        
	// sda
    // P1_PULL_UP_ENABLE(GPIO_Pin_6);
    // P1_DRIVE_HIGH(GPIO_Pin_6);
	GPIO_InitStructure.Pin  = GPIO_Pin_6;
	GPIO_InitStructure.Mode = GPIO_PullUp;         
	GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);

	// scl
	GPIO_InitStructure.Pin  = GPIO_Pin_7;
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);

    P16 = 1;
    P17 = 1; 
}


#if 0

void MyI2C_SDA_OutPP(uint8_t onoff){
    // GPIO_InitTypeDef	GPIO_InitStructure;        
	// GPIO_InitStructure.Pin  = GPIO_Pin_6;
    // if(onoff){
    //     GPIO_InitStructure.Mode = GPIO_OUT_PP;         
    // }else{
    //     GPIO_InitStructure.Mode = GPIO_PullUp;   
    // }
    // GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);
}

void MyI2C_W_SCL(uint8_t BitValue)  //SCL电平状态设置函�?
{
    P17 = BitValue;
	delay_us(5);
}
 
void MyI2C_W_SDA(uint8_t BitValue)  //SDA电平状态设置函�?
{
    P16 = BitValue;
    delay_us(5);
}
 
uint8_t MyI2C_R_SDA(void)  //SDA读取电平状态函�?
{
	uint8_t BitValue;
	BitValue = P16;
	delay_us(10);
	return BitValue;
}

void MyI2C_Start(void)  //IIC起始函数
{
	MyI2C_W_SDA(1);   //先拉高SDA再拉高SCL
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(0);
	MyI2C_W_SCL(0);
}
 
void MyI2C_Stop(void)  //IIC终止函数
{
	MyI2C_W_SDA(0);   //先拉低SDA再拉高SCL再拉高SDA
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(1);
}

void MyI2C_SendByte(uint8_t Byte) //发送一个字节函�?
{
	uint8_t i;
	for (i = 0; i < 8; i ++)
	{
        // if(Byte & 0x80)  MyI2C_W_SDA(1);
		// else            MyI2C_W_SDA(0);
		// Byte <<= 1;

		MyI2C_W_SDA(Byte & (0x80 >> i));  //Byte & 0x80 取出Byte的高�?
		MyI2C_W_SCL(1);
		MyI2C_W_SCL(0);
	}
}

uint8_t MyI2C_ReceiveByte(void)  //接收一个字节函�?
{
	uint8_t i, Byte = 0x00;
	MyI2C_W_SDA(1);   
 
	for (i = 0; i < 8; i ++)
	{
		MyI2C_W_SCL(1);
		if (MyI2C_R_SDA() == 1)
		{
			Byte |= (0x80 >> i);
		}
		MyI2C_W_SCL(0);
	}
	return Byte;
}

void MyI2C_SendAck(uint8_t AckBit) //发送应�?
{
	MyI2C_W_SDA(AckBit);
	MyI2C_W_SCL(1);
	MyI2C_W_SCL(0);
}
 
uint8_t MyI2C_ReceiveAck(void)  //接收应答
{
	uint8_t AckBit;
	MyI2C_W_SDA(1);
	MyI2C_W_SCL(1);
	AckBit = MyI2C_R_SDA();
	MyI2C_W_SCL(0);
	return AckBit;
}


void MyI2C_SendBuffer(uint8_t I2C_Slave_Addr, uint8_t AR_Addr,uint8_t *Buffer, uint32_t len)
{
    int i = 0;
    MyI2C_Start();
    MyI2C_SendByte(I2C_Slave_Addr);
    MyI2C_ReceiveAck();
    MyI2C_SDA_OutPP(TRUE);
    MyI2C_SendByte(AR_Addr);
    MyI2C_SDA_OutPP(FALSE);
    MyI2C_ReceiveAck();

    for (i = 0; i < len; i++)
    {
        MyI2C_SendByte(Buffer[i]);
        MyI2C_ReceiveAck();
    }
    MyI2C_Stop();
}

void MyI2C_Send_Byte(uint8_t I2C_Slave_Addr, uint8_t AR_Addr,uint8_t data_m)
{

    int i = 0;
    MyI2C_Start();
    MyI2C_SendByte(I2C_Slave_Addr);
    MyI2C_ReceiveAck();
    MyI2C_SendByte(AR_Addr);
    MyI2C_ReceiveAck();
    

    MyI2C_SendByte(data_m);
    MyI2C_ReceiveAck();

    MyI2C_Stop();
}

void MyI2C_RecvBuffer(uint8_t I2C_Slave_Addr, uint8_t AR_Addr, uint8_t *Buffer, uint32_t len)
{
    int i = 0;
    // MyI2C_SDA_OutPP(TRUE);
    MyI2C_Start();
 
    MyI2C_SendByte(I2C_Slave_Addr);
    MyI2C_ReceiveAck();
 
    MyI2C_SendByte(AR_Addr);
    // MyI2C_SDA_OutPP(FALSE);
    MyI2C_ReceiveAck();
 
    MyI2C_Start();
    MyI2C_SendByte(I2C_Slave_Addr | 0x01);
    MyI2C_ReceiveAck();
    for (i = 0; i < len; i++)
    {
        Buffer[i] = MyI2C_ReceiveByte();
        if (i == len - 1)
        {
            MyI2C_SendAck(1);
        }
        else
        {
            MyI2C_SendAck(0);
        }
    }
 
    MyI2C_Stop();
}

#endif
