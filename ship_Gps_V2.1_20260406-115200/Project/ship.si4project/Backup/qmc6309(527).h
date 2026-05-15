#ifndef __QMC6309_H
#define __QMC6309_H

#include "config.h"

//#define QMC6309_RECOVER

#define QMC6309_OK			1
#define QMC6309_FAIL		0

#define QMC6309_IIC_ADDR				0xf8
#define QMC6309H_IIC_ADDR				0x0c

//#define QMC6309H_VDD_MODE1			// 1:vio = vdd  2:vio=1.8 vdd < 2.1
//#define QMC6309H_VDD_MODE2			// vio = 1.8 vdd >= 2.1

/* chip id*/
#define QMC6309_CHIP_ID					0x90

/*data output register*/
#define QMC6309_CHIP_ID_REG				0x00
#define QMC6309_DATA_OUT_X_LSB_REG		0x01
#define QMC6309_DATA_OUT_X_MSB_REG		0x02
#define QMC6309_DATA_OUT_Y_LSB_REG		0x03
#define QMC6309_DATA_OUT_Y_MSB_REG		0x04
#define QMC6309_DATA_OUT_Z_LSB_REG		0x05
#define QMC6309_DATA_OUT_Z_MSB_REG		0x06
#define QMC6309_TEMP_OUT_LSB_REG		0x07
#define QMC6309_TEMP_OUT_MSB_REG		0x08

#define QMC6309_DATA_OUT_ST_X			0x13

/*Status registers */
#define QMC6309_STATUS_REG				0x09
/* configuration registers */
#define QMC6309_CTL_REG_ONE				0x0A  /* Contrl register one */
#define QMC6309_CTL_REG_TWO				0x0B  /* Contrl register two */
#define QMC6309_CTL_REG_THREE			0x0D  /* Contrl register three */
#define QMC6309_CTL_IBI					0x21

#define QMC6309_FIFO_REG_STATUS			0x20
#define QMC6309_FIFO_REG_CTRL			0x2E
#define QMC6309_FIFO_REG_DATA			0x2F

#define QMC6309_DATA_OFFSET_X_LSB_REG	0x22

#define QMC6309_SET_RESET_ON 			0
#define QMC6309_SET_ON 					1
#define QMC6309_RESET_ON 				2
#define QMC6309_SET_RESET_OFF 			3

#define QMC6309_RNG_32G					0
#define QMC6309_RNG_16G					1
#define QMC6309_RNG_8G					2

#define QMC6309_MODE_SUSPEND			0
#define QMC6309_MODE_NORMAL				1
#define QMC6309_MODE_SINGLE				2
#define QMC6309_MODE_HPFM				3

#define QMC6309_ODR_HPFM				0
#define QMC6309_ODR_1HZ					0
#define QMC6309_ODR_10HZ				1
#define QMC6309_ODR_50HZ				2
#define QMC6309_ODR_100HZ				3
#define QMC6309_ODR_200HZ				4

// CIC filter OSR: 512/256/128/64(0~3)
#define QMC6309_OSR1_8					0
#define QMC6309_OSR1_4					1
#define QMC6309_OSR1_2					2
#define QMC6309_OSR1_1					3

// Moving average depth normal mode the depth:16/8/4/2/1(7~4/3/2/1/0). Single Mode also use this filter.	
#define QMC6309_OSR2_1					0
#define QMC6309_OSR2_2					1
#define QMC6309_OSR2_4					2
#define QMC6309_OSR2_8					3
#define QMC6309_OSR2_16					4
//#define QMC6309_OSR2_64				6
//#define QMC6309_OSR2_128				7

//ZDBL_ENB	0: OSR1_Z=2*OSR_XY   1:OSR1_Z=OSR_XY
#define QMC6309H_ZDBL_ENB_ON			1
#define QMC6309H_ZDBL_ENB_OFF			0

#define QMC6309_ZDBL_ENB_ON				0
#define QMC6309_ZDBL_ENB_OFF			1

#define QMC6309_STATUS_DRDY				0x01
#define QMC6309_STATUS_OVFL				0x02

#define QMC6309_FIFO_STATUS_OR			0x04
#define QMC6309_FIFO_STATUS_WMK			0x02
#define QMC6309_FIFO_STATUS_FULL		0x01

#define QMC6309_SELFTEST_MAX_X			(50)
#define QMC6309_SELFTEST_MIN_X			(1)
#define QMC6309_SELFTEST_MAX_Y			(50)
#define QMC6309_SELFTEST_MIN_Y			(1)
#define QMC6309_SELFTEST_MAX_Z			(50)
#define QMC6309_SELFTEST_MIN_Z			(1)

#define QMC6309_ABS(X) 					((X) < 0 ? (-1 * (X)) : (X))
#define QMC6309_ABSF(X) 				((X) < 0.0f ? (-1.0 * (X)) : (X))

#define QMC6309_LOG		printf
#define QMC6309_CHECK_ERR(ret)	do {\
								if((ret) != QMC6309_OK)	\
								QMC6309_LOG("qmc6309 error:%d line:%d\r\n",ret, __LINE__);	\
									}while(0)

//#define QMC6309_MODE_SWITCH

enum
{
	TYPE_UNKNOW,
	TYPE_QMC6309,
	TYPE_QMC6309H,

	TYPE_MAX
};

typedef enum
{
	QMC6309_FIFO_MODE_BYPASS = (0<<6),
	QMC6309_FIFO_MODE_FIFO = (1<<6),
	QMC6309_FIFO_MODE_STREAM = (2<<6),
	QMC6309_FIFO_MODE_DEFAULT = (3<<6)
} qmc6309_fifo_mode;

typedef enum
{
	QMC6309_FIFO_CH_X = 0x01,
	QMC6309_FIFO_CH_Y = 0x02,
	QMC6309_FIFO_CH_Z = 0x04,
	QMC6309_FIFO_CH_ALL = 0x07
} qmc6309_fifo_ch;

typedef enum
{
	QMC6309_IBI_OFF = 0x00,
	QMC6309_IBI_DRDY = 0x01,
	QMC6309_IBI_OVFL = 0x02,
	QMC6309_IBI_ST_RDY = 0x04,
	QMC6309_IBI_FIFO_FULL = 0x08,
	QMC6309_IBI_FIFO_WMK = 0x10,
	QMC6309_IBI_EN = 0x20
} qmc6309_fifo_ibi;

typedef struct
{
	char			mode;
	unsigned short	count;
} qmc6309_set_ctl_t;

typedef union
{
	struct
	{
		unsigned char mode:2;
		unsigned char zdbl_enb:1;
		unsigned char osr1:2;
		unsigned char osr2:3;
	}bit_reg;
	unsigned char value;
} qmc6309_ctrlreg1;

typedef union
{
	struct
	{
		unsigned char set_rst:2;
		unsigned char range:2;
		unsigned char odr:3;
		unsigned char soft_rst:1;
	}bit_reg;
	unsigned char value;
} qmc6309_ctrlreg2;

typedef struct
{
	unsigned char			slave_addr;
	unsigned char			chip_type;
	unsigned short			ssvt;
	short					last_data[3];
	qmc6309_ctrlreg1		ctrl1;
	qmc6309_ctrlreg2		ctrl2;
	
} qmc6309_data_t;


int8_t qmc6309_init(void);

//int qmc6309_read_mag_xyz(float uT[3], short raw[3]);
int8_t qmc6309_read_mag_raw(short raw[3]);

#endif

