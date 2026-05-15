#ifndef _GYRO_H_
#define _GYRO_H_
    #include <math.h>
    #include "config.h"
    #include "STC8G_H_Soft_I2C.h"
    #include "STC8G_H_Delay.h"
    #include "gyro_i2c.h"

    #define SENSITIVITY 1024
    #define OFFSET_THRESHOLD 400*SENSITIVITY/1000	//1G=1000mg=1024lsb, 400mg

   
    /*******************************************************************************
    Macro definitions - Register define for Gsensor
    ********************************************************************************/
    #define REG_SPI_CONFIG              0x00
    #define REG_CHIP_ID                 0x01
    #define REG_ACC_X_LSB               0x02
    #define REG_ACC_X_MSB               0x03
    #define REG_ACC_Y_LSB               0x04
    #define REG_ACC_Y_MSB               0x05
    #define REG_ACC_Z_LSB               0x06
    #define REG_ACC_Z_MSB               0x07
    #define REG_MOTION_FLAG             0x09
    #define REG_NEWDATA_FLAG            0x0A
    #define REG_ACTIVE_STATUS           0x0B
    #define REG_RESOLUTION_RANGE        0x0F
    #define REG_ODR_AXIS                0x10
    #define REG_MODE_AXIS               0x11
    #define REG_SWAP_POLARITY           0x12
    #define REG_INT_SET1                0x16
    #define REG_INT_SET2                0x17
    #define REG_INT_MAP1                0x19
    #define REG_INT_MAP2                0x1A
    #define REG_INT_CONFIG              0x20
    #define REG_INT_LATCH               0x21
    #define REG_ACTIVE_DUR              0x27
    #define REG_ACTIVE_THS              0x28

    int8_t mir3da_init(void);
    void mir3da_read_raw_data(short *x, short *y, short *z);
    void Gyro_Read_Handle(void);
    void do_difff_calibrate(void);
#endif
