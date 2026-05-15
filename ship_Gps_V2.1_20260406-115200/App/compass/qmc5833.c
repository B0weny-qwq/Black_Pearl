#include "qmc5833.h"
#include "STC8G_H_Soft_I2C.h"

#ifdef		MAG_QMC_5883
int8_t qmc5833_i2c_read(uint8_t addr, uint8_t *data_m, uint8_t len)
{
    SI2C_ReadNbyte(QMC_5833_SLAW,addr,data_m,len);
    return 0;
}

int8_t qmc5833_i2c_write(uint8_t addr, uint8_t data_m)
{   
    SI2C_WriteNbyte(QMC_5833_SLAW,addr,(uint8_t *)&data_m,1);
    return 0;
}

void qmc5833_Init(void){
 	qmc5833_i2c_write(0x09,0x1d);  //控制寄存器配置
	qmc5833_i2c_write(0x0b,0x01);  //设置清除时间寄存器
	qmc5833_i2c_write(0x20,0x40);  //
	qmc5833_i2c_write(0x21,0x01);  //	   
}

void qmc5833_Read_XYZ(int16_t *xyzdata)
{   
    uint8_t tmp_data[6] = {0};
    qmc5833_i2c_read(0x00, (uint8_t *)&tmp_data, 6);
    xyzdata[0] = tmp_data[1] << 8 | tmp_data[0]; //Combine MSB and LSB of X Data output register 
    xyzdata[1] = tmp_data[3] << 8 | tmp_data[2]; //Combine MSB and LSB of Y Data output register
    xyzdata[2] = tmp_data[5] << 8 | tmp_data[4]; //Combine MSB and LSB of Z Data output register
    
    if(xyzdata[0]>0x7fff)xyzdata[0]-=0xffff;	  
    if(xyzdata[1]>0x7fff)xyzdata[1]-=0xffff;
    if(xyzdata[2]>0x7fff)xyzdata[2]-=0xffff;	 

}

#endif