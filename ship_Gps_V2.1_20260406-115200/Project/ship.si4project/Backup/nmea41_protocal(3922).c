#include "nmea41_protocal.h"
#include "utils_splitString.h"
#include "utils_printf.h"
#include "autoDrive.h"

//uint8_t nmea41_Angel_yuliu[500] = {0};

uint8_t nmea41_Angel[12 + 1] = {0};
uint8_t nmea41_JingDu[12 + 1] = {0},nmea41_WeiDu[12 + 1] = {0};
uint8_t nmea41_GSV_NUM[3 + 1] = {0};

uint8_t Position_Refresh_Flag = 0;



extern uint8_t idata autoDrive_State;

void nmea41_protocal_Resolve_Handle(uint8_t *data_m,uint16_t length){
    uint8_t *protocal = NULL;
    uint16_t protocal_len = length - 3;
    uint8_t tmpdata[6] = {0};
    char *cmdId = NULL;

    protocal = data_m + 3; //remove "gp"

#if 0
    // GPGLL,3731.52627,N,12205.44154,E,073213.00,A,A*
     
    strncpy((char *)&tmpdata,(char *)protocal,3);
    cmdId = strtok((char *)&tmpdata,(char *)&",");

    // printf("cmdId==%s\r\n",cmdId);
    if (cmdId == NULL){
        return;
    }
#endif
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
    memset((uint8_t *)&nmea41_Angel,0,sizeof(nmea41_Angel));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 1 , (uint8_t *)&nmea41_Angel,5) > 0){

    }
    //printf("angel==%s\r\n",nmea41_Angel);
}

uint8_t returnTestFlag = 0;
void nmea41_Resolve_GGA(uint8_t *protocal,uint16_t protocal_len){

    // $GNGLL,3730.30648,N,12201.69274,E,085447.000,A,A*4D   1  3
    // $GPGGA,073904.00,3728.97217,N,12202.31424,E,1,05,2.92,4.3,M,8.2,M,,*63
    memset((uint8_t *)&nmea41_WeiDu,0,sizeof(nmea41_WeiDu));
    memset((uint8_t *)&nmea41_JingDu,0,sizeof(nmea41_JingDu));
    // printf("gps");
    // uprintf_array(protocal,protocal_len);
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 2, (uint8_t *)&nmea41_WeiDu,12) > 0
    &&get_NTime_String_ByChar(protocal,protocal_len, ',' , 4 , (uint8_t *)&nmea41_JingDu,12) > 0){
        GPS_Position_Set_Position_Refresh_Flag(TRUE);
        // printf("gps ok on=%bd",returnTestFlag);
        if(returnTestFlag == 0){
            returnTestFlag = 1;
            // Gps_Return_Test_Handle();
            // printf("gps return test start\r\n");
        }
    }

    printf("WeiDu==%s,JingDu==%s\r\n",nmea41_WeiDu,nmea41_JingDu);
}

void nmea41_Resolve_GSV(uint8_t *protocal,uint16_t protocal_len){
    // $GPGSV,3,1,10,04,03,033,,05,35,251,39,06,50,077,42,07,03,099,*73
    memset((uint8_t *)&nmea41_GSV_NUM,0,sizeof(nmea41_GSV_NUM));
    if(get_NTime_String_ByChar(protocal,protocal_len, ',' , 3, (uint8_t *)&nmea41_GSV_NUM,3) > 0){
        
    }
  	//printf("gsv==%s\r\n",nmea41_GSV_NUM);
}
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

#if 0
uint16_t nmea41_Get_JingDu22(void){
    uint32_t jingdu2 = 0;
    uint32_t jingdu_seconds = 0;
    char *rightString = NULL;
    if(strlen((uint8_t *)&nmea41_JingDu)>0){
        rightString = strstr((uint8_t *)&nmea41_JingDu,".")+1;
        jingdu2 = atoi(rightString);
        jingdu_seconds = jingdu2*6/10;
        jingdu2 = (uint16_t)jingdu_seconds;
        return  (uint16_t)jingdu2;
    }else{
        return 0;
    }
}
#endif


uint16_t nmea41_Get_Angel(void){
    uint16_t angel = 0;
    if(strlen((uint8_t *)nmea41_Angel)>0){
        // angel = atof((uint8_t *)&nmea41_Angel);
        angel = atoi((uint8_t *)nmea41_Angel);
        //printf("nowAngelstring=%s,%hu\r\n",nmea41_Angel,angel);
        return angel;
    }else{
        return 65535;
    }
}

uint8_t nmea41_Get_GPS_Num(void){
    uint8_t num = 0;
    if(strlen((uint8_t *)&nmea41_GSV_NUM)>0){
        num = atoi((uint8_t *)&nmea41_GSV_NUM);
        return num;
    }else{
        return 0;
    }
}

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
