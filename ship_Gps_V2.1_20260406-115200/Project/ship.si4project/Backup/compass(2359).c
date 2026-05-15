#include "compass.h"
#include "qmc5833.h"
#include "pwm_Speed.h"
#include "utils_printf.h"
#include "pwm.h"
#include "pwm_Speed.h"
#include <math.h>
#include "config.h"
#include "qmc5883p.h"
#include "qmc6309.h"
#include "eeprom.h"

int16_t idata compassXYZData[3] = {0};

int16_t xdata magX_min,magX_max;
int16_t xdata magY_min,magY_max;


int16_t idata g_magx_offset = 0;      /* x轴补偿值 */
int16_t idata g_magy_offset = 0;      /* y轴补偿值 */
bit s_in_calib_state = 0;

uint16_t mag_calib_time;

void Compass_Init(void){
	#ifdef	MAG_QMC_5883P
	qmc5883p_init();
	#elif defined(MAG_QMC_6309)
	qmc6309_init();
	#else
    qmc5833_Init();
	#endif
	magX_min = 0;
	magX_max = 0;
	magY_min = 0;
	magY_max = 0;
	g_magx_offset = cfg_get_x_offset();
	g_magy_offset = cfg_get_y_offset();
	s_in_calib_state = 0;
	mag_calib_time = 0;
}


#if 0
int16_t Compass_Get_Azimuth(void){

	double angel = atan2( compassXYZData[1], compassXYZData[0] ) * 572.9578;
	angel = angel < 0 ? 3600.0 + angel : angel;
	//printf("compass angel=%f\r\n",angel);
	//return (int16_t)(angel * 10);
	return (int16_t)angel;
}
#endif

#if 0
int16_t Compass_Get_Azimuth(void){

	double angel = atan2( compassXYZData[1], compassXYZData[0] ) * 57.29578;
	angel = angel < 0 ? 360.0 + angel : angel;
	//printf("compass angel=%f\r\n",angel);
	//return (int16_t)(angel * 10);
	return (int16_t)angel;
}
#endif

float compass_get_angle(void)  
{
    float angle;
    int16_t magx, magy;
        
    magx = (compassXYZData[0] - g_magx_offset) ;     /* 根据校准参数, 计算新的输出 */
    magy = (compassXYZData[1] - g_magy_offset) ;     /* 根据校准参数, 计算新的输出 */

    /* 根据不同的象限情况, 进行方位角换算 */
    if ((magx > 0) && (magy > 0))
    {
        angle = (atan((double)magy / magx) * 57.29578);
    }
    else if ((magx > 0) && (magy < 0))
    {
        angle = 360 + (atan((double)magy / magx) * 57.29578);
    }
    else if ((magx == 0) && (magy > 0))
    {
        angle = 90;
    }
    else if ((magx == 0) && (magy < 0))
    {
        angle = 270;
    }
    else if (magx < 0)
    {
        angle = 180 + (atan((double)magy / magx) * 57.29578);
    }
    
    if (angle > 360) angle = 360; /* 限定方位角范围 */
    if (angle < 0) angle = 0;     /* 限定方位角范围 */

    return (int16_t)(angle*10);
}


void Compass_Read_XYZ(void){
	#ifdef	MAG_QMC_5883P
	qmc5883p_read_mag_xyz((int16_t *)compassXYZData);
	#elif defined(MAG_QMC_6309)
	qmc6309_read_mag_raw((int16_t *)compassXYZData);
	#else
	qmc5833_Read_XYZ((int16_t *)compassXYZData);
	#endif
	//printf("XYZ=%hd,%hd,%hd\r\n",compassXYZData[0],compassXYZData[1],compassXYZData[2]);
}


void Compass_calib_OK(void)
{	
	g_magx_offset = (magX_min + magX_max) / 2;    /* X轴偏移量 */
    g_magy_offset = (magY_max + magY_min) / 2;    /* Y轴偏移量 */
	System_Config_Save(g_magx_offset,g_magy_offset);

	printf("MAG calib  x_offset=%d,y_offset=%d\r\n",g_magx_offset,g_magy_offset);
}

void Compass_calib_handle(void){

	//qmc5833_Read_XYZ((int16_t *)&compassXYZData);
	if (s_in_calib_state)
	{	
		magX_max = magX_max < compassXYZData[0] ? compassXYZData[0] : magX_max;            /* 记录x最大值 */
	    magX_min = magX_min > compassXYZData[0] ? compassXYZData[0] : magX_min;            /* 记录x最小值 */
	    magY_max = magY_max < compassXYZData[1] ? compassXYZData[1] : magY_max;            /* 记录y最大值 */
	    magY_min = magY_min > compassXYZData[1] ? compassXYZData[1] : magY_min;            /* 记录y最小值 */
		if (mag_calib_time)
			mag_calib_time --;
		else
		{	// 退出校准模式
			s_in_calib_state = 0;
			Compass_calib_OK();

			DcMotor_Direction_Set(DIRECTION_FORWARD);
            DcMotor_MRight_PWM_Set(FALSE,0);
            DcMotor_MLeft_PWM_Set(FALSE,0);
            nowPwmAccelerator_Set(0);
		}
		
	}
	
	// printf("XYZ=%hd,%hd,%hd\r\n",compassXYZData[0],compassXYZData[1],compassXYZData[2]);
}

void Compass_calib_start(void)
{	
	s_in_calib_state = 1;

	magX_min = 0;
	magX_max = 0;
	magY_min = 0;
	magY_max = 0;
	
	mag_calib_time = 500;
	DcMotor_Direction_Set(DIRECTION_RIGHT);
	DcMotor_MRight_PWM_Set(TRUE,80);
	DcMotor_MLeft_PWM_Set(TRUE,80);
	
}

