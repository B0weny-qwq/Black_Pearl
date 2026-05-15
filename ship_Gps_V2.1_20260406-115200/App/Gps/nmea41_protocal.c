#include "nmea41_protocal.h"
#include "utils_splitString.h"
#include "utils_printf.h"
#include "autoDrive.h"
#include "pwm_Speed.h"

//uint8_t nmea41_Angel_yuliu[500] = {0};


uint8_t nmea41_JingDu[12 + 1] = {0},nmea41_WeiDu[12 + 1] = {0};


uint8_t Position_Refresh_Flag = 0;



extern uint8_t idata autoDrive_State;

#ifdef		GPS_DK2
uint8_t nmea41_Angel[12 + 1] = {0};
uint8_t nmea41_GPGSV_NUM[3 + 1] = {0};
uint8_t nmea41_BDGSV_NUM[3 + 1] = {0};
uint8_t nmea41_GAGSV_NUM[3 + 1] = {0};
void nmea41_protocal_Resolve_Handle(uint8_t *data_m,uint16_t length){
    uint8_t *protocal = NULL;
    uint16_t protocal_len = length - 1;

    protocal = data_m + 1; //remove "$"

    if(memcmp(protocal,"GNVTG",5) == 0){
        nmea41_Resolve_VTG(protocal,protocal_len);
    }else if(memcmp(protocal,"GNGGA",5) == 0){
       nmea41_Resolve_GGA(protocal,protocal_len);
    }else if(memcmp(protocal,"GPGSV",5) == 0){
       nmea41_Resolve_GPGSV(protocal,protocal_len);
		}else if(memcmp(protocal,"BDGSV",5) == 0){
       nmea41_Resolve_BDGSV(protocal,protocal_len);
		}else if(memcmp(protocal,"GAGSV",5) == 0){
       nmea41_Resolve_GAGSV(protocal,protocal_len);
		}else if(memcmp(protocal,"GNRMC",5) == 0){
//       printf("WWWWW   %s\r\n",protocal);
    }
//		else if(memcmp(protocal,"GN",2) == 0){
//       printf("GN   %s\r\n",protocal);
//    }
//		else if(memcmp(protocal,"GP",2) == 0){
//       printf("GP   %s\r\n",protocal);
//    }
//		else if(memcmp(protocal,"BD",2) == 0){
//       printf("BD   %s\r\n",protocal);
//    }
	if (autoDrive_State > 3)
		printf("autodrive error!!! value = %bd\r\n",autoDrive_State);
}

void nmea41_Resolve_VTG(uint8_t *protocal,uint16_t protocal_len){
    // $GPVTG,284.53,T,,M,2.440,N,4.518,K,A*3F
    memset((uint8_t *)nmea41_Angel,0,sizeof(nmea41_Angel));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 1 , (uint8_t *)nmea41_Angel,5) > 0){

    }
		printf("VTG==%s\r\n",protocal);
    printf("angel==%s\r\n",nmea41_Angel);
}


void nmea41_Resolve_GGA(uint8_t *protocal,uint16_t protocal_len){

    // $GNGLL,3730.30648,N,12201.69274,E,085447.000,A,A*4D   1  3
    // $GPGGA,073904.00,3728.97217,N,12202.31424,E,1,05,2.92,4.3,M,8.2,M,,*63
    memset((uint8_t *)nmea41_WeiDu,0,sizeof(nmea41_WeiDu));
    memset((uint8_t *)nmea41_JingDu,0,sizeof(nmea41_JingDu));
    // printf("gps");
    // uprintf_array(protocal,protocal_len);
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 2, (uint8_t *)nmea41_WeiDu,12) > 0
    &&get_NTime_String_ByChar(protocal,protocal_len, ',' , 4 , (uint8_t *)nmea41_JingDu,12) > 0){
        GPS_Position_Set_Position_Refresh_Flag(TRUE);

    }

//   printf("WeiDu==%s,JingDu==%s\r\n",nmea41_WeiDu,nmea41_JingDu);
}

void nmea41_Resolve_GPGSV(uint8_t *protocal,uint16_t protocal_len){
    // $GPGSV,3,1,10,04,03,033,,05,35,251,39,06,50,077,42,07,03,099,*73
    memset((uint8_t *)nmea41_GPGSV_NUM,0,sizeof(nmea41_GPGSV_NUM));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 3, (uint8_t *)nmea41_GPGSV_NUM,3) > 0){
        
    }
//  	printf("gpgsv==%s\r\n",nmea41_GPGSV_NUM);
}
void nmea41_Resolve_BDGSV(uint8_t *protocal,uint16_t protocal_len){
    // $GPGSV,3,1,10,04,03,033,,05,35,251,39,06,50,077,42,07,03,099,*73
    memset((uint8_t *)nmea41_BDGSV_NUM,0,sizeof(nmea41_BDGSV_NUM));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 3, (uint8_t *)nmea41_BDGSV_NUM,3) > 0){
        
    }
//  	printf("bdgsv==%s\r\n",nmea41_BDGSV_NUM);
}
void nmea41_Resolve_GAGSV(uint8_t *protocal,uint16_t protocal_len){
    // $GPGSV,3,1,10,04,03,033,,05,35,251,39,06,50,077,42,07,03,099,*73
    memset((uint8_t *)nmea41_GAGSV_NUM,0,sizeof(nmea41_GAGSV_NUM));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 3, (uint8_t *)nmea41_GAGSV_NUM,3) > 0){
        
    }
//  	printf("gagsv==%s\r\n",nmea41_GAGSV_NUM);
}
uint8_t nmea41_Get_GPS_Num(void){
    uint16_t num = 0;
		uint8_t num1 = 0;
		uint8_t num2 = 0;
		uint8_t num3 = 0;
    if(strlen((uint8_t *)nmea41_GPGSV_NUM)>0){
        num1 = atoi((uint8_t *)nmea41_GPGSV_NUM);
				
    }
		else
			num1 = 0;
	
		if(strlen((uint8_t *)nmea41_BDGSV_NUM)>0){
        num2 = atoi((uint8_t *)nmea41_BDGSV_NUM);
				
    }
		else
			num2 = 0;

		if(strlen((uint8_t *)nmea41_GAGSV_NUM)>0){
        num3 = atoi((uint8_t *)nmea41_GAGSV_NUM);
    }
		else
			num3 = 0;
		if(num1>num2)
			num = num1;
		else
			num = num2;
//		num = num1 +num2 +num3;
//		printf("num==%hu\r\n",num);
		if(num >24)
			num = 24;
    return (uint8_t)num;
		    
}
#elif		defined(GPS_DK2_RMC)
uint16_t nmea41_Angel = 0;
uint8_t nmea41_GNGGA_NUM[2 + 1] = {0};

uint8_t nmea41_GNRMC_STATUS[1 + 1] = {0};
uint8_t nmea41_GNRMC_TIME[10 + 1] = {0};
uint8_t nmea41_GNGSA_NUM = 0;

// gps angel
static uint16_t  angle_buf[GPS_DATA_MAX_PERS] = {0};
static uint8_t angle_cnt = 0;
static uint8_t last_time_stamp[10+1] = {0};
// remove max min cal average
/*
static uint16_t calc_avg_remove_max_min(uint16_t *buf, uint8_t cnt)
{
		uint16_t sum = 0;
		uint16_t min = buf[0], max = buf[0];
		uint8_t  i = 0;
    if (cnt == 0) return 65535;
    if (cnt <= 2) {
        for (i = 0; i < cnt; i++) sum += buf[i];
        return sum / cnt;
    }

    for (i = 0; i < cnt; i++) {
        sum += buf[i];
        if (buf[i] < min) min = buf[i];
        if (buf[i] > max) max = buf[i];
    }

    sum -= min;
    sum -= max;
    return sum / (cnt - 2);
}
*/
// clear 
static void angle_buf_clear(void)
{
    angle_cnt = 0;
    memset(angle_buf, 0, sizeof(angle_buf));
}

void nmea41_protocal_Resolve_Handle(uint8_t *data_m,uint16_t length){
    uint8_t *protocal = NULL;
    uint16_t protocal_len = length - 1;

    protocal = data_m + 1; //remove "$"

    if(memcmp(protocal,"GNRMC",5) == 0){
        nmea41_Resolve_RMC(protocal,protocal_len);
    }else if(memcmp(protocal,"GNGGA",5) == 0){
				nmea41_GNGSA_NUM = 0;
       nmea41_Resolve_GNGGA(protocal,protocal_len);
		}
		else if(memcmp(protocal,"GNGSA",5) == 0){
       nmea41_Resolve_GNGSA(protocal,protocal_len);
		}
		else
		{
//			printf("nmea41_protocal_Resolve_Handle   %s\r\n",protocal);
		}
//		

	if (autoDrive_State > 3)
		printf("autodrive error!!! value = %bd\r\n",autoDrive_State);
}

void nmea41_Resolve_RMC(uint8_t *protocal,uint16_t protocal_len){
    // $GNRMC,054456.00,A,3724.2182068,N,12156.4607500,E,0.07,1.84,030326,,,D,V*33
	
		uint8_t curr_time[10+1] = {0};
		uint16_t curr_angle = 0;
		uint8_t angle_str[12+1] = {0};

//    memset((uint8_t *)nmea41_Angel,0,sizeof(nmea41_Angel));	

		memset((uint8_t *)nmea41_GNRMC_STATUS,0,sizeof(nmea41_GNRMC_STATUS));
//		printf("Compass now angel=%u\r\n",(3600-nowAveAngel));
			if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 1 , (uint8_t *)curr_time,6) <= 0){
			//	printf("time err\r\n");
        return;
			}
//			printf("get gps time==%s\r\n",nmea41_GNRMC_TIME);
			
		if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 2 , (uint8_t *)nmea41_GNRMC_STATUS,1) > 0){
			
			if(memcmp(nmea41_GNRMC_STATUS,"V",1) == 0)
			{
				printf("GPS-RMC data disable!!!   %s\r\n",protocal);
				return;
			}
    }
		else
		{
			printf("GPS-RMC data NULL!!!   %s\r\n",protocal);
			return;
		}
		/*
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 8 , (uint8_t *)angle_str,5) > 0){
			
			curr_angle  = atoi((uint8_t *)angle_str);
    }
		else
		{
			 curr_angle = 65535;
		}
*/
		if (memcmp(curr_time, last_time_stamp, 6) != 0) {
//				nmea41_Angel = calc_avg_remove_max_min(angle_buf, angle_cnt);
			  if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 8 , (uint8_t *)angle_str,5) > 0){
			
					nmea41_Angel  = atoi((uint8_t *)angle_str);
				}
				else
				{
					nmea41_Angel = 65535;
				}
			
			  memset((uint8_t *)nmea41_WeiDu,0,sizeof(nmea41_WeiDu));
				memset((uint8_t *)nmea41_JingDu,0,sizeof(nmea41_JingDu));

				if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 3, (uint8_t *)nmea41_WeiDu,12) > 0
				&&get_NTime_String_ByChar(protocal,protocal_len, ',' , 5 , (uint8_t *)nmea41_JingDu,12) > 0){
					GPS_Position_Set_Position_Refresh_Flag(TRUE);
					
				}
				memcpy(last_time_stamp, curr_time, 6);
				angle_buf_clear();
				printf("angel==%hu,WeiDu==%s,JingDu==%s\r\n",nmea41_Angel,nmea41_WeiDu,nmea41_JingDu);

		}	
		if ((angle_cnt < GPS_DATA_MAX_PERS)&&(curr_angle>=0)&&((curr_angle<=360))) {
        angle_buf[angle_cnt++] = curr_angle;
    }

		
//		printf("angel==%s,WeiDu==%s,JingDu==%s\r\n",nmea41_Angel,nmea41_WeiDu,nmea41_JingDu);
		printf("GPS-RMC   %s\r\n",protocal);

}
void nmea41_Resolve_GNGSA(uint8_t *protocal,uint16_t protocal_len){
    // $GNGSA,A,3,02,0c,06,14,17,19,22,30,194,195,199,,0.9,0.6,0.7,1*06
		uint8_t i;
		uint8_t nmea41_GPGSV_NUM[3 + 1] = {0};
		nmea41_GNGSA_NUM = 0;
		for(i = 0;i<12;i++)
		{
			if(get_NTime_String_ByChar(protocal,protocal_len, ',' , (3+i), (uint8_t *)nmea41_GPGSV_NUM,3) > 0){
        
			}
			else
			{
				return;
			}
		}
		nmea41_GNGSA_NUM = i;
  	
//		printf("GPS-GNGSA   %s\r\n",protocal);
}
void nmea41_Resolve_GNGGA(uint8_t *protocal,uint16_t protocal_len){
    // $GPGSV,3,1,10,04,03,033,,05,35,251,39,06,50,077,42,07,03,099,*73
    memset((uint8_t *)nmea41_GNGGA_NUM,0,sizeof(nmea41_GNGGA_NUM));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 7, (uint8_t *)nmea41_GNGGA_NUM,2) > 0){
        
    }
//  	printf("GPS-gngga==%s\r\n",nmea41_GNGGA_NUM);
}

uint8_t nmea41_Get_GPS_Num(void){
    uint8_t num = 0;
//		if(memcmp(nmea41_GNRMC_STATUS,"V",1) == 0)
//		{
//				printf("GPS-RMC data disable---V!!! \r\n");
//				return 0;
//		}
		if(nmea41_GNGSA_NUM>0)
		{
			printf("GPS-nmea41_GNGSA_NUM==%bd\r\n",nmea41_GNGSA_NUM);
			return nmea41_GNGSA_NUM;
		}
    if(strlen((uint8_t *)nmea41_GNGGA_NUM)>0){
        num = atoi((uint8_t *)nmea41_GNGGA_NUM);
				
    }
		else
			num = 0;
//		printf("GPS-nmea41_Get_GPS_num==%bd\r\n",num);
		if(num >24)
			num = 24;
    return num;
		    
}
void nmea41_print_Angel_Time(void){
   
//  printf("GPS-nowAngelstring get time =%s\r\n",nmea41_GNRMC_TIME);
}
#else
uint8_t nmea41_GSV_NUM[3 + 1] = {0};
uint8_t nmea41_Angel[12 + 1] = {0};
void nmea41_protocal_Resolve_Handle(uint8_t *data_m,uint16_t length){
    uint8_t *protocal = NULL;
    uint16_t protocal_len = length - 3;

    protocal = data_m + 3; //remove "gp"

    if(memcmp(protocal,"VTG",3) == 0){
        nmea41_Resolve_VTG(protocal,protocal_len);
    }else if(memcmp(protocal,"GGA",3) == 0){
       nmea41_Resolve_GGA(protocal,protocal_len);
    }else if(memcmp(protocal,"GSV",3) == 0){
       nmea41_Resolve_GSV(protocal,protocal_len);
    }
	if (autoDrive_State > 3)
		printf("autodrive error!!! value = %bd\r\n",autoDrive_State);
}

void nmea41_Resolve_VTG(uint8_t *protocal,uint16_t protocal_len){
    // $GPVTG,284.53,T,,M,2.440,N,4.518,K,A*3F
    memset((uint8_t *)nmea41_Angel,0,sizeof(nmea41_Angel));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 1 , (uint8_t *)nmea41_Angel,5) > 0){

    }
		printf("Compass now angel=%u\r\n",(3600-nowAveAngel));
    printf("angel==%s\r\n",nmea41_Angel);
}


void nmea41_Resolve_GGA(uint8_t *protocal,uint16_t protocal_len){

    // $GNGLL,3730.30648,N,12201.69274,E,085447.000,A,A*4D   1  3
    // $GPGGA,073904.00,3728.97217,N,12202.31424,E,1,05,2.92,4.3,M,8.2,M,,*63
    memset((uint8_t *)nmea41_WeiDu,0,sizeof(nmea41_WeiDu));
    memset((uint8_t *)nmea41_JingDu,0,sizeof(nmea41_JingDu));
    // printf("gps");
    // uprintf_array(protocal,protocal_len);
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 2, (uint8_t *)nmea41_WeiDu,12) > 0
    &&get_NTime_String_ByChar(protocal,protocal_len, ',' , 4 , (uint8_t *)nmea41_JingDu,12) > 0){
        GPS_Position_Set_Position_Refresh_Flag(TRUE);

    }
		printf("GPS-GGA   %s\r\n",protocal);
//    printf("WeiDu==%s,JingDu==%s\r\n",nmea41_WeiDu,nmea41_JingDu);
}

void nmea41_Resolve_GSV(uint8_t *protocal,uint16_t protocal_len){
    // $GPGSV,3,1,10,04,03,033,,05,35,251,39,06,50,077,42,07,03,099,*73
    memset((uint8_t *)nmea41_GSV_NUM,0,sizeof(nmea41_GSV_NUM));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 3, (uint8_t *)nmea41_GSV_NUM,3) > 0){
        
    }
//  	printf("gsv==%s\r\n",nmea41_GSV_NUM);
}

uint8_t nmea41_Get_GPS_Num(void){
    uint8_t num = 0;
    if(strlen((uint8_t *)nmea41_GSV_NUM)>0){
        num = atoi((uint8_t *)nmea41_GSV_NUM);
        return num;
    }else{
        return 0;
    }
}
#endif

    // char *str = "12201.69274";
    // char *rightString = strstr(str,".")+1;
    // uint16_t left = atoi(str);
    // uint32_t right = atoi(rightString);

uint16_t nmea41_Get_WeiDu1(void){
    uint16_t weidu1 = 0;
    if(strlen((uint8_t *)nmea41_WeiDu)>0){
        weidu1 = atoi((uint8_t *)nmea41_WeiDu);
        return weidu1;
    }else{
        return 0;
    }
}

uint16_t nmea41_Get_WeiDu2(void){
    uint32_t weidu2 = 0;
    uint32_t weidu_seconds = 0;
    char *rightString = NULL;
    if(strlen((uint8_t *)nmea41_WeiDu)>0){
        rightString = strstr((uint8_t *)nmea41_WeiDu,".")+1;
		if (rightString == NULL)
			return 0;
		if (strlen(rightString) > 4)
			rightString[4] = 0;
	
        weidu2 = atoi(rightString);
        #if 0
        weidu_seconds = weidu2*6/10;
        weidu2 = (uint16_t)weidu_seconds;
        #endif
        return (uint16_t)weidu2;
    }else{
        return 0;
    }
}

uint16_t nmea41_Get_JingDu1(void){
    uint16_t jingdu1 = 0;
    if(strlen((uint8_t *)nmea41_JingDu)>0){
        jingdu1 = atoi((uint8_t *)nmea41_JingDu);
        return jingdu1;
    }else{
        return 0;
    }
}

uint16_t nmea41_Get_JingDu2(void){
    uint32_t jingdu2 = 0;
    uint32_t jingdu_seconds = 0;
    char *rightString = NULL;
    if(strlen((uint8_t *)nmea41_JingDu)>0){
        rightString = strstr((uint8_t *)nmea41_JingDu,".")+1;

		if (rightString == NULL)
			return 0;
		if (strlen(rightString) > 4)
			rightString[4] = 0;
	
        jingdu2 = atoi(rightString);
		#if 0
        //jingdu_seconds = jingdu2*6/10;
        //jingdu2 = (uint16_t)jingdu_seconds;
		#endif
        // printf("jingdu2==%hu\r\n",  (uint16_t)jingdu2);
        return  (uint16_t)jingdu2;
    }else{
        return 0;
    }
}


#if		defined(GPS_DK2_RMC)
uint16_t nmea41_Get_Angel(void){
        return nmea41_Angel;
}
#else
uint16_t nmea41_Get_Angel(void){
    uint16_t angel = 0;
    if(strlen((uint8_t *)nmea41_Angel)>0){
        // angel = atof((uint8_t *)&nmea41_Angel);
        angel = atoi((uint8_t *)nmea41_Angel);
//        printf("GPS-nowAngelstring=%s,%hu\r\n",nmea41_Angel,angel);
        return angel;
    }else{
        return 65535;
    }
}
#endif


uint8_t nmea41_Get_EW(void){
    return 'E';
}

uint8_t nmea41_Get_NS(void){
    return 'W';
}

void GPS_Position_Set_Position_Refresh_Flag(uint8_t flag){
    Position_Refresh_Flag = flag;
}

uint8_t GPS_Position_Get_Position_Refresh_Flag(void){
    return Position_Refresh_Flag;
}
