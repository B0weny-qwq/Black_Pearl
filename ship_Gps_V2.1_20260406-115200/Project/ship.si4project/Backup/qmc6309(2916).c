
#include "qmc6309.h"
#include "STC8G_H_Soft_I2C.h"
#include "timer.h"

#ifdef		MAG_QMC_6309

qmc6309_data_t idata p_mag;

unsigned char code mag_slave[] = {QMC6309_IIC_ADDR,QMC6309H_IIC_ADDR};
//static const unsigned char mag_slave[] = {QMC6309H_IIC_ADDR};

int8_t qmc6309_read_block(uint8_t addr, uint8_t *data_m, uint8_t len)
{
    SI2C_ReadNbyte(p_mag.slave_addr,addr,data_m,len);
    return QMC6309_OK;
}

int8_t qmc6309_write_reg(uint8_t addr, uint8_t data_m)
{   
    SI2C_WriteNbyte(p_mag.slave_addr,addr,(uint8_t *)&data_m,1);
    return QMC6309_OK;
}


void qmc6309_delay(unsigned int ms_cnt)
{
	//extern void qst_delay_ms(unsigned int n_ms);

	SysTimer_delayms(ms_cnt);
}

#if 0
void qmc6309_get_chip_info(unsigned int *info)
{
	unsigned char verid = 0;
	unsigned char ctrl_value[4];
	
	qmc6309_read_block(0x37, ctrl_value, 3);
	qmc6309_read_block(0x12, &verid, 1);
	info[0] = (unsigned int)(((unsigned int)0x90<<24)|((unsigned int)ctrl_value[0]<<16)|((unsigned int)ctrl_value[2]<<8)|((unsigned int)ctrl_value[1]));
	info[1] = verid;
}
#endif

int8_t qmc6309_get_chipid(void)
{
	int8_t ret = QMC6309_FAIL;
	int retry=0;
	unsigned char chip_id = 0x00;

	retry = 0;
	
	ret = qmc6309_read_block(QMC6309_CHIP_ID_REG, &chip_id, 1);
	
	if((chip_id == QMC6309_CHIP_ID)||(chip_id == 0xa5))
	{
		QMC6309_LOG("mag2 chipid ok !!!\r\n");
		return 1;
	}
	else
	{
		QMC6309_LOG("mag2 chipid fail !!!\r\n");
		return 0;
	}
}

#if 0
void qmc6309_set_range(unsigned char range)
{
	QMC6309_LOG("qmc6309_set_range 0x%x\r\n", range);
	p_mag.ctrl2.bit_reg.range = range;

	switch(p_mag.ctrl2.bit_reg.range)
	{
		case QMC6309_RNG_32G:
			p_mag.ssvt = 1000;
			break;
		case QMC6309_RNG_16G:
			p_mag.ssvt = 2000;
			break;
		case QMC6309_RNG_8G:
			p_mag.ssvt = 4000;
			break;
		default:
			p_mag.ssvt = 1000;
			break;
	}
}
#endif

int8_t qmc6309_enable(void)
{
	int8_t ret = 0;
	int i;
	unsigned char ctrl_data[2];
	
	QMC6309_LOG("mag2 enable!\r\n");
	
	//QMC6309_LOG("QMC6309  init!  ctrl2=0x%bx  ctrl1=0x%bx\r\n", p_mag.ctrl2.value,p_mag.ctrl1.value);
	
	qmc6309_delay(10);
	
	for (i = 0;i < 10;i ++)
	{	ret = qmc6309_write_reg(QMC6309_CTL_REG_TWO, p_mag.ctrl2.value);
		
		qmc6309_delay(2);
		ret = qmc6309_write_reg(QMC6309_CTL_REG_ONE, p_mag.ctrl1.value);
		qmc6309_delay(2);

		//qmc6309_get_chipid();
		
		qmc6309_read_block(QMC6309_CTL_REG_ONE, ctrl_data, 2);

		qmc6309_read_block(QMC6309_CTL_REG_TWO, &ctrl_data[1], 1);
		
		QMC6309_LOG("ctrlreg [0x0a=0x%bx 0x0b=0x%bx] \r\n", ctrl_data[0], ctrl_data[1]);
		if ((p_mag.ctrl1.value == ctrl_data[0]) && (p_mag.ctrl2.value == ctrl_data[1]))
		{	
			QMC6309_LOG("mag2 enable succ!!!\r\n");
			break;
		}
		else
			QMC6309_LOG("mag2 enable fail !!!\r\n");
	}
	
	return ret;
}

#if 1
void qmc6309_disable(void)
{
	QMC6309_LOG("mag2 disable!\r\n");
	
	qmc6309_write_reg(QMC6309_CTL_REG_ONE, 0x00);

}
#endif


void qmc6309_init_para(unsigned char mode, unsigned char odr)
{
	p_mag.ctrl1.bit_reg.mode = mode;	// QMC6309_MODE_HPFM; QMC6309_MODE_NORMAL
	p_mag.ctrl1.bit_reg.osr1 = QMC6309_OSR1_8;
	p_mag.ctrl1.bit_reg.osr2 = QMC6309_OSR2_4;
	if(p_mag.chip_type == TYPE_QMC6309)	
		p_mag.ctrl1.bit_reg.zdbl_enb = QMC6309_ZDBL_ENB_ON;
	else
		p_mag.ctrl1.bit_reg.zdbl_enb = QMC6309H_ZDBL_ENB_ON;		

	p_mag.ctrl2.bit_reg.set_rst = QMC6309_SET_RESET_ON;		// QMC6309_SET_ON, QMC6309_SET_RESET_ON	QMC6309_SET_RESET_OFF
	p_mag.ctrl2.bit_reg.range = QMC6309_RNG_32G;
	p_mag.ctrl2.bit_reg.odr = odr;
	p_mag.ctrl2.bit_reg.soft_rst = 0;

	//qmc6309_set_range(p_mag.ctrl2.bit_reg.range);

}

#if 0
void qmc6309_reload_otp(void)
{
	int8_t ret = 0;
	unsigned char status = 0;
	int retry = 0;
	int count = 0;

	QMC6309_LOG("qmc6309_reload_otp\r\n");
	while(retry++ < 20)
	{
		ret = qmc6309_write_reg(0x28, 0x02);
		qmc6309_delay(2);
		if(ret != QMC6309_OK)
		{
			QMC6309_LOG("write 0x28 = 0x02 fail!\r\n");
		}
		qmc6309_delay(2);
		count = 0;
		while(count++<100)
		{
			qmc6309_delay(1);
			status = 0;
			ret = qmc6309_read_block(QMC6309_STATUS_REG, &status, 1);
			if((ret==QMC6309_OK)&&(status & 0x10))
			{
				QMC6309_LOG("qmc6309_reload_otp done slave=0x%bx status=0x%bx\r\n", p_mag.slave_addr, status);
				//qmc6309_dump_reg();				
				return;
			}
		}
	}
	
	QMC6309_LOG("qmc6309_reload_otp fail\r\n");
}


void qmc6309_check_otp(void)
{
	int8_t ret = QMC6309_FAIL;
	int retry = 0;
	unsigned char status = 0x00;
	int count=10;

	while(count > 0)
	{
		count--;
		retry = 0;
		while(retry++<5)
		{
			ret = qmc6309_read_block(QMC6309_STATUS_REG, &status, 1);
			QMC6309_CHECK_ERR(ret);
			QMC6309_LOG("qmc6309 status 0x%bx\r\n", status);
			if(status & 0x10)
			{
				QMC6309_LOG("qmc6309 NVM load done!\r\n");
				return;
			}
			qmc6309_delay(1);
		}

		if(!(status & 0x10))
		{
			qmc6309_reload_otp();
		}
		else
		{
			return;
		}
	}
}

#endif

void qmc6309_soft_reset(void)
{
	int ret = QMC6309_FAIL;
	int retry = 0;
	unsigned char status = 0x00;

	QMC6309_LOG("mag2 soft_reset!\r\n");
	ret = qmc6309_write_reg(QMC6309_CTL_REG_TWO, 0x80);
	qmc6309_delay(5);
	ret = qmc6309_write_reg(QMC6309_CTL_REG_TWO, 0x00);
	qmc6309_delay(50);

	while(retry++<5)
	{
		ret = qmc6309_read_block(QMC6309_STATUS_REG, &status, 1);
		QMC6309_CHECK_ERR(ret);
		//QMC6309_LOG("qmc6309 status 0x%bx\r\n", status);
		if((status & 0x10)&&(status & 0x08))
		{
			QMC6309_LOG("mag2 NVM load done!\r\n");
			break;
		}
		qmc6309_delay(1);
	}
}

unsigned char get_mag_tick = 0;

int8_t qmc6309_read_mag_raw(short *raw)
{
	int8_t res = QMC6309_FAIL;
	unsigned char mag_data[6];
	
	#if 0
	unsigned char rdy = 0;
	unsigned char t1 = 0;
	/* Check status register for data availability */
	res = qmc6309_read_block(QMC6309_STATUS_REG, &rdy, 1);
	while(!(rdy & (QMC6309_STATUS_DRDY|QMC6309_STATUS_OVFL)) & (t1++ < 5))
	{
		res = qmc6309_read_block(QMC6309_STATUS_REG, &rdy, 1);
		QMC6309_CHECK_ERR(res);
		qmc6309_delay(1);
	}
	if((res == QMC6309_FAIL)||(!(rdy & QMC6309_STATUS_DRDY)))
  	{
		raw[0] = p_mag.last_data[0];
		raw[1] = p_mag.last_data[1];
		raw[2] = p_mag.last_data[2];
		QMC6309_LOG("qmc6309_read_mag_raw read drdy fail! res=%bd rdy=0x%bx\r\n",res,rdy);
		if(qmc6309_fail_num++ > 10)
		{

			qmc6309_fail_num = 0;
		}
		res = QMC6309_OK;	// QMC6309_FAIL;
	}
	else if(rdy & QMC6309_STATUS_OVFL)
	{
		raw[0] = 32767;
		raw[1] = 32767;
		raw[2] = 32767;
	}
	else
#endif
	
	{
		
		mag_data[0] = QMC6309_DATA_OUT_X_LSB_REG;
		res = qmc6309_read_block(QMC6309_DATA_OUT_X_LSB_REG, mag_data, 6);
		if(res == QMC6309_FAIL)
	  	{
			QMC6309_LOG("mag2 read data fail! res=%d\r\n",res);
			raw[0] = p_mag.last_data[0];
			raw[1] = p_mag.last_data[1];
			raw[2] = p_mag.last_data[2];
			res = QMC6309_OK;	// QMC6309_FAIL;
		}
		else
		{
			raw[0] = (short)(((mag_data[1]) << 8) | mag_data[0]);
			raw[1] = (short)(((mag_data[3]) << 8) | mag_data[2]);
			raw[2] = (short)(((mag_data[5]) << 8) | mag_data[4]);
			//printf("mag raw data :%x %x %x \r\n", raw[0], raw[1], raw[2]);
		}
	}
	
	p_mag.last_data[0] = raw[0];
	p_mag.last_data[1] = raw[1];
	p_mag.last_data[2] = raw[2];	
	return res;
}

#if 0
int8_t qmc6309_read_mag_xyz(float uT[3], short raw[3])
{
	int8_t res = QMC6309_FAIL;

	res = qmc6309_read_mag_raw(raw);
	if(res == QMC6309_OK)
	{
		uT[0] = (float)((float)raw[0] / ((float)p_mag.ssvt/100.f));		// ut
		uT[1] = (float)((float)raw[1] / ((float)p_mag.ssvt/100.f));		// ut
		uT[2] = (float)((float)raw[2] / ((float)p_mag.ssvt/100.f));		// ut
	}
	else
	{
		uT[0] = uT[1]= uT[2] = 0.0f;
	}

	if(p_mag.ctrl1.bit_reg.mode == QMC6309_MODE_SINGLE)
	{
		res = qmc6309_write_reg(QMC6309_CTL_REG_ONE, p_mag.ctrl1.value);
		QMC6309_CHECK_ERR(res);
	}

	return res;
}
#endif


void qmc6309_dump_reg(void)
{
	unsigned char ctrl_value[4];
	unsigned char version_id;
	unsigned char wafer_id;
	unsigned char die_id[2];

	QMC6309_LOG("mag2 dump_reg\r\n");
	qmc6309_read_block(0x12, &version_id, 1);
	qmc6309_read_block(0x37, &wafer_id, 1);
	qmc6309_read_block(0x38, die_id, 2);
	QMC6309_LOG("version id:0x%bx wafer id:0x%bx die id:0x%x \r\n", version_id, wafer_id, (die_id[1]<<8)|die_id[0]);
	qmc6309_read_block(QMC6309_CTL_REG_ONE, ctrl_value, 2);
	QMC6309_LOG("ctrlreg [0x0a=0x%bx 0x0b=0x%bx] \r\n", ctrl_value[0], ctrl_value[1]);
	qmc6309_read_block(QMC6309_FIFO_REG_CTRL, ctrl_value, 1);
	QMC6309_LOG("fifo-ctrl 0x%bx=0x%bx \r\n", QMC6309_FIFO_REG_CTRL, ctrl_value[0]);
	qmc6309_read_block(0x40, ctrl_value, 1);
	QMC6309_LOG("0x40 = 0x%bx \r\n", ctrl_value[0]);
}

int8_t qmc6309_init(void)
{
	int8_t ret = 0;
	int i = 0;

	p_mag.chip_type = TYPE_UNKNOW;
#if 0
	p_mag.slave_addr = 1;
	for (i = 0;i < 250;i ++)
	{	
		ret = qmc6309_get_chipid();
		if (ret == 1)
			break;
		p_mag.slave_addr ++;
	}
#endif
	
	for(i=0; i<sizeof(mag_slave)/sizeof(mag_slave[0]); i++)
	{
		p_mag.slave_addr = mag_slave[i];
		ret = qmc6309_get_chipid();
		if(ret)
		{
			if(p_mag.slave_addr == QMC6309_IIC_ADDR)
			{
				p_mag.chip_type = TYPE_QMC6309;
				
			}
			else if(p_mag.slave_addr == QMC6309H_IIC_ADDR)
			{
				p_mag.chip_type = TYPE_QMC6309H;
			}
			else
			{
				p_mag.chip_type = TYPE_UNKNOW;
			}
			break;
		}
	}
	//p_mag.chip_type = TYPE_QMC6309;
	if(p_mag.chip_type != TYPE_UNKNOW)
	{
		qmc6309_soft_reset();

		qmc6309_get_chipid();
		
		//qmc6309_reload_otp();	// reload otp
		//qmc6309_check_otp();

		qmc6309_disable();
		
		qmc6309_init_para(QMC6309_MODE_HPFM, QMC6309_ODR_100HZ);
		//ret = qmc6309_disable();
		
		qmc6309_enable();
		QMC6309_LOG("mag2 init OK!\r\n");
		//qmc6309_dump_reg();

		
		return 1;
	}
	return 0;
}

#endif

