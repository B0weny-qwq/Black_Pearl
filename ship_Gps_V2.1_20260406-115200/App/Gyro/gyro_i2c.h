#ifndef _GYRO_I2C_H_
#define _GYRO_I2C_H_

    #include "config.h"
    #include "STC8G_H_Soft_I2C.h"
    #include "STC8G_H_Delay.h"
    #include "STC8G_H_GPIO.h"

    void MyI2C_Init(void);
    void MyI2C_SDA_OutPP(uint8_t onoff);
    void MyI2C_SendBuffer(uint8_t I2C_Slave_Addr, uint8_t AR_Addr,uint8_t *Buffer, uint32_t len);
    void MyI2C_RecvBuffer(uint8_t I2C_Slave_Addr, uint8_t AR_Addr, uint8_t *Buffer, uint32_t len);
    void MyI2C_Send_Byte(uint8_t I2C_Slave_Addr, uint8_t AR_Addr,uint8_t data_m);
#endif
