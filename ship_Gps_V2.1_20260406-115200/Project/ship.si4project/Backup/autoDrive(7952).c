#include "autoDrive.h"
#include <math.h>
#include "eeprom.h"


#define		AUTODRIVE_WORK_OVERTIME			10*60*100		// 10·ÖÖÓ

uint8_t autoDrive_Switch = FALSE;

uint8_t autoDrive_State = AUTO_DRIVE_IDLE;
uint8_t autoDrive_Mode = AUTO_DRIVE_CLOSE;

uint8_t autoDrive_GetDirection_Times = 0;
uint16_t autoDrive_Get_Start_Direction_Turn_Times = 0;
uint16_t autodrive_work_overtime;
uint8_t autoDrive_fail_flag = 0;

GPS_POSITION idle_Gps_Position;
GPS_POSITION now_Gps_Position,last_Gps_Position;

GPS_POSITION returnPosition;
GPS_POSITION fishPosition;


uint8_t  destination_Direction = POSITION_EAST;
uint8_t  nowRun_Direction = POSITION_EAST;
uint16_t nowRun_Angel,destination_Angel;


typedef struct
{	
	uint8_t auto_ret_onoff;
	GPS_POSITION ret_point;

}RETURN_CONFIG_T;


RETURN_CONFIG_T autodrv_cfg;


void autodrv_init(void)
{	
	ep_readShipConfig((uint8_t *)&autodrv_cfg,sizeof(RETURN_CONFIG_T));
	if (autodrv_cfg.auto_ret_onoff == 0xff)
	{	
		memset(&autodrv_cfg,0x00,sizeof(RETURN_CONFIG_T));
		autodrv_cfg.auto_ret_onoff = 0x30;			// Ä¬ÈÏ¿ªÆô
	}
	autoDrive_Switch = autodrv_cfg.auto_ret_onoff;
	//printf("init jingDu=%hu.%hu,",autodrv_cfg.ret_point.jingdu_Left,autodrv_cfg.ret_point.jingdu_Right);
    //printf("init weidu=%hu.%hu,",autodrv_cfg.ret_point.weidu_Left,autodrv_cfg.ret_point.weidu_Right);
    autodrive_work_overtime = 0;
	autoDrive_fail_flag = 0;
}

void autoDrive_Set_Mode(uint8_t mode){
    autoDrive_Mode = mode;
    if(autoDrive_Mode==AUTO_DRIVE_CLOSE){
        autoDrive_State = AUTO_DRIVE_IDLE;
    }
}

uint8_t autoDrive_Get_Mode(void){
    return autoDrive_Mode;
}

uint8_t autoDrive_in_active(void){

	//printf("get auto state=%bd,%bd \r\n",autoDrive_State,autoDrive_Mode);
	
    if (autoDrive_State != AUTO_DRIVE_IDLE)
	{	
		if (autoDrive_Mode == AUTO_DRIVE_GO_HOME_POSITION)
			return 1;
		else if(autoDrive_Mode == AUTO_DRIVE_GO_FISISH_POSITION)
			return 2;
    }
	return 0;
}

uint8_t autoDrive_is_can_active(GPS_POSITION *point)
{	
	uint16_t distance = 0;
	
	if (autoDrive_State != AUTO_DRIVE_IDLE)		//µ±Ç°ÒÑ¾­ÔÚ×Ô¶¯·µº½Ä£Ê½
		return FALSE;
	if ((point->jingdu_EW != 'E') && (point->jingdu_EW != 'W'))		// ·µº½µãÎÞÐ§
		return FALSE;
	if (point->jingdu_Left == 0)
		return FALSE;
	if ((idle_Gps_Position.jingdu_Left == 0) || (nmea41_Get_GPS_Num() < 7))	// µ±Ç°¶¨Î»Ê§°Ü»òÎÀÐÇÊýÁ¿ Ð¡ÓÚ7 ²»ÆôÓÃ·µº½
		return FALSE;
	distance = autoDrive_Get_Distance_NowPosition_Destination((uint8_t *)point,(uint8_t *)&idle_Gps_Position);
	if ((distance > 30) && (distance < 800))		// ¾àÀë´óÓÚ50Ã×  Ð¡ÓÚ600Ã×
		return TRUE;
	return FALSE;
}

void autoDrive_Set_ReturnPosition(uint8_t *data_m){
    printf("Set return position \r\n");

	if (autoDrive_State != AUTO_DRIVE_IDLE)
		return ;
    memcpy((uint8_t *)&returnPosition,data_m,sizeof(GPS_POSITION));
    printf("back jingDu=%hu.%hu \r\n",returnPosition.jingdu_Left,returnPosition.jingdu_Right);
    printf("back weidu=%hu.%hu  \r\n",returnPosition.weidu_Left,returnPosition.weidu_Right);

	if (!autoDrive_is_can_active(&returnPosition))
		return ;
    autoDrive_Set_Mode(AUTO_DRIVE_GO_HOME_POSITION);
    autoDrive_State = AUTO_DRIVE_START;
    //uprintf_array((uint8_t *)&returnPosition,sizeof(GPS_POSITION));
	autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;		// ·µº½³¬Ê±ÅÐ¶Ï
	autoDrive_fail_flag = 0;

	printf("enter autodrive  go home position!!!\r\n");
}

void autoDrive_Set_FishPosition(uint8_t *data_m){
    printf("Set fISH position\r\n");

	if (autoDrive_State != AUTO_DRIVE_IDLE)
		return ;
	
    memcpy((uint8_t *)&fishPosition,data_m,sizeof(GPS_POSITION));
    printf("fish jingDu=%hu.%hu,",fishPosition.jingdu_Left,fishPosition.jingdu_Right);
    printf("fish weidu=%hu.%hu,",fishPosition.weidu_Left,fishPosition.weidu_Right);
	
	if (!autoDrive_is_can_active(&fishPosition))
		return ;
	
    autoDrive_Set_Mode(AUTO_DRIVE_GO_FISISH_POSITION);
    autoDrive_State = AUTO_DRIVE_START;
    //uprintf_array((uint8_t *)&fishPosition,sizeof(GPS_POSITION));
	autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;
	autoDrive_fail_flag = 0;

	printf("enter autodrive  go fISH position!!!\r\n");
}

// Æô¶¯×Ô¶¯·µº½
void autoDrive_active(void){

	if (autodrv_cfg.auto_ret_onoff != 0x30)		
	{	
		if (autoDrive_fail_flag)
			return ;
		if (autoDrive_is_can_active(&autodrv_cfg.ret_point))
		{	memcpy((uint8_t *)&returnPosition,&autodrv_cfg.ret_point,sizeof(GPS_POSITION));
		    autoDrive_Set_Mode(AUTO_DRIVE_GO_HOME_POSITION);
		    autoDrive_State = AUTO_DRIVE_START;
			autodrive_work_overtime = AUTODRIVE_WORK_OVERTIME;

			printf("wireless less  go home start!!!\r\n");
		}
	}
}

void autodrive_overtime_fail(void)
{	
	autoDrive_fail_flag = 1;
}

// ±£´æ×Ô¶¯·µº½ÉèÖÃ
void autoDrive_Set_Switch(uint8_t *data_m){
    autoDrive_Switch = data_m[0];
	autodrv_cfg.auto_ret_onoff = data_m[0];
	memcpy((uint8_t *)&autodrv_cfg.ret_point,&data_m[1],sizeof(GPS_POSITION));
	ep_saveShipConfig((uint8_t *)&autodrv_cfg,sizeof(RETURN_CONFIG_T));

	//printf("set jingDu=%hu.%hu,",autodrv_cfg.ret_point.jingdu_Left,autodrv_cfg.ret_point.jingdu_Right);
    //printf("set weidu=%hu.%hu,",autodrv_cfg.ret_point.weidu_Left,autodrv_cfg.ret_point.weidu_Right);
}

uint8_t autoDrive_Get_Switch(void){
    return autoDrive_Switch;
}

void autoDrive_Set_Turn_Times(uint16_t times){
    if(times > 200){
        times = 200;
    }
    autoDrive_Get_Start_Direction_Turn_Times = times;
}

uint16_t autoDrive_Get_Turn_Times(void){
    return autoDrive_Get_Start_Direction_Turn_Times;
}
// get desposition angel
uint8_t autoDrive_Get_Direction_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData){
    GPS_POSITION *nowposition, *desposition;
    uint8_t direction = 0;
    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;
    // printf("now jing=%hu.%hu  ",nowposition->jingdu_Left,nowposition->jingdu_Right);
    // printf("now wei=%hu.%hu  ",nowposition->weidu_Left,nowposition->weidu_Right);
    // printf("last jing=%hu.%hu ",desposition->jingdu_Left,desposition->jingdu_Right);
    // printf("last wei=%hu.%hu \r\n",desposition->weidu_Left,desposition->weidu_Right);
    if(
        (nowposition->jingdu_Left > desposition->jingdu_Left)
        ||(nowposition->jingdu_Left == desposition->jingdu_Left && nowposition->jingdu_Right > desposition->jingdu_Right)
    ){
        // 向西
        direction = POSITION_WEST;
        // direction = POSITION_EAST;
        if(
            (nowposition->weidu_Left > desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right > desposition->weidu_Right)
        ){
            // 向南 
            direction = POSITION_WEST_SOUTH;
            // direction = POSITION_EAST_NORTH;
        }else if(

            (nowposition->weidu_Left < desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right < desposition->weidu_Right)
        ){
            // 向北 
            direction = POSITION_WEST_NORTH;
            // direction = POSITION_EAST_SOUTH;
        }
    }
    
    if(
        (nowposition->jingdu_Left < desposition->jingdu_Left)
        ||(nowposition->jingdu_Left == desposition->jingdu_Left && nowposition->jingdu_Right < desposition->jingdu_Right)
    ){
       // 向东 
       direction = POSITION_EAST;
        // direction = POSITION_WEST;
        if(
            (nowposition->weidu_Left > desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right > desposition->weidu_Right)
        ){
            // 向南 
            direction = POSITION_EAST_SOUTH;
            // direction = POSITION_WEST_NORTH;
        }else if(

            (nowposition->weidu_Left < desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right < desposition->weidu_Right)
        ){
            // 向北 
            direction = POSITION_EAST_NORTH;
            // direction = POSITION_WEST_SOUTH;
        }
    }
    return direction;
}

#if 0
uint16_t autoDrive_Get_Angel_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData){
    GPS_POSITION *nowposition, *desposition;

    double distance_width = 0,distance_height = 0,distance = 0,angel = 0;

    uint32_t seconds_now = 0,seconds_des = 0;
    uint32_t millseconds_now = 0,millseconds_des = 0;

    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;

// jingdu
    seconds_now = (nowposition->jingdu_Left/100*3600 + (nowposition->jingdu_Left - nowposition->jingdu_Left/100*100)*60 + nowposition->jingdu_Right/1000);
    millseconds_now = seconds_now * 1000 + (nowposition->jingdu_Right - nowposition->jingdu_Right/1000*1000);
    // printf("jingduNow.millseconds_now = %u\r\n",millseconds_now);

    seconds_des = (desposition->jingdu_Left/100*3600 + (desposition->jingdu_Left - desposition->jingdu_Left/100*100)*60 + desposition->jingdu_Right/1000);
    millseconds_des = seconds_des * 1000 + (desposition->jingdu_Right - desposition->jingdu_Right/1000*1000);
    // printf("jingduNow.millseconds_des = %u\r\n",millseconds_des);


    distance_width = millseconds_des > millseconds_now ? millseconds_des-millseconds_now:millseconds_now-millseconds_des;
    distance_width = distance_width*30.86/1000;
    // printf("jingdu.distance_width2 = %f\r\n",distance_width);

// weidu
    seconds_now = (nowposition->weidu_Left/100*3600 + (nowposition->weidu_Left - nowposition->weidu_Left/100*100)*60 + nowposition->weidu_Right/1000);
    millseconds_now = seconds_now * 1000 + (nowposition->weidu_Right - nowposition->weidu_Right/1000*1000);
    // printf("jingduNow.millseconds_now = %u\r\n",millseconds_now);

    seconds_des = (desposition->weidu_Left/100*3600 + (desposition->weidu_Left - desposition->weidu_Left/100*100)*60 + desposition->weidu_Right/1000);
    millseconds_des = seconds_des * 1000 + (desposition->weidu_Right - desposition->weidu_Right/1000*1000);
    // printf("jingduNow.millseconds_des = %u\r\n",millseconds_des);


    distance_height = millseconds_des > millseconds_now ? millseconds_des-millseconds_now:millseconds_now-millseconds_des;
    distance_height = distance_height*30.86/1000;
    // printf("jingdu.distance_height2 = %f\r\n",distance_height);
    distance = sqrt(distance_height*distance_height + distance_width*distance_width);
    // printf("distance==%f\r",distance);

    // distance_height = 2;distance_width = 1;
    angel = atan(distance_height/distance_width)*180/PI;

    printf("tan==%hu,%hu,%hu\r",(uint16_t)distance_height,(uint16_t)distance_width,(uint16_t)angel);
    // printf("angel==%hu\r",angel);
    if(angel > 360){
        angel = 65535;
    }
    return (uint16_t)angel;
}

uint16_t autoDrive_Get_Distance_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData){
    GPS_POSITION *nowposition, *desposition;

    double distance_width = 0,distance_height = 0,distance = 0;

    uint32_t seconds_now = 0,seconds_des = 0;
    uint32_t millseconds_now = 0,millseconds_des = 0;

    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;

// jingdu
    seconds_now = (nowposition->jingdu_Left/100*3600 + (nowposition->jingdu_Left - nowposition->jingdu_Left/100*100)*60 + nowposition->jingdu_Right/1000);
    millseconds_now = seconds_now * 1000 + (nowposition->jingdu_Right - nowposition->jingdu_Right/1000*1000);
    // printf("jingduNow.millseconds_now = %u\r\n",millseconds_now);

    seconds_des = (desposition->jingdu_Left/100*3600 + (desposition->jingdu_Left - desposition->jingdu_Left/100*100)*60 + desposition->jingdu_Right/1000);
    millseconds_des = seconds_des * 1000 + (desposition->jingdu_Right - desposition->jingdu_Right/1000*1000);
    // printf("jingduNow.millseconds_des = %u\r\n",millseconds_des);


    distance_width = millseconds_des > millseconds_now ? millseconds_des-millseconds_now:millseconds_now-millseconds_des;
    distance_width = distance_width*30.86/1000;
    // printf("jingdu.distance_width2 = %f\r\n",distance_width);

// weidu
    seconds_now = (nowposition->weidu_Left/100*3600 + (nowposition->weidu_Left - nowposition->weidu_Left/100*100)*60 + nowposition->weidu_Right/1000);
    // printf("weiduNow.seconds_now = %u\r\n",seconds_now);
    millseconds_now = seconds_now * 1000 + (nowposition->weidu_Right - nowposition->weidu_Right/1000*1000);
    // printf("weiduNow.millseconds_now = %u\r\n",millseconds_now);

    seconds_des = (desposition->weidu_Left/100*3600 + (desposition->weidu_Left - desposition->weidu_Left/100*100)*60 + desposition->weidu_Right/1000);
    // printf("weiduNow.seconds_des = %u\r\n",seconds_des);
    millseconds_des = seconds_des * 1000 + (desposition->weidu_Right - desposition->weidu_Right/1000*1000);
    // printf("weiduNow.millseconds_des = %u\r\n",millseconds_des);


    distance_height = millseconds_des > millseconds_now ? millseconds_des-millseconds_now:millseconds_now-millseconds_des;
    distance_height = distance_height*30.86/1000;
    // printf("jingdu.distance_height2 = %f\r\n",distance_height);

    distance = sqrt(distance_height*distance_height + distance_width*distance_width);
    // printf("jingdu.distance = %f\r\n",distance);

    return (uint16_t)distance;
}

#else

//1��	111.13km
//1��	1.85km

double cal_distance_jingdu(GPS_POSITION *now,GPS_POSITION *des)
{	
	uint16_t dd1,dd2;
	double sec1,sec2;

	dd1 = now->jingdu_Left/100;
	dd2 = des->jingdu_Left/100;

	sec1 = (double)(now->jingdu_Left%100) + ((double)now->jingdu_Right)/10000;
	sec2 = (double)(des->jingdu_Left%100) + ((double)des->jingdu_Right)/10000;

	if (dd1 == dd2)
	{	
		dd1 = 0;
		if (sec1 >= sec2)
			sec1 -= sec2;
		else
			sec1 = sec2 - sec1;
	}
	else if (dd1 > dd2)
	{	
		dd1 = dd1 - dd2;
		if (sec1 > sec2)
		{	
			sec1 -= sec2;
		}
		else
		{	
			dd1 -= 1;
			sec1 += 60.00;
			sec1 = sec1 - sec2;
		}
	}
	else
	{	
		dd1 = dd2 - dd1;
		if (sec2 > sec1)
		{	
			sec1 -= sec2;
		}
		else
		{	
			dd1 -= 1;
			sec2 += 60.00;
			sec1 = sec2 - sec1;
		}
	}
	if (dd1 != 0)
		return (double)(((double)dd1)*111130 + sec1*1850.00);
	else
		return (double)(sec1*1850.00);
	
}

double cal_distance_weidu(GPS_POSITION *now,GPS_POSITION *des)
{	
	uint16_t dd1,dd2;
	double sec1,sec2;
	uint16_t distance;

	dd1 = now->weidu_Left/100;
	dd2 = des->weidu_Left/100;

	sec1 = (double)(now->weidu_Left%100) + ((double)now->weidu_Right)/10000;
	sec2 = (double)(des->weidu_Left%100) + ((double)des->weidu_Right)/10000;

	if (dd1 == dd2)
	{	
		dd1 = 0;
		if (sec1 >= sec2)
			sec1 -= sec2;
		else
			sec1 = sec2 - sec1;
	}
	else if (dd1 > dd2)
	{	
		dd1 = dd1 - dd2;
		if (sec1 > sec2)
		{	
			sec1 -= sec2;
		}
		else
		{	
			dd1 -= 1;
			sec1 += 60.00;
			sec1 = sec1 - sec2;
		}
	}
	else
	{	
		dd1 = dd2 - dd1;
		if (sec2 > sec1)
		{	
			sec1 -= sec2;
		}
		else
		{	
			dd1 -= 1;
			sec2 += 60.00;
			sec1 = sec2 - sec1;
		}
	}
	if (dd1 != 0)
		return (double)(((double)dd1)*111130 + sec1*1850.00);
	else
		return (double)(sec1*1850.00);
}


uint16_t autoDrive_Get_Angel_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData){

    GPS_POSITION *nowposition, *desposition;

    double distance_width = 0,distance_height = 0,distance = 0;
    double angel = 0;
	
    // uprintf_array(nowpositionData,sizeof(GPS_POSITION));
    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;


    printf("weiduABS1=%hu.%hu\r\n",nowposition->jingdu_Right,desposition->jingdu_Right);
    printf("weiduABS1=%hu.%hu\r\n",nowposition->weidu_Right,desposition->weidu_Right);

	distance_width = cal_distance_jingdu(nowposition,desposition);

	distance_height = cal_distance_weidu(nowposition,desposition);

    distance = sqrt(distance_height*distance_height + distance_width*distance_width);
    // printf("distance==%f\r",distance);

    // distance_height = 2;distance_width = 1;
    angel = atan(distance_height/distance_width)*180/PI;

    printf("tan==%hu,%hu,%hu\r",(uint16_t)distance_height,(uint16_t)distance_width,(uint16_t)angel);
    // printf("angel==%hu\r",angel);
    if(angel > 360){
        angel = 65535;
    }
    return (uint16_t)angel;
}

uint16_t autoDrive_Get_Distance_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData){

    GPS_POSITION *nowposition, *desposition;
	double distance_width = 0,distance_height = 0,distance = 0;

    double angel = 0;
	
    // uprintf_array(nowpositionData,sizeof(GPS_POSITION));
    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;


	distance_width = cal_distance_jingdu(nowposition,desposition);

	distance_height = cal_distance_weidu(nowposition,desposition);

    distance = sqrt(distance_height*distance_height + distance_width*distance_width);

	printf("distance = %hu \r",distance);
	
    return (uint16_t)distance;
}

#endif

uint16_t autoDrive_Get_North_Angel(uint8_t direction,uint8_t angel){
    uint16_t northAngel = 0;
    switch(direction){
        case POSITION_NORTH:
            northAngel = 0;
        break;
        case POSITION_EAST_NORTH:
            northAngel = 90 - angel;
        break;
        case POSITION_EAST:
            northAngel = 90;
        break;
        case POSITION_EAST_SOUTH:
            northAngel = 90 + angel;
        break;
        case POSITION_SOUTH:
            northAngel = 180;
        break;
        case POSITION_WEST_SOUTH:
            northAngel = 270 - angel;
        break;
        case POSITION_WEST:
            northAngel = 270;
        break;
        case POSITION_WEST_NORTH:
            northAngel = 270 + angel;
        break;
    }
    return northAngel;
}


void autoDrive_Turn_Handle(void){
    if(autoDrive_Get_Start_Direction_Turn_Times > 0){
        autoDrive_Get_Start_Direction_Turn_Times--;
    }else{
        DcMotor_Direction_Set(DIRECTION_FORWARD);        
        DcMotor_MLeft_PWM_Set(TRUE,100);
        DcMotor_MRight_PWM_Set(TRUE,100);
    }
    
}

void autoDrive_Handle(void){
    // printf("ads=%bd",autoDrive_State);
    static uint8_t runningWaitTimes = 0;
    uint8_t direction_Sub_Abs = 0;
    uint16_t destinationDistance = 0,angel_Sub_Abs = 0;
    switch (autoDrive_State)
    {
        case AUTO_DRIVE_IDLE:{
            idle_Gps_Position.jingdu_Left  = nmea41_Get_JingDu1();
            idle_Gps_Position.jingdu_Right = nmea41_Get_JingDu2();
            idle_Gps_Position.weidu_Left   = nmea41_Get_WeiDu1();
            idle_Gps_Position.weidu_Right  = nmea41_Get_WeiDu2();
        }
        break;
        case AUTO_DRIVE_START:{
            
            Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
            if(autoDrive_Get_Mode()==AUTO_DRIVE_GO_FISISH_POSITION){
                // destination_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&fishPosition,(uint8_t *)&idle_Gps_Position);
                destination_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&idle_Gps_Position,(uint8_t *)&fishPosition);
                
                DcMotor_Direction_Set(DIRECTION_FORWARD);
                DcMotor_MLeft_PWM_Set(TRUE,90);
                DcMotor_MRight_PWM_Set(TRUE,90);
                autoDrive_Set_Turn_Times(0);
                autoDrive_State = AUTO_DRIVE_GET_DIRECTION;
            }else if(autoDrive_Get_Mode()==AUTO_DRIVE_GO_HOME_POSITION){
                // destination_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&returnPosition,(uint8_t *)&idle_Gps_Position);
                destination_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&idle_Gps_Position,(uint8_t *)&returnPosition);
    
                DcMotor_Direction_Set(DIRECTION_LEFT);
                DcMotor_MLeft_PWM_Set(TRUE,100);
                DcMotor_MRight_PWM_Set(TRUE,100);
                autoDrive_Set_Turn_Times(230);//225-good
                autoDrive_State = AUTO_DRIVE_GET_DIRECTION;
            }else{
                autoDrive_State = AUTO_DRIVE_IDLE;
                DcMotor_Direction_Set(DIRECTION_FORWARD);
                DcMotor_MRight_PWM_Set(FALSE,0);
                DcMotor_MLeft_PWM_Set(FALSE,0);
            }
            autoDrive_Turn_Handle();  
        }
        break;
        // get around direction  dong  nan  xibei   
        case AUTO_DRIVE_GET_DIRECTION:{
            autoDrive_Turn_Handle();
            if(autoDrive_Get_Start_Direction_Turn_Times > 0){

            }else{
                last_Gps_Position.jingdu_Left = nmea41_Get_JingDu1();
                last_Gps_Position.jingdu_Right = nmea41_Get_JingDu2();
                last_Gps_Position.weidu_Left = nmea41_Get_WeiDu1();
                last_Gps_Position.weidu_Right = nmea41_Get_WeiDu2();
                autoDrive_State = AUTO_DRIVE_RUNING;

                DcMotor_Direction_Set(DIRECTION_FORWARD);
                DcMotor_MLeft_PWM_Set(TRUE,90);
                DcMotor_MRight_PWM_Set(TRUE,90);
                autoDrive_Set_Turn_Times(100);
            }
        }
        break;
		
        case AUTO_DRIVE_RUNING:{
            autoDrive_Turn_Handle();
            // printf("turn times==%hu\r\n",autoDrive_Get_Start_Direction_Turn_Times);
            if (autodrive_work_overtime)			// ·µº½³¬Ê± ÍÆ³ö×Ô¶¯·µº½
				autodrive_work_overtime --;
			else
			{	
				autoDrive_Set_Mode(AUTO_DRIVE_CLOSE);
				autodrive_overtime_fail();
				break;
			}
            if (GPS_Position_Get_Position_Refresh_Flag()==TRUE && autoDrive_Get_Start_Direction_Turn_Times == 0)
            {
                GPS_Position_Set_Position_Refresh_Flag(FALSE);
                
                if(runningWaitTimes >= 2){
                    runningWaitTimes = 0;
                }else{
                    runningWaitTimes ++ ;
                    return;
                }
                now_Gps_Position.jingdu_Left = nmea41_Get_JingDu1();
                now_Gps_Position.jingdu_Right = nmea41_Get_JingDu2();
                now_Gps_Position.weidu_Left = nmea41_Get_WeiDu1();
                now_Gps_Position.weidu_Right = nmea41_Get_WeiDu2();
                // printf("now_Gps_Position=%hu--%hu\r\n",now_Gps_Position.jingdu_Right,now_Gps_Position.weidu_Right);
                // nowRun_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&last_Gps_Position);
                nowRun_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&last_Gps_Position,(uint8_t *)&now_Gps_Position);

                printf("nowRunDirect==%bd,des=%bd\r\n",nowRun_Direction,destination_Direction);
                #if 1

                    if(autoDrive_Get_Mode()==AUTO_DRIVE_GO_FISISH_POSITION){
                        destination_Angel = autoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&fishPosition);
                        if(destination_Angel == 65535){
                            return;
                        }
                        destinationDistance = autoDrive_Get_Distance_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&fishPosition);
                        // printf("fish angel==%hu\r\n",destination_Angel);
                    }else if(autoDrive_Get_Mode()==AUTO_DRIVE_GO_HOME_POSITION){
                        destination_Angel = autoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&returnPosition);
                        if(destination_Angel == 65535){
                            return;
                        }
                        destinationDistance = autoDrive_Get_Distance_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&returnPosition);
                        printf("back jingDu=%hu.%hu,",returnPosition.jingdu_Left,returnPosition.jingdu_Right);
                        printf("back weidu=%hu.%hu,",returnPosition.weidu_Left,returnPosition.weidu_Right);
                    }
                    destination_Angel = autoDrive_Get_North_Angel(destination_Direction,destination_Angel);
                    printf("des cal angel=%hu\r",destination_Angel);
                    nowRun_Angel = nmea41_Get_Angel();

                    // if(1){
                    if(nowRun_Angel==65535 || nowRun_Angel > 360){
                        // nowRun_Angel = autoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&last_Gps_Position);
                        nowRun_Angel = autoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&last_Gps_Position,(uint8_t *)&now_Gps_Position);
                        if(nowRun_Angel == 65535||nowRun_Angel > 360){
                            DcMotor_Direction_Set(DIRECTION_FORWARD);
                            DcMotor_MLeft_PWM_Set(TRUE,100);
                            DcMotor_MRight_PWM_Set(TRUE,100);
                            return;
                        }
                        nowRun_Angel = autoDrive_Get_North_Angel(nowRun_Direction,nowRun_Angel);
                        printf("cal now angel=%hu\r",nowRun_Angel);
                    }else{
                        // nowRun_AngelautoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&last_Gps_Position);
                        printf("get now angel=%hu\r\n",nowRun_Angel);
                    }
                    
                    if((nowRun_Angel>270 && destination_Angel < 90)){
                        angel_Sub_Abs = 360 - nowRun_Angel + destination_Angel;
                        if(angel_Sub_Abs < 10){
                            return;
                        }
                        DcMotor_Direction_Set(DIRECTION_RIGHT);
                        DcMotor_MLeft_PWM_Set(TRUE,100);
                        DcMotor_MRight_PWM_Set(TRUE,100);
                        autoDrive_Set_Turn_Times((angel_Sub_Abs));
                        printf("turn right=%hu\r\n",angel_Sub_Abs);
                    }else if((nowRun_Angel < 90 && destination_Angel > 270)){
                        angel_Sub_Abs = 360 - destination_Angel + nowRun_Angel;
                        if(angel_Sub_Abs < 10){
                            return;
                        }
                        DcMotor_Direction_Set(DIRECTION_LEFT);
                        DcMotor_MLeft_PWM_Set(TRUE,100);
                        DcMotor_MRight_PWM_Set(TRUE,100);
                        autoDrive_Set_Turn_Times((angel_Sub_Abs));
                        printf("turn left=%hu\r\n",angel_Sub_Abs);
                    }else{
                        angel_Sub_Abs = nowRun_Angel>destination_Angel?nowRun_Angel - destination_Angel:destination_Angel - nowRun_Angel;
                        if(angel_Sub_Abs < 10){
                            return;
                        }
                        if(nowRun_Angel > destination_Angel){
                            DcMotor_Direction_Set(DIRECTION_LEFT);
                            DcMotor_MLeft_PWM_Set(TRUE,100);
                            DcMotor_MRight_PWM_Set(TRUE,100);
                            autoDrive_Set_Turn_Times(angel_Sub_Abs);
                            printf("turn left=%hu\r\n",angel_Sub_Abs);
                        }else{
                            DcMotor_Direction_Set(DIRECTION_RIGHT);
                            DcMotor_MLeft_PWM_Set(TRUE,100);
                            DcMotor_MRight_PWM_Set(TRUE,100);
                            autoDrive_Set_Turn_Times(angel_Sub_Abs);
                            printf("turn right=%hu\r\n",angel_Sub_Abs);
                        }
                    }
                
                    if(destinationDistance < 5){
                        autoDrive_State = AUTO_DRIVE_IDLE;
                        autoDrive_Set_Mode(AUTO_DRIVE_CLOSE);
                        DcMotor_Direction_Set(DIRECTION_FORWARD);
                        DcMotor_MLeft_PWM_Set(TRUE,0);
                        DcMotor_MRight_PWM_Set(TRUE,0);
                        printf("stop\r\n");
                    }
                    printf("desAngel==%hu,nowAngel=%hu\r\n",destination_Angel,nowRun_Angel);
                    printf("distance==%hu\r\n",(uint16_t)destinationDistance);

                    destinationDistance = autoDrive_Get_Distance_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&last_Gps_Position);
                    if(destinationDistance < 1.5){
                            DcMotor_Direction_Set(DIRECTION_FORWARD);
                            DcMotor_MLeft_PWM_Set(TRUE,100);
                            DcMotor_MRight_PWM_Set(TRUE,100);
                            autoDrive_Set_Turn_Times(60);
                    }
                    // memcpy(&last_Gps_Position,&now_Gps_Position,sizeof(GPS_POSITION));
                    last_Gps_Position.jingdu_Left = now_Gps_Position.jingdu_Left;
                    last_Gps_Position.jingdu_Right = now_Gps_Position.jingdu_Right;
                    last_Gps_Position.weidu_Left = now_Gps_Position.weidu_Left;
                    last_Gps_Position.weidu_Right = now_Gps_Position.weidu_Right;
                #endif     
            }
        }
        break;

		default:		// ������״̬ �˻ص�����ģʽ
			autodrive_work_overtime = 0;
			autoDrive_Set_Mode(AUTO_DRIVE_CLOSE);
			break;
    }
}

void autoBack_Check_Handle(void){

}

GPS_POSITION testReturnPosition;
void Gps_Return_Test_Handle(void){
    testReturnPosition.jingdu_Left = 12205;
    testReturnPosition.jingdu_Right = 47538;
    testReturnPosition.weidu_Left = 3731;
    testReturnPosition.weidu_Right = 52206;
    autoDrive_Set_FishPosition((GPS_POSITION *)&testReturnPosition);
}
