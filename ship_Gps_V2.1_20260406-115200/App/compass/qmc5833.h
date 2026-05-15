#ifndef _QMC_5833_H_
#define _QMC_5833_H_
    #include "config.h"
    #define QMC_5833_SLAW    0x1A
    #define QMC_5833_SLAR    0x1B

    int8_t qmc5833_i2c_read(uint8_t addr, uint8_t *data_m, uint8_t len);
    int8_t qmc5833_i2c_write(uint8_t addr, uint8_t data_m);
    void qmc5833_Init(void);
    void qmc5833_Read_XYZ(int16_t *xyzdata);
#endif
