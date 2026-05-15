#ifndef _AUTO_DRIVE_H_
#define _AUTO_DRIVE_H_
    #include "config.h"

    #include "wirelessProtocal.h"
    #include "utils_printf.h"
    #include "pwm.h"
    #include "DcMotor_Gpio.h"
    #include "utils_checkSum.h"
    #include "nmea41_protocal.h"

    #define GET_DIRECTION_TIME 200  //2ï¿?
    #define PI 3.14159265
    typedef enum{
        AUTO_DRIVE_IDLE = 0,
        AUTO_DRIVE_START,
        AUTO_DRIVE_GET_DIRECTION,
        AUTO_DRIVE_RUNING,
    }AUTO_DRIVE_STATE;

    typedef enum{
        AUTO_DRIVE_CLOSE = 0,
        AUTO_DRIVE_GO_FISISH_POSITION,
        AUTO_DRIVE_GO_HOME_POSITION,
    }AUTO_DRIVE_MODE;

    typedef enum{
        POSITION_NORTH = 1,
        POSITION_EAST_NORTH = 2,
        POSITION_EAST = 3,
        POSITION_EAST_SOUTH = 4,
        POSITION_SOUTH = 5,
        POSITION_WEST_SOUTH = 6,
        POSITION_WEST = 7,
        POSITION_WEST_NORTH = 8,
    }POSITION_DIRECTION;

	extern uint8_t idata autoDrive_State;
	
	void autodrv_init(void);
	
    void autoDrive_Set_Mode(uint8_t mode);
    uint8_t autoDrive_Get_Mode(void);
    uint8_t autoDrive_Get_Mode(void);
    void autoDrive_Set_ReturnPosition(uint8_t *data_m);
    void autoDrive_Set_FishPosition(uint8_t *data_m);
    void autoDrive_Set_Turn_Times(uint16_t times);
    uint16_t autoDrive_Get_Turn_Times(void);
    uint16_t autoDrive_Get_North_Angel(uint8_t direction,uint8_t angel);
    uint8_t autoDrive_Get_Direction_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData);
    uint16_t autoDrive_Get_Angel_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData);
    uint16_t autoDrive_Get_Distance_NowPosition_Destination(uint8_t *nowpositionData,uint8_t *despositionData);
    void autoDrive_Turn_Handle(void);
    void autoDrive_Handle(void);
    void Gps_Return_Test_Handle(void);
#endif
