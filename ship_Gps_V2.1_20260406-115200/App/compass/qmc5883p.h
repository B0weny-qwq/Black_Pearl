#ifndef _QMC5883P_H_
#define _QMC5883P_H_

#include "config.h"


/* vendor chip id*/
#define QMC5883P_IIC_ADDR							(0x58)//(0001 1100 << 1) == (0011 1000) == 0x38

#define QMC5883P_CHIP_ID_REG					0x00

/*data output register*/
#define QMC5883P_DATA_OUT_X_LSB_REG		0x01
#define QMC5883P_DATA_OUT_X_MSB_REG		0x02
#define QMC5883P_DATA_OUT_Y_LSB_REG		0x03
#define QMC5883P_DATA_OUT_Y_MSB_REG		0x04
#define QMC5883P_DATA_OUT_Z_LSB_REG		0x05
#define QMC5883P_DATA_OUT_Z_MSB_REG		0x06
/*Status registers */
#define QMC5883P_STATUS_REG						0x09
/* configuration registers */
#define QMC5883P_CTL_REG_ONE					0x0A  /* Contrl register one */
#define QMC5883P_CTL_REG_TWO					0x0B  /* Contrl register two */

/* Magnetic Sensor Operating Mode MODE[1:0]*/
#define QMC5883P_SUSPEND_MODE					0x00
#define QMC5883P_NORMAL_MODE					0x01
#define QMC5883P_SINGLE_MODE					0x02
#define QMC5883P_H_PFM_MODE						0x03

/*data output rate OSR2[2:0]*/
#define OUTPUT_DATA_RATE_800HZ 				0x00
#define OUTPUT_DATA_RATE_400HZ 				0x01
#define OUTPUT_DATA_RATE_200HZ 				0x02
#define OUTPUT_DATA_RATE_100HZ 				0x03

/*oversample Ratio  OSR[1]*/
#define OVERSAMPLE_RATE_256						0x01
#define OVERSAMPLE_RATE_128 					0x00

#define SET_RESET_ON 									0x00
#define SET_ONLY_ON 									0x01
#define SET_RESET_OFF 								0x02

#define QMC5883P_DEFAULT_DELAY				200



int8_t qmc5883p_init(void);
int8_t qmc5883p_read_mag_xyz(short *raw_data);

#endif

