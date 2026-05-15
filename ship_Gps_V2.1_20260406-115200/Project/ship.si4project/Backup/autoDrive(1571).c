#include "autoDrive.h"
#include <math.h>
#include "eeprom.h"


uint8_t autoDrive_Switch = FALSE;

uint8_t autoDrive_State = AUTO_DRIVE_IDLE;
uint8_t autoDrive_Mode = AUTO_DRIVE_CLOSE;

uint8_t autoDrive_GetDirection_Times = 0;
uint16_t autoDrive_Get_Start_Direction_Turn_Times = 0;

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
		autodrv_cfg.auto_ret_onoff = 0x30;			// 默认开启
	}
	autoDrive_Switch = autodrv_cfg.auto_ret_onoff;
	//printf("init jingDu=%hu.%hu,",autodrv_cfg.ret_point.jingdu_Left,autodrv_cfg.ret_point.jingdu_Right);
    //printf("init weidu=%hu.%hu,",autodrv_cfg.ret_point.weidu_Left,autodrv_cfg.ret_point.weidu_Right);
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



void autoDrive_Set_ReturnPosition(uint8_t *data_m){
    printf("Set return position");
    memcpy((uint8_t *)&returnPosition,data_m,sizeof(GPS_POSITION));
    printf("back jingDu=%hu.%hu,",returnPosition.jingdu_Left,returnPosition.jingdu_Right);
    printf("back weidu=%hu.%hu,",returnPosition.weidu_Left,returnPosition.weidu_Right);
    autoDrive_Set_Mode(AUTO_DRIVE_GO_HOME_POSITION);
    autoDrive_State = AUTO_DRIVE_START;
    uprintf_array((uint8_t *)&returnPosition,sizeof(GPS_POSITION));
}

void autoDrive_Set_FishPosition(uint8_t *data_m){
    printf("Set fISH position");
    memcpy((uint8_t *)&fishPosition,data_m,sizeof(GPS_POSITION));
    printf("fish jingDu=%hu.%hu,",returnPosition.jingdu_Left,returnPosition.jingdu_Right);
    printf("fish weidu=%hu.%hu,",returnPosition.weidu_Left,returnPosition.weidu_Right);
    autoDrive_Set_Mode(AUTO_DRIVE_GO_FISISH_POSITION);
    autoDrive_State = AUTO_DRIVE_START;
    uprintf_array((uint8_t *)&fishPosition,sizeof(GPS_POSITION));
}

// 启动自动返航
void autoDrive_active(void){

	if (autodrv_cfg.auto_ret_onoff != 0x30)		
	{	
		uint16_t distance = 0;
		if (autoDrive_State != AUTO_DRIVE_IDLE)		//当前已经在自动返航模式
			return ;
		if ((autodrv_cfg.ret_point.jingdu_EW != 'E') && (autodrv_cfg.ret_point.jingdu_EW != 'W'))		// 返航点无效
			return;
		if ((idle_Gps_Position.jingdu_Left == 0) || (nmea41_Get_GPS_Num() < 7))	// 当前定位失败或卫星数量 小于7 不启用返航
			return ;
		distance = autoDrive_Get_Distance_NowPosition_Destination((uint8_t *)&autodrv_cfg.ret_point,(uint8_t *)&idle_Gps_Position);
		if ((distance > 50) && (distance < 600))
		{	memcpy((uint8_t *)&returnPosition,&autodrv_cfg.ret_point,sizeof(GPS_POSITION));
		    autoDrive_Set_Mode(AUTO_DRIVE_GO_HOME_POSITION);
		    autoDrive_State = AUTO_DRIVE_START;
		}
	}
}

// 保存自动返航设置
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
    autoDrive_Get_Start_Direction_Turn_Times = times;
}

uint16_t autoDrive_Get_Turn_Times(void){
    return autoDrive_Get_Start_Direction_Turn_Times;
}
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
        // 鍚戣タ
        direction = POSITION_EAST;
        if(
            (nowposition->weidu_Left > desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right > desposition->weidu_Right)
        ){
            // 鍚戝崡 
            direction = POSITION_EAST_NORTH;
        }else if(

            (nowposition->weidu_Left < desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right < desposition->weidu_Right)
        ){
            // 鍚戝寳 
            direction = POSITION_EAST_SOUTH;
        }
    }
    
    if(
        (nowposition->jingdu_Left < desposition->jingdu_Left)
        ||(nowposition->jingdu_Left == desposition->jingdu_Left && nowposition->jingdu_Right < desposition->jingdu_Right)
    ){
       // 鍚戜笢 
        direction = POSITION_WEST;
        if(
            (nowposition->weidu_Left > desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right > desposition->weidu_Right)
        ){
            // 鍚戝崡 
            direction = POSITION_WEST_NORTH;
        }else if(

            (nowposition->weidu_Left < desposition->weidu_Left)
            ||(nowposition->weidu_Left == desposition->weidu_Left && nowposition->weidu_Right < desposition->weidu_Right)
        ){
            // 鍚戝寳 
            direction = POSITION_WEST_SOUTH;
        }
    }
    return direction;
}


uint8_t debug_data_buf[40];
void SysTimer_delay10ms(uint8_t time);

void wireless_send_debug_data(uint8_t *data_m,uint8_t len);

void send_debug_data(void)
{		
	int index = 0;
	long gps_float = 0;
	int gps_int = 0;
	
	memcpy(&debug_data_buf[index],&now_Gps_Position.jingdu_Left,2);
	index += 2;

	gps_float = now_Gps_Position.jingdu_Right;
	memcpy(&debug_data_buf[index],&gps_float,4);
	index += 4;
	
	memcpy(&debug_data_buf[index],&now_Gps_Position.weidu_Left,2);
	index += 2;

	gps_float = now_Gps_Position.weidu_Right;
	//memcpy(&debug_data_buf[index],&now_Gps_Position.weidu_Right,4);
	memcpy(&debug_data_buf[index],&gps_float,4);
	index += 4;
	
	memcpy(&debug_data_buf[index],&returnPosition.jingdu_Left,2);
	index += 2;

	gps_int = returnPosition.jingdu_Right;
	memcpy(&debug_data_buf[index],&gps_int,2);
	index += 2;
	
	memcpy(&debug_data_buf[index],&returnPosition.weidu_Left,2);
	index += 2;

	gps_int = returnPosition.weidu_Right;
	memcpy(&debug_data_buf[index],&gps_int,2);
	index += 2;
	

	memcpy(&debug_data_buf[index],&destination_Angel,2);
	index += 2;

	memcpy(&debug_data_buf[index],&nowRun_Angel,2);
	index += 2;

	debug_data_buf[index] = destination_Direction;
	index += 1;

	debug_data_buf[index] = nowRun_Direction ;
	index += 1;

	//RF_Send_Gps_Data(wirelessConfig.RF_Channel[2],LT8920_SEND);
	wireless_send_debug_data((uint8_t *)debug_data_buf,index);
	SysTimer_delay10ms(3);
	wireless_send_debug_data((uint8_t *)debug_data_buf,index);
}


uint16_t autoDrive_Get_Angel_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData){
    GPS_POSITION *nowposition, *desposition;

    uint16_t SubLeftJingdu = 0,SubRightJingdu = 0;
    uint16_t SubLeftWeidu = 0,SubRightWeidu = 0;
    double distance_width = 0,distance_height = 0,distance = 0;

    
    uint8_t jingdu_dd = 0, jingdu_min = 0, jingdu_seconds = 0;
    uint16_t jingdu_millseconds = 0;

    uint8_t weidu_dd = 0, weidu_min = 0, weidu_seconds = 0;
    uint16_t weidu_millseconds = 0;
    double angel = 0;
    // uprintf_array(nowpositionData,sizeof(GPS_POSITION));
    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;


    printf("weiduABS1=%hu.%hu\r\n",nowposition->jingdu_Right,desposition->jingdu_Right);
    printf("weiduABS1=%hu.%hu\r\n",nowposition->weidu_Right,desposition->weidu_Right);
    
    // printf("jingduABS1=%lu--%l04x\r\n",1234,0x1234);

    // SubLeftJingdu = labs(nowposition->jingdu_Left - desposition->jingdu_Left);
    // SubRightJingdu = labs(nowposition->jingdu_Right - desposition->jingdu_Right);
    SubLeftJingdu  = nowposition->jingdu_Left >= desposition->jingdu_Left?nowposition->jingdu_Left - desposition->jingdu_Left:desposition->jingdu_Left - nowposition->jingdu_Left;
    SubRightJingdu = nowposition->jingdu_Right >= desposition->jingdu_Right?nowposition->jingdu_Right - desposition->jingdu_Right:desposition->jingdu_Right - nowposition->jingdu_Right;
    // printf("jingduABS2=%hu.%hu\r\n",SubLeftJingdu,SubRightJingdu);

    // SubLeftWeidu = labs(nowposition->weidu_Left - desposition->weidu_Left);
    // SubRightWeidu = labs(nowposition->weidu_Right - desposition->weidu_Right);

    SubLeftWeidu  = nowposition->weidu_Left >= desposition->weidu_Left?nowposition->weidu_Left - desposition->weidu_Left:desposition->weidu_Left - nowposition->weidu_Left;
    SubRightWeidu = nowposition->weidu_Right >= desposition->weidu_Right?nowposition->weidu_Right - desposition->weidu_Right:desposition->weidu_Right - nowposition->weidu_Right;
    // printf("weiduABS=%hu.%hu\r\n",SubLeftWeidu,SubRightWeidu);

    if(SubLeftJingdu > 100){
        jingdu_dd = SubLeftJingdu/100; 
    }
    jingdu_min = SubLeftJingdu - jingdu_dd * 100;
    jingdu_seconds = SubRightJingdu / 1000;
    jingdu_millseconds = SubRightJingdu - jingdu_seconds*1000;
    // printf("mill cal==%hu - %hu=%hu\r",SubRightJingdu,jingdu_seconds*1000,jingdu_millseconds);
    // printf("jing min==%hu,seconds==%hu,mill==%hu\r\n",jingdu_min,jingdu_seconds,jingdu_millseconds);
    distance_width = (double)((jingdu_dd * 60) + jingdu_seconds + (double)((double)jingdu_millseconds/1000)) * 30.86;
    // printf("jingdu_distance==%b02x\r",distance_width);


    if(SubLeftWeidu > 100){
        weidu_dd = SubLeftWeidu/100; 
    }
    weidu_min = SubLeftWeidu - jingdu_dd * 100;
    weidu_seconds = SubRightWeidu / 1000;
    weidu_millseconds = SubRightWeidu - weidu_seconds*1000;
    // printf("wei min==%hu,seconds==%hu,mill==%hu\r\n",weidu_min,weidu_seconds,weidu_millseconds);
    distance_height = (double)((weidu_dd * 60) + weidu_seconds + (double)((double)weidu_millseconds/1000)) * 30.86;

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

    uint16_t SubLeftJingdu = 0,SubRightJingdu = 0;
    uint16_t SubLeftWeidu = 0,SubRightWeidu = 0;
    double distance_width = 0,distance_height = 0,distance = 0;

    
    uint8_t jingdu_dd = 0, jingdu_min = 0, jingdu_seconds = 0;
    uint16_t jingdu_millseconds = 0;

    uint8_t weidu_dd = 0, weidu_min = 0, weidu_seconds = 0;
    uint16_t weidu_millseconds = 0;
    double angel = 0;
    // uprintf_array(nowpositionData,sizeof(GPS_POSITION));
    nowposition = (GPS_POSITION *)nowpositionData;
    desposition = (GPS_POSITION *)despositionData;



    SubLeftJingdu  = nowposition->jingdu_Left >= desposition->jingdu_Left?nowposition->jingdu_Left - desposition->jingdu_Left:desposition->jingdu_Left - nowposition->jingdu_Left;
    SubRightJingdu = nowposition->jingdu_Right >= desposition->jingdu_Right?nowposition->jingdu_Right - desposition->jingdu_Right:desposition->jingdu_Right - nowposition->jingdu_Right;
    // printf("jingduABS2=%hu.%hu\r\n",SubLeftJingdu,SubRightJingdu);

    // SubLeftWeidu = labs(nowposition->weidu_Left - desposition->weidu_Left);
    // SubRightWeidu = labs(nowposition->weidu_Right - desposition->weidu_Right);

    SubLeftWeidu  = nowposition->weidu_Left >= desposition->weidu_Left?nowposition->weidu_Left - desposition->weidu_Left:desposition->weidu_Left - nowposition->weidu_Left;
    SubRightWeidu = nowposition->weidu_Right >= desposition->weidu_Right?nowposition->weidu_Right - desposition->weidu_Right:desposition->weidu_Right - nowposition->weidu_Right;
    // printf("weiduABS=%hu.%hu\r\n",SubLeftWeidu,SubRightWeidu);

    if(SubLeftJingdu > 100){
        jingdu_dd = SubLeftJingdu/100; 
    }
    jingdu_min = SubLeftJingdu - jingdu_dd * 100;
    jingdu_seconds = SubRightJingdu / 1000;
    jingdu_millseconds = SubRightJingdu - jingdu_seconds*1000;
    // printf("mill cal==%hu - %hu=%hu\r",SubRightJingdu,jingdu_seconds*1000,jingdu_millseconds);
    // printf("jing min==%hu,seconds==%hu,mill==%hu\r\n",jingdu_min,jingdu_seconds,jingdu_millseconds);
    distance_width = (double)((jingdu_dd * 60) + jingdu_seconds + (double)((double)jingdu_millseconds/1000)) * 30.86;
    // printf("jingdu_distance==%b02x\r",distance_width);


    if(SubLeftWeidu > 100){
        weidu_dd = SubLeftWeidu/100; 
    }
    weidu_min = SubLeftWeidu - jingdu_dd * 100;
    weidu_seconds = SubRightWeidu / 1000;
    weidu_millseconds = SubRightWeidu - weidu_seconds*1000;
    // printf("wei min==%hu,seconds==%hu,mill==%hu\r\n",weidu_min,weidu_seconds,weidu_millseconds);
    distance_height = (double)((weidu_dd * 60) + weidu_seconds + (double)((double)weidu_millseconds/1000)) * 30.86;

    distance = sqrt(distance_height*distance_height + distance_width*distance_width);

    return (uint16_t)distance;
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
                destination_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&fishPosition,(uint8_t *)&idle_Gps_Position);
                DcMotor_Direction_Set(DIRECTION_FORWARD);
                DcMotor_MLeft_PWM_Set(TRUE,90);
                DcMotor_MRight_PWM_Set(TRUE,90);
                autoDrive_Set_Turn_Times(0);
                autoDrive_State = AUTO_DRIVE_GET_DIRECTION;
            }else if(autoDrive_Get_Mode()==AUTO_DRIVE_GO_HOME_POSITION){
                destination_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&returnPosition,(uint8_t *)&idle_Gps_Position);
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
            }
        }
        break;

        case AUTO_DRIVE_RUNING:{
            autoDrive_Turn_Handle();
            // printf("turn times==%hu\r\n",autoDrive_Get_Start_Direction_Turn_Times);
            
            if (GPS_Position_Get_Position_Refresh_Flag()==TRUE && autoDrive_Get_Start_Direction_Turn_Times == 0)
            {
                GPS_Position_Set_Position_Refresh_Flag(FALSE);
                
                if(runningWaitTimes >= 3){
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
                nowRun_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&last_Gps_Position);
                // nowRun_Direction = autoDrive_Get_Direction_NowPosition_Destination((uint8_t *)&last_Gps_Position,(uint8_t *)&now_Gps_Position);

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
                    if(nowRun_Angel==65535 || nowRun_Angel > 361){
                        // nowRun_Angel = autoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&now_Gps_Position,(uint8_t *)&last_Gps_Position);
                        nowRun_Angel = autoDrive_Get_Angel_NowPosition_Destination((uint8_t *)&last_Gps_Position,(uint8_t *)&now_Gps_Position);
                        if(nowRun_Angel == 65535){
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
                        autoDrive_Set_Turn_Times((angel_Sub_Abs)/5);
                        printf("turn right=%hu\r\n",angel_Sub_Abs);
                    }else if((nowRun_Angel < 90 && destination_Angel > 270)){
                        angel_Sub_Abs = 360 - destination_Angel + nowRun_Angel;
                        if(angel_Sub_Abs < 10){
                            return;
                        }
                        DcMotor_Direction_Set(DIRECTION_LEFT);
                        DcMotor_MLeft_PWM_Set(TRUE,100);
                        DcMotor_MRight_PWM_Set(TRUE,100);
                        autoDrive_Set_Turn_Times((angel_Sub_Abs)/5);
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
                            autoDrive_Set_Turn_Times(angel_Sub_Abs/3);
                            printf("turn left=%hu\r\n",angel_Sub_Abs);
                        }else{
                            DcMotor_Direction_Set(DIRECTION_RIGHT);
                            DcMotor_MLeft_PWM_Set(TRUE,100);
                            DcMotor_MRight_PWM_Set(TRUE,100);
                            autoDrive_Set_Turn_Times(angel_Sub_Abs/3);
                            printf("turn right=%hu\r\n",angel_Sub_Abs);
                        }
                    }
                
                    if(destinationDistance < 10){
                        autoDrive_State = AUTO_DRIVE_IDLE;
                        printf("stop\r\n");
                    }
                    //printf("desAngel==%hu,nowAngel=%hu\r\n",destination_Angel,nowRun_Angel);
                    //printf("distance==%hu\r\n",(uint16_t)destinationDistance);

                    // memcpy(&last_Gps_Position,&now_Gps_Position,sizeof(GPS_POSITION));
                    last_Gps_Position.jingdu_Left = now_Gps_Position.jingdu_Left;
                    last_Gps_Position.jingdu_Right = now_Gps_Position.jingdu_Right;
                    last_Gps_Position.weidu_Left = now_Gps_Position.weidu_Left;
                    last_Gps_Position.weidu_Right = now_Gps_Position.weidu_Right;
					//send_debug_data();
                #endif     
            }
        }
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
