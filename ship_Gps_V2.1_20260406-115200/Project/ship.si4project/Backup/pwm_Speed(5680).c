#include "pwm_Speed.h"
#include "pwm.h"
#include "wirelessProtocal.h"
#include "compass.h"
#include "qmc5833.h"
#include "timer.h"
#include "eeprom.h"


PWM_DUAL_MOTOR_TURN  idata pwmTurnAdjust = {0};

uint8_t idata nowPwmAccelerator = 0;
uint8_t idata correct_speed_gain;
uint8_t idata adjust_angel_offet;			//调整的角度

int16_t idata nowAveAngel =  0,startAveAngel = 0;

uint16_t idata AdjustAngelTimes = 0;
uint8_t idata average_angel_cnt = 0;
uint8_t idata adjust_ok_cnt = 0;
uint8_t idata register_angel_delay = 0;


int16_t idata angel_cache[3];
int16_t idata last_angel_offset = 0;
uint16_t xdata correct_speed_tick;

bit s_angel_is_left = 0;
bit s_angel_dir_last = 0;
bit s_angel_offset_chng;
bit s_angel_first_check;

extern uint8_t idata lastDirection;

#ifdef		MID_SHIP
#define			RIGHT_DEFAULT_GAIN			27
#define			LEFT_DEFAULT_GAIN			30
#define			SPEED_GAIN_MIN				16

//uint8_t code ANGEL_OFFSET_TAB[] = {7,13,18,23,28};
uint8_t code ANGEL_OFFSET_TAB[] = {3,4,6,7,8};

#elif		defined(BIG_SHIP)
#define			RIGHT_DEFAULT_GAIN			27
#define			LEFT_DEFAULT_GAIN			30
#define			SPEED_GAIN_MIN				16

uint8_t code ANGEL_OFFSET_TAB[] = {3,4,6,9,10};

#else
#define			RIGHT_DEFAULT_GAIN			21
#define			LEFT_DEFAULT_GAIN			24

#define			SPEED_GAIN_MIN				12
uint8_t code ANGEL_OFFSET_TAB[] = {3,4,6,9,10};
#endif

#define			LEFT_DIVIDE_MAX				35

#define			LEFT_DIVIDE_NORMAL			30


int16_t Compass_Get_Azimuth(void);


void Pwm_Speed_Init(void){
    
    pwmTurnAdjust.turnTimes = 0;
	pwmTurnAdjust.is_left = 0;
	pwmTurnAdjust.speed_gain = RIGHT_DEFAULT_GAIN;
	pwmTurnAdjust.speed_divid = LEFT_DIVIDE_NORMAL;
	correct_speed_gain = 0;
	correct_speed_tick = 0;
	register_angel_delay = 0;
	s_angel_offset_chng = 0;
}


void nowPwmAccelerator_Set(uint8_t accelerator){
    nowPwmAccelerator = accelerator;
	if (nowPwmAccelerator > 100)
		nowPwmAccelerator = 100;
}

uint8_t Pwm_Speed_Get_left(void){
	#if 0
    if(!s_angel_is_left){
        
        return nowPwmAccelerator;
    }else
    {
        uint16_t speed = nowPwmAccelerator;
        return (uint8_t)((speed*pwmTurnAdjust.speed_gain)/pwmTurnAdjust.speed_divid);
    }
	#else
	return nowPwmAccelerator;
	#endif
}

uint8_t Pwm_Speed_Get_right(void){
	#if 0
	if(!s_angel_is_left){
        uint16_t speed = nowPwmAccelerator;
        return (uint8_t)((speed*pwmTurnAdjust.speed_gain)/pwmTurnAdjust.speed_divid);
    }else
	{
        return nowPwmAccelerator;
    }
	#else
	uint16_t speed = nowPwmAccelerator;
    return (uint8_t)((speed*pwmTurnAdjust.speed_gain)/pwmTurnAdjust.speed_divid);
	#endif
}

void Pwm_adjust_reset(void){
    
	AdjustAngelTimes = 0;
	last_angel_offset = 0;
}

void comPass_cache_init_angel_delay(void)
{	
	int n;
	int16_t angel = 0;
	
	AdjustAngelTimes = 0;
	adjust_ok_cnt = 0;
	//pwmTurnAdjust.speed_gain = RIGHT_DEFAULT_GAIN;
	s_angel_dir_last = 0;
	s_angel_is_left = 0;
	average_angel_cnt = 0;

	SysTimer_delay10ms(50);
	
	for (n = 0;n < 3;n ++)
	{	
		SysTimer_delay10ms(2);
		
		Compass_Read_XYZ();
		angel = compass_get_angle();
		angel_cache[n] = angel;
	}
	nowAveAngel = (angel_cache[0] + angel_cache[1] + angel_cache[2])/3;
	startAveAngel = nowAveAngel;

	register_angel_delay = 0;
	adjust_angel_offet = 0;

	s_angel_first_check = 1;
	printf("register angel=%u\r\n",startAveAngel);
}

void comPass_cache_init_angel(void)
{	
	int n;
	int16_t angel = 0;
	
	AdjustAngelTimes = 0;
	adjust_ok_cnt = 0;

	SysTimer_delay10ms(15);
	
	for (n = 0;n < 3;n ++)
	{	
		SysTimer_delay10ms(2);
		
		Compass_Read_XYZ();
		angel = compass_get_angle();
		angel_cache[n] = angel;
	}
	nowAveAngel = (angel_cache[0] + angel_cache[1] + angel_cache[2])/3;
	
	if ((correct_speed_gain > SPEED_GAIN_MIN) && (correct_speed_gain < LEFT_DIVIDE_NORMAL))
		pwmTurnAdjust.speed_gain = correct_speed_gain;
	else
		pwmTurnAdjust.speed_gain = RIGHT_DEFAULT_GAIN;
	s_angel_dir_last = 0;
	s_angel_is_left = 0;
	average_angel_cnt = 0;
	
	startAveAngel = nowAveAngel;

	correct_speed_tick = 0;
	adjust_angel_offet = 0;
	s_angel_first_check = 1;
	//register_angel_delay = 25;
	printf("register angel=%u\r\n",startAveAngel);
}

void Compass_Angel_adjust_Handle(void){

	int16_t angel = 0;
	uint16_t angel_abs;
	uint8 gain_angel = 0;
	
	Compass_Read_XYZ();

	Compass_calib_handle();
	
	//angel = (int16_t)Compass_Get_Azimuth();
	angel = compass_get_angle();
	#if 1
	if (average_angel_cnt < 3)
	{	
		angel_cache[average_angel_cnt] = angel;
		average_angel_cnt ++;
	}
	if (average_angel_cnt >= 3)
	{	
		average_angel_cnt = 0;
		
		angel = (angel_cache[0] + angel_cache[1] + angel_cache[2])/3;
		angel_abs = abs(angel - nowAveAngel);
		if (angel_abs > 15)
		{	
			if (!s_angel_offset_chng)		// 第一次检测到 偏差大 只标记，不处理
			{	
				s_angel_offset_chng = 1;
				return ;
			}
			else	// 连续第二次偏差大  确认 为角度变化大
				s_angel_offset_chng = 0;
		}
		else
		{	
			s_angel_offset_chng = 0;
		}
		nowAveAngel = angel;
		
		//printf("cur angel=%d\r\n",nowAveAngel);
	}
	else
		return ;
	#endif
	#if 0
	if (register_angel_delay)
	{	register_angel_delay --;
		if (register_angel_delay == 0)
			startAveAngel = nowAveAngel;
	}
	#endif
	
    if((lastDirection == DIRECTION_FORWARD) && (nowPwmAccelerator >= 10))
	{
		angel = nowAveAngel - startAveAngel;
		angel = angel>0?angel:(angel + 3600);
		angel = angel>1800?(angel-3600):angel;

		if (!s_angel_first_check)
			angel = angel + ANGEL_OFFSET_TAB[nowPwmAccelerator/21];		// 电机运转 对地磁有干扰，纠正偏差
		
		s_angel_first_check = 0;
		
		angel_abs = abs(angel);

		gain_angel = angel_abs/10;

		
		if (AdjustAngelTimes != 0)
		{	
			AdjustAngelTimes --;

			if(angel > 15){				// 调整 过程中 已经反向偏航， 重新开始调整
				if (!s_angel_is_left)
				{	
					printf("L *** Dir change A=%d\r\n",angel);
					AdjustAngelTimes = 0;
					return ;
				}
			
			}else if(angel < -15){

				if (s_angel_is_left)
				{	
					printf("R *** Dir change A=%d\r\n",angel);
					AdjustAngelTimes = 0;
					return ;
				}
			}

			if (!s_angel_is_left)
			{	if (angel_abs > (last_angel_offset+8))			// 角度更大了，需要加大调整
				{	
					if (pwmTurnAdjust.speed_gain > 3)
					{	pwmTurnAdjust.speed_gain --;
						//printf("Left Sub speed=%bd\r\n",pwmTurnAdjust.speed_gain);
					}
				}
				else if (angel_abs < 20)
				{
					//  间隔8*3 = 240ms 调整一次
					if ((angel_abs > 12)  && ((AdjustAngelTimes &0x07) == 0) && (AdjustAngelTimes < 18))  // 角度偏差在缩小，减小偏差
					{	
						if ((pwmTurnAdjust.speed_gain < pwmTurnAdjust.speed_divid) && (adjust_angel_offet > 0))
						{	
							pwmTurnAdjust.speed_gain ++;
							adjust_angel_offet --;
							//printf("Left Add speed=%bd\r\n",pwmTurnAdjust.speed_gain);
						}
					}
				}
			}
			else
			{	
				if (angel_abs > (last_angel_offset+8))			// 角度更大了，需要加大调整
				{	
					if (pwmTurnAdjust.speed_gain < LEFT_DIVIDE_MAX)
					{	pwmTurnAdjust.speed_gain ++;
						//printf("Right Add speed=%bd\r\n",pwmTurnAdjust.speed_gain);
					}
				}
				else if (angel_abs < 20)
				{
					//  间隔8*3 = 240ms 调整一次
					if ((angel_abs > 12)  && ((AdjustAngelTimes &0x07) == 0) && (AdjustAngelTimes < 18))  // 角度偏差在缩小，减小偏差
					{	
						if ((pwmTurnAdjust.speed_gain > 8) && (adjust_angel_offet > 0))
						{	
							pwmTurnAdjust.speed_gain --;
							adjust_angel_offet --;
							//printf("Right Sub speed=%bd\r\n",pwmTurnAdjust.speed_gain);
						}
					}
				}
			}
			printf("C *** T=%d,a=%d\r\n",AdjustAngelTimes,angel_abs);
			printf("C *** Gain=%bd \r\n",pwmTurnAdjust.speed_gain);
			
			DcMotor_MRight_PWM_Set(TRUE,Pwm_Speed_Get_right());
    		DcMotor_MLeft_PWM_Set(TRUE,Pwm_Speed_Get_left());
		}
		else
		{	
			
			if(angel > 15){
				s_angel_is_left = 1;
				printf("LEFT !!!\r\n");
				
			}else if(angel < -15){
				s_angel_is_left = 0;
			}

			if (angel_abs < 15)
			{	
				correct_speed_tick ++;
				if (correct_speed_tick > 60)			// 记录 正确的速度
				{	
					correct_speed_tick = 0;
					correct_speed_gain = pwmTurnAdjust.speed_gain;
					printf("C *** CORRECT SPEED = %bd\r\n",correct_speed_gain);
				}
			}
			else
			{
				if (s_angel_dir_last != s_angel_is_left)		// 左右 偏航方向变化，初始化速度 重新调整
				{	
					if ((pwmTurnAdjust.speed_gain <= SPEED_GAIN_MIN) || (pwmTurnAdjust.speed_gain >= LEFT_DIVIDE_MAX))
						pwmTurnAdjust.speed_gain = s_angel_is_left?LEFT_DEFAULT_GAIN:RIGHT_DEFAULT_GAIN;
					else
					{	
						pwmTurnAdjust.speed_gain = correct_speed_gain;
						if (!s_angel_is_left)
							pwmTurnAdjust.speed_gain -= 2;
						else
							pwmTurnAdjust.speed_gain += 2;
					}
				}
				else
				{	
					;
				}
				correct_speed_tick = 0;
				AdjustAngelTimes = gain_angel*15;		// 根据偏航角度计算 调整时间
			}
			s_angel_dir_last = s_angel_is_left;

			adjust_angel_offet = 0;
			
			if (!s_angel_is_left)
			{	if (angel_abs > 60)
				{	
					if (pwmTurnAdjust.speed_gain > 15)
						pwmTurnAdjust.speed_gain -= 8;
					else
						pwmTurnAdjust.speed_gain = 5;
				}
				else if (angel_abs > 30)
				{	
					if (pwmTurnAdjust.speed_gain > 10)
					{	pwmTurnAdjust.speed_gain -= gain_angel;
						adjust_angel_offet = gain_angel - 1;
					}
					else
						pwmTurnAdjust.speed_gain = 6;
				}
				else if (angel_abs > 15)
				{	
					if (pwmTurnAdjust.speed_gain > 6)
					{	
						pwmTurnAdjust.speed_gain -= gain_angel;
						adjust_angel_offet = gain_angel - 1;
					}
					else
						pwmTurnAdjust.speed_gain = 5;
				}
				else                                                                                                                            
					AdjustAngelTimes = 0;
				
			}
			else
			{	
				if (angel_abs > 30)
				{	
					if ((pwmTurnAdjust.speed_gain+1) < LEFT_DIVIDE_MAX)
					{	pwmTurnAdjust.speed_gain += 2;
						adjust_angel_offet = 1;
					}
					else
						pwmTurnAdjust.speed_gain = LEFT_DIVIDE_MAX;
				}
				else if (angel_abs > 15)
				{	
					if (pwmTurnAdjust.speed_gain < LEFT_DIVIDE_MAX)
						pwmTurnAdjust.speed_gain += 1;
					else
						pwmTurnAdjust.speed_gain = LEFT_DIVIDE_MAX;
				}
				else
					AdjustAngelTimes = 0;
				
			}
			
			DcMotor_MRight_PWM_Set(TRUE,Pwm_Speed_Get_right());
    		DcMotor_MLeft_PWM_Set(TRUE,Pwm_Speed_Get_left());

			printf("C ### gain=%bd,speed=%bd\r\n",pwmTurnAdjust.speed_gain,nowPwmAccelerator);
			printf("C ### na=%d,la=%d\r\n",angel_abs,last_angel_offset);
		}
		//printf("C *** Gain=%bd %bd %bd\r\n",pwmTurnAdjust.speed_gain,Pwm_Speed_Get_left(),Pwm_Speed_Get_right());
		last_angel_offset = angel_abs;
		printf("@@@ a=%d\r\n",angel);
		
		
	}
	else{
		
		//register_angel_delay ++;
		//if (register_angel_delay > 10)
		//{	printf("$$ p=%bd m=%bd\r\n",nowPwmAccelerator,lastDirection);
		//	register_angel_delay = 0;
		//}
		AdjustAngelTimes = 0;
	}
}

