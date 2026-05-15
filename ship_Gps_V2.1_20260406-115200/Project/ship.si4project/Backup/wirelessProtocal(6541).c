#include "wirelessProtocal.h"
#include "utils_printf.h"
#include "power_Adc.h"

#include "led.h"
#include "pwm.h"
#include "DcMotor_Gpio.h"
#include "LT8920_SPI.h"
#include "utils_checkSum.h"
#include "nmea41_protocal.h"
#include "autoDrive.h"
WIRELESS_CONFIG wirelessConfig;
uint16_t wirelessAccelerator_OutTime_Times = 0;
uint16_t wirelessAccelerator_close_motor_time = 0;

uint16_t pair_wait_rsp_time = 0;


uint8_t protocalData[WIRELESS_PROTOCAL_MAX_LEN] = {0};

void WirelessProtocal_Receive_Handle(uint8_t *data_m,uint8_t len){
    uint8_t i = 0;
    
    uint8_t protocalIndex = 0;
    uint8_t protocalRecLen = 0;
    uint8_t protocalRecFinishFlag = 0;
    uint8_t protocalCheckResult = TRUE;
    uint8_t protocalCheckSum = 0;
    // printf("wirlepro receive\r\n");
    // uprintf_array(data_m,len);

    for(i=0;i<len;i++){
        protocalCheckResult = TRUE;
        // printf("data[%bd]=%b02x,%bd\r\n",i,data_m[i],protocalIndex);
        switch(protocalIndex){
            case 0:{
                if(data_m[i]!=0xaa){
                    protocalIndex = 0;
                    protocalCheckResult = FALSE;              
                }
            }
            break;
            case 1:{
                protocalRecLen = data_m[i] + 1;
				if (protocalRecLen > 25)
					protocalRecLen = 2;
            }
            break;

            default:{
                if(protocalRecLen > 0){
                    protocalRecLen --;
                    if(protocalRecLen == 0){
                        protocalRecFinishFlag = TRUE;
                        // printf("rec finish\r\n");
                    }

                }
            }
            break;
        }
        if(protocalCheckResult == TRUE){
            protocalData[protocalIndex++] = data_m[i];
            if(protocalRecFinishFlag == TRUE){
                protocalRecFinishFlag = FALSE;
                protocalCheckSum = cal_checksum_XOR((uint8_t *)&protocalData[1],protocalIndex-3);
                // printf("Cal Check=%b02x,data=%b02x\r\n",protocalCheckSum,protocalData[protocalIndex-2]);
                if(protocalCheckSum == protocalData[protocalIndex-2] && protocalData[protocalIndex-1] == 0xbb){
                    // printf("check success\r\n");
                    // uprintf_array(data_m,len);
                    WirelessProtocal_Resolve_Handle((uint8_t *)&protocalData,protocalIndex);
                }
            }
        }

    }


}

        // �? 200


// �? 0  // �? 100     // �? 200   


        // �? 0
void WirelessProtoca_Motor_Control(uint8_t leftRight,uint8_t frontBack){
#if 0
    if(leftRight < 110 && leftRight > 90 && frontBack > 110){
        Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
        DcMotor_Direction_Set(DIRECTION_FORWARD);
        DcMotor_MRight_PWM_Set(TRUE,frontBack-100);
        DcMotor_MLeft_PWM_Set(TRUE,frontBack-100);
        
    }else if(leftRight < 110 && leftRight > 90 && frontBack < 90){
        Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
        DcMotor_Direction_Set(DIRECTION_BACK);
        DcMotor_MRight_PWM_Set(TRUE,100-frontBack);
        DcMotor_MLeft_PWM_Set(TRUE,100-frontBack);
        
    }else if(leftRight >= 110 ){
        Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
        // right
        if(frontBack > 110){
            // right and forward
            DcMotor_Direction_Set(DIRECTION_RIGHT);
            DcMotor_MRight_PWM_Set(TRUE,frontBack-100);
            DcMotor_MLeft_PWM_Set(TRUE,frontBack-100);
        }else if(frontBack < 90){
            // right and back
            DcMotor_Direction_Set(DIRECTION_LEFT);
            DcMotor_MRight_PWM_Set(TRUE,100-frontBack);
            DcMotor_MLeft_PWM_Set(TRUE,100-frontBack);
        }else{
            DcMotor_Direction_Set(DIRECTION_RIGHT);
            DcMotor_MRight_PWM_Set(TRUE,leftRight-100);
            DcMotor_MLeft_PWM_Set(TRUE,leftRight-100);
        }
        

    }else if(leftRight <= 90 ){
        Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
        // left
        if(frontBack > 110){
            // right and forward
            DcMotor_Direction_Set(DIRECTION_LEFT);
            DcMotor_MRight_PWM_Set(TRUE,frontBack-100);
            DcMotor_MLeft_PWM_Set(TRUE,frontBack-100);
        }else if(frontBack < 90){
            // right and back
            DcMotor_Direction_Set(DIRECTION_RIGHT);
            DcMotor_MRight_PWM_Set(TRUE,100-frontBack);
            DcMotor_MLeft_PWM_Set(TRUE,100-frontBack);
        }else{
            DcMotor_Direction_Set(DIRECTION_LEFT);
            DcMotor_MRight_PWM_Set(TRUE,100-leftRight);
            DcMotor_MLeft_PWM_Set(TRUE,100-leftRight);
        }
    }else{
        if(Get_DcMotor_PWM_CruiseControl_Mode()!= CRUISE_STOP){
            return;
        }
        DcMotor_Direction_Set(DIRECTION_FORWARD);
        DcMotor_MRight_PWM_Set(FALSE,0);
        DcMotor_MLeft_PWM_Set(FALSE,0);
    }
#else
   
        uint8_t abs_leftRight,abs_frontBack;
        uint8_t myDcMotor_PWM_CruiseControl_Mode;
        abs_leftRight = abs(leftRight - 100);
        abs_frontBack = abs(frontBack - 100) + 10;
        if(abs_frontBack > abs_leftRight && (abs_leftRight > 10 || abs_frontBack > 10)){
            if(frontBack > 110){
                DcMotor_Direction_Set(DIRECTION_FORWARD);
                DcMotor_MRight_PWM_Set(TRUE,frontBack-100);
                DcMotor_MLeft_PWM_Set(TRUE,frontBack-100);
                // printf("DIRECTION_FORWARD==%b02x\r\n",frontBack-100);
            }else if(frontBack < 90){
                DcMotor_Direction_Set(DIRECTION_BACK);
                DcMotor_MRight_PWM_Set(TRUE,100-frontBack);
                DcMotor_MLeft_PWM_Set(TRUE,100-frontBack);
                // printf("DIRECTION_BACK==%b02x\r\n",100-frontBack);
                if(Get_DcMotor_PWM_CruiseControl_Mode() != CRUISE_STOP)		// 巡航模式�?油门后退 退出巡航模�?
                {	
                	if (frontBack < 30)
						Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
                }
            }

        }else if(abs_frontBack < abs_leftRight && (abs_leftRight > 10 || abs_frontBack > 10)){
            if(leftRight <= 90 ){
                DcMotor_Direction_Set(DIRECTION_LEFT);
                DcMotor_MRight_PWM_Set(TRUE,100-leftRight);
                DcMotor_MLeft_PWM_Set(TRUE,100-leftRight);
                // printf("DIRECTION_LEFT==%b02x\r\n",100-leftRight);
            }else if(leftRight > 110){
                DcMotor_Direction_Set(DIRECTION_RIGHT);
                DcMotor_MRight_PWM_Set(TRUE,leftRight-100);
                DcMotor_MLeft_PWM_Set(TRUE,leftRight-100);
                // printf("DIRECTION_RIGHT==%b02x\r\n",leftRight-100);
            }
        }else{		// 调整方向�?继续巡航
            myDcMotor_PWM_CruiseControl_Mode = Get_DcMotor_PWM_CruiseControl_Mode();
            if(Get_DcMotor_PWM_CruiseControl_Mode()!= CRUISE_STOP){
				DcMotor_Direction_Set(DIRECTION_FORWARD);
                switch(myDcMotor_PWM_CruiseControl_Mode){
                    case CRUISE_LOW:
                        DcMotor_MLeft_PWM_Set(TRUE,30);
                        DcMotor_MRight_PWM_Set(TRUE,30);
                    break;

                    case CRUISE_MIDDLE:
                        DcMotor_MLeft_PWM_Set(TRUE,60);
                        DcMotor_MRight_PWM_Set(TRUE,60);
                    break;

                    case CRUISE_HIGH:
                        DcMotor_MLeft_PWM_Set(TRUE,100);
                        DcMotor_MRight_PWM_Set(TRUE,100);
                    break;
                }
                return;
            }
        
            DcMotor_Direction_Set(DIRECTION_FORWARD);
            DcMotor_MRight_PWM_Set(FALSE,0);
            DcMotor_MLeft_PWM_Set(FALSE,0);
        }

    #endif 

}

void WirelessProtoca_Key_Control(uint8_t accelerator_leftRight,uint8_t accelerator_frontBack,uint8_t keyCode){

    static uint8_t lastKeyCode = 0;

    if(lastKeyCode != keyCode){
        lastKeyCode = keyCode;
        switch(lastKeyCode){
            case KEY_A_FLAG:
                Ship_LED_Set_Handle();
            break;
            case KEY_B_FLAG:
                // DcMotor_PWM_CruiseControl_Set_Handle();
            break;
            case KEY_C_FLAG:
                DcMotor_Front_Set_Run_MillSeconds(150);
            break;
            case KEY_D_FLAG:
                DcMotor_Back_Set_Run_MillSeconds(150);
            break;
            case KEY_E_FLAG:
                if(accelerator_frontBack > 150){		// 前进模式�?按下E�?启动巡航模式
                    Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_HIGH);
                }else if (accelerator_frontBack < 110){
                    Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
                }
                autoDrive_Set_Mode(AUTO_DRIVE_CLOSE);
            break;
        }


    }

}

void WirelessProtocal_Resolve_Handle(uint8_t *data_m,uint8_t len){
    uint8_t dataLen = data_m[1] - 2;
    uint8_t cmdID = data_m[2];

    switch(cmdID){
		case WIRELESS_CMD_PAIR_RSP:		// 配对成功�?通知主板，陀螺仪校准
			if (pair_wait_rsp_time != 0)		// 在开机发送配对指令后3秒内有效,其它时间收到 认为无效
			{	
				printf("pair rsp gyro \r");
				pair_wait_rsp_time = 0;
			}
			//printf("pair rsp succ \r");
			break;
        case WIRELESS_CMD_ACCELERATOR:{

            
            WirelessProtocal_Accelerator_Resolve(data_m[3],data_m[4],data_m[5]);
            
        }
        break;

        case WIRELESS_CMD_SET_RETURN:{
            autoDrive_Set_ReturnPosition((uint8_t *)&data_m[3]);
        }
        break;

        case WIRELESS_CMD_SET_DESTINATION:{
            autoDrive_Set_FishPosition((uint8_t *)&data_m[3]);
        }
        break;

        case WIRELESS_CMD_SWITCH_AUTO_RETURN:{
            autoDrive_Set_Switch(&data_m[3]);
            //autoDrive_Set_ReturnPosition((uint8_t *)&data_m[4]);
        }
        break;
    }

}

void WirelessProtocal_Accelerator_Resolve(uint8_t accelerator_leftRight,uint8_t accelerator_frontBack,uint8_t key){
    if(autoDrive_Get_Mode()==AUTO_DRIVE_CLOSE){
        WirelessProtoca_Motor_Control(accelerator_leftRight,accelerator_frontBack);
    }
    WirelessProtoca_Key_Control(accelerator_leftRight,accelerator_frontBack,key);
    WirelessProtocal_Accelerator_OutTime_Clear();
}

void WirelessProtocal_Accelerator_OutTime_Handle(void){
    if(wirelessAccelerator_OutTime_Times < 120*100){
        wirelessAccelerator_OutTime_Times ++;

    }else{
    	#if 0
        autoDrive_Set_Mode(AUTO_DRIVE_CLOSE);
        DcMotor_Direction_Set(DIRECTION_FORWARD);
        DcMotor_MRight_PWM_Set(FALSE,0);
        DcMotor_MLeft_PWM_Set(FALSE,0);
		#endif
		printf("enter auto drive!!!\r");
		autoDrive_active();
		wirelessAccelerator_OutTime_Times = 0;
		
    }
	if (wirelessAccelerator_close_motor_time)
	{	
		wirelessAccelerator_close_motor_time --;
		if (wirelessAccelerator_close_motor_time == 0)
		{	
			if(autoDrive_Get_Mode()==AUTO_DRIVE_CLOSE)
		    {   
		    	DcMotor_Direction_Set(DIRECTION_FORWARD);
		        DcMotor_MRight_PWM_Set(FALSE,0);
		        DcMotor_MLeft_PWM_Set(FALSE,0);
				Set_DcMotor_PWM_CruiseControl_Mode(CRUISE_STOP);
			}
		}
	}
	if (pair_wait_rsp_time)
		pair_wait_rsp_time --;
}

void WirelessProtocal_Accelerator_OutTime_Clear(void){
    wirelessAccelerator_OutTime_Times = 0;
	wirelessAccelerator_close_motor_time = 500;		// 5秒收不到数据 如果电机在工作，停止工作
}


uint8_t RF_Pair_Get_Channel(uint8_t *data_m,uint8_t len){
    uint8_t channel = 0;

    channel = ((uint8_t)( ((data_m[3]+0x06)%0x40) + ((data_m[2]>>3)*0x08) + ((data_m[1] | data_m[0])%0x08/2)))%0x40;

    wirelessConfig.RF_Channel[0] = channel;
    wirelessConfig.RF_Channel[1] = channel;
    wirelessConfig.RF_Channel[2] = channel + 0X40;
    printf("RF_Channel 0=%b02x,1=%b02x \r",wirelessConfig.RF_Channel[0],wirelessConfig.RF_Channel[2]);
    return TRUE;
}

uint8_t RF_Pair_Get_Key(uint8_t *data_m,uint8_t len){

    wirelessConfig.RF_Send_Key[0] = (uint8_t)((uint8_t)(data_m[0]<<4)>>4) + ((uint8_t)(data_m[3]>>2) + data_m[3]%0x03);
    wirelessConfig.RF_Send_Key[1] = (uint8_t)((uint8_t)(data_m[1]<<4)>>4) + ((uint8_t)(data_m[2]>>3) + data_m[0]%0x06);
    printf("key1=%b02x,key2=%b02x \r",wirelessConfig.RF_Send_Key[0],wirelessConfig.RF_Send_Key[1]);
    return TRUE;



}

void RF_Pair(uint8_t FreqChannel,uint8_t role){

    uint8_t pairData[4]={0,0,0,0};  //02  02  2a
    pairData[0] = *((unsigned char*) (0xf0 + 4));
    pairData[1] = *((unsigned char*) (0xf0 + 5));
    pairData[2] = *((unsigned char*) (0xf0 + 6));
    pairData[3] = *((unsigned char*) (0xf0 + 7));
    

    Wireless_Send(FreqChannel,WIRELESS_CMD_PAIR,(uint8_t *)&pairData,4,role);


    RF_Pair_Get_Key((uint8_t *)&pairData[0],4);
    RF_Pair_Get_Channel((uint8_t *)&pairData[0],4);
    

    printf("RF_Pair channel=%b02x,key=%b02x,%b02x\r\n",wirelessConfig.RF_Channel[0],wirelessConfig.RF_Send_Key[0],wirelessConfig.RF_Send_Key[1]);

}

void RF_Encrypt_Config(uint8_t role){
  

    LT_WriteReg(7,0,Pair_Channel,role);

    if(role == LT8920_SEND){
        LT_WriteReg(36, wirelessConfig.RF_Send_Key[1], wirelessConfig.RF_Send_Key[1],role);  //同�?�字配置
        LT_WriteReg(39, wirelessConfig.RF_Send_Key[0], wirelessConfig.RF_Send_Key[0],role);	 //同�?�字配置
    }else{
        LT_WriteReg(36, wirelessConfig.RF_Send_Key[0], wirelessConfig.RF_Send_Key[0],role);  //同�?�字配置
        LT_WriteReg(39, wirelessConfig.RF_Send_Key[1], wirelessConfig.RF_Send_Key[1],role);	 //同�?�字配置
		
    }
}

void RF_Close_Channel(uint8_t FreqChannel,uint8_t role){
  LT_WriteReg(7,0,FreqChannel,role);
}

uint8_t autoDrive_in_active(void);

// len1 num1 angel4 jingdu6  weidu6
void RF_Send_Gps_Data(uint8_t FreqChannel,uint8_t role){

    uint8_t gpsData[22]={0};  
    uint8_t index = 0;
    uint16_t tempData_u16 = 0;

    gpsData[index++] = nmea41_Get_GPS_Num();

    tempData_u16 = nmea41_Get_Angel();
    memcpy(&gpsData[index],&tempData_u16,2);
    index += 2;

    gpsData[index++] = nmea41_Get_EW();

    tempData_u16 = nmea41_Get_JingDu1();
    memcpy(&gpsData[index],&tempData_u16,2);
    index += 2;
    // printf("jingdu=%b02x.",tempData_u16);

    tempData_u16 = nmea41_Get_JingDu2();
    memcpy(&gpsData[index],&tempData_u16,2);
    index += 2;
    // printf("%b02x\r\n",tempData_u16);
    gpsData[index++] = nmea41_Get_NS();

    tempData_u16 = nmea41_Get_WeiDu1();
    memcpy(&gpsData[index],&tempData_u16,2);
    index += 2;
    // printf("weidu=%b02x.",tempData_u16);
    tempData_u16 = nmea41_Get_WeiDu2();
    memcpy(&gpsData[index],&tempData_u16,2);
    index += 2;
    // printf("%b02x\r\n",tempData_u16);
    gpsData[index++] = Power_ADC_Get_Level();

    gpsData[index++] = autoDrive_in_active();

	//printf("GPS  autodrive = %bd\r\n",gpsData[index-1]);
	
    Wireless_Send(FreqChannel,WIRELESS_CMD_UPLOAD_GPS,(uint8_t *)&gpsData,index,role);
}

void Wireless_Send(uint8_t FreqChannel,uint8_t cmdID,uint8_t *data_m,uint8_t len,uint8_t role){

    uint8_t wirelessSendData[30]={0};  
    uint8_t index = 0;
    wirelessSendData[index++] = 0xaa;
    wirelessSendData[index++] = 2 + len;
    wirelessSendData[index++] = cmdID;

    memcpy(&wirelessSendData[index],data_m,len);
    index += len;

    wirelessSendData[index++] = cal_checksum_XOR(&wirelessSendData[1],2 + len);
    wirelessSendData[index++] = 0xbb;

    // uprintf_array((uint8_t *)&wirelessSendData,index);
    LT8920_TxData(FreqChannel,(uint8_t *)&wirelessSendData,index,role);
}

void Radio_progress(void)
{
  static uint16_t lt8920_waitTimes = 30,pairSendTimes = 10;
  static uint8_t rf_send_rec_Times = 0;
  if(lt8920_waitTimes >0){
    lt8920_waitTimes--;
    return;
  }
  

  if(pairSendTimes>0){
    pairSendTimes--;
    lt8920_waitTimes = 30;
    RF_Pair(Pair_Channel,LT8920_SEND);

    if(pairSendTimes==0){
      RF_Encrypt_Config(LT8920_SEND);
      RF_Encrypt_Config(LT8920_REC);
		pair_wait_rsp_time = 500;
    }
  }else{
    rf_send_rec_Times++;
    if(rf_send_rec_Times > 80){
        RF_Send_Gps_Data(wirelessConfig.RF_Channel[2],LT8920_SEND);
        rf_send_rec_Times = 0;
		//printf("send gps chn=%b02x \r\n",wirelessConfig.RF_Channel[2]);
    }else{
        RF_Receive(wirelessConfig.RF_Channel[0],LT8920_REC);
    }
    
    // RF_Send_Gps_Data(wirelessConfig.RF_Channel[0],LT8920_SEND);

    // RF_Receive(wirelessConfig.RF_Channel[2],LT8920_REC);
  }
}
// -------


