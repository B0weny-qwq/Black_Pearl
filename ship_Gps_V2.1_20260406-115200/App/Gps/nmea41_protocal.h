#ifndef _NMEA41_PROTOCAL_H_
#define _NMEA41_PROTOCAL_H_
#include "gps_uart.h"

    typedef struct{
        uint8_t jingdu_EW;
        uint16_t jingdu_Left;
        uint16_t jingdu_Right;
        uint8_t weidu_EW;
        uint16_t weidu_Left;
        uint16_t weidu_Right;
    }GPS_POSITION;

    void nmea41_protocal_Resolve_Handle(uint8_t *data_m,uint16_t length);
    void nmea41_Resolve_VTG(uint8_t *protocal,uint16_t protocal_len);
    void nmea41_Resolve_GGA(uint8_t *protocal,uint16_t protocal_len);
		#ifdef		GPS_DK2		
		void nmea41_Resolve_GPGSV(uint8_t *protocal,uint16_t protocal_len);
		void nmea41_Resolve_BDGSV(uint8_t *protocal,uint16_t protocal_len);
		void nmea41_Resolve_GAGSV(uint8_t *protocal,uint16_t protocal_len);
		#elif		defined(GPS_DK2_RMC)
		void nmea41_Resolve_RMC(uint8_t *protocal,uint16_t protocal_len);
		void nmea41_Resolve_GNGGA(uint8_t *protocal,uint16_t protocal_len);

		void nmea41_Resolve_GNGSA(uint8_t *protocal,uint16_t protocal_len);
		void nmea41_print_Angel_Time(void);
		#else
    void nmea41_Resolve_GSV(uint8_t *protocal,uint16_t protocal_len);
		#endif

    uint16_t nmea41_Get_WeiDu1(void);
    uint16_t nmea41_Get_WeiDu2(void);
    uint16_t nmea41_Get_JingDu1(void);
    uint16_t nmea41_Get_JingDu2(void);

    uint16_t nmea41_Get_Angel(void);
    
    uint8_t nmea41_Get_GPS_Num(void);

    uint8_t nmea41_Get_EW(void);

    uint8_t nmea41_Get_NS(void);
    
    void GPS_Position_Set_Position_Refresh_Flag(uint8_t flag);

    uint8_t GPS_Position_Get_Position_Refresh_Flag(void);
#endif
