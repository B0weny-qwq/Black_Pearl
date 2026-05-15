#ifndef _WIRELESS_PROTOCAL_H_
#define _WIRELESS_PROTOCAL_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_Delay.h"
    #include "STC8G_H_EEPROM.h"

    #include "pwm.h"

    #define PAIR_KEY_ADDRESS   0X0
    #define WIRELESS_PROTOCAL_MAX_LEN 30
    typedef struct{
        uint8_t RF_Channel[3];
        uint8_t RF_Send_Key[2];
    }WIRELESS_CONFIG;
    
    typedef enum  _KEY_SEND_FLAG{
        KEY_A_FLAG = 0xA3,
        KEY_B_FLAG = 0xA5,
        KEY_C_FLAG = 0xA7,
        KEY_D_FLAG = 0xA9,
        KEY_E_FLAG = 0xA1,
        KEY_NULL_FLAG = 0xA0,
    }KEY_SEND_FLAG;
    

    typedef enum _WIRELESS_PROTOCAL_CMD_ID{
		WIRELESS_CMD_PAIR_RSP = 0x0f,
        WIRELESS_CMD_PAIR = 0X10,
        WIRELESS_CMD_ACCELERATOR = 0X11,
        WIRELESS_CMD_UPLOAD_GPS = 0X12,
        WIRELESS_CMD_SET_RETURN = 0X13,
        WIRELESS_CMD_SET_DESTINATION = 0X14,
        WIRELESS_CMD_SWITCH_AUTO_RETURN = 0X15,

    }WIRELESS_PROTOCAL_CMD_ID;

    void autoDrive_Set_ReturnPosition(uint8_t *data_m);
    void autoDrive_Set_FishPosition(uint8_t *data_m);
    void autoDrive_Set_Switch(uint8_t *data_m);
    uint8_t autoDrive_Get_Switch(void);
	void autoDrive_active(void);
	
    void WirelessProtoca_Motor_Control(uint8_t leftRight,uint8_t frontBack);
    void WirelessProtocal_Receive_Handle(uint8_t *data_m,uint8_t len);
    void WirelessProtocal_Resolve_Handle(uint8_t *data_m,uint8_t len);
    void WirelessProtocal_Accelerator_Resolve(uint8_t accelerator_leftRight,uint8_t accelerator_frontBack,uint8_t key);
    void WirelessProtocal_Accelerator_OutTime_Handle(void);
    void WirelessProtocal_Accelerator_OutTime_Clear(void);
    
    void Wireless_Send(uint8_t FreqChannel,uint8_t cmdID,uint8_t *data_m,uint8_t len,uint8_t role);
    void RF_Pair(uint8_t FreqChannel,uint8_t role);


    void RF_Encrypt_Config(uint8_t role);
    void RF_Close_Channel(uint8_t FreqChannel,uint8_t role);


    void RF_Close_Channel(uint8_t FreqChannel,uint8_t role);

    void RF_Send_Gps_Data(uint8_t FreqChannel,uint8_t role);

    void Radio_progress(void);

#endif
