#ifndef _GPS_UART_H_
#define _GPS_UART_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_Delay.h"
    #include "STC8G_H_UART.h"
    #include "STC8G_H_Switch.h"

    #define GPS_UART_RX_BUF_SIZE 100
    #define GPS_RX_CACHE_SIZE 200

	
    typedef struct
    {
    	uint8_t wline,rline;
		uint16_t resolveFlag, save_index;
        uint8_t  rx_buffer[8][GPS_UART_RX_BUF_SIZE + 1];
        
    }UART_GPS_CACHE;

    void Gps_Uart_Gpio_Init(void);

    void Gps_Uart_Init(void);

    void Gps_Uart_Send_Buff(uint8_t *data_m,uint8_t len);

    void Gps_Uart_Data_Resolve(void);

    uint8_t nmea41_get_checkNum(uint8_t *data_m,uint16_t data_length);

    uint16_t Nmea41_Uart_Buffer_Read (uint8_t* buffer, uint16_t size);
    
    void nmea41_UART_RX_ISR(uint8_t rx_data);

    uint16_t Nmea41_Uart_Buffer_Read (uint8_t* buffer, uint16_t size);

#endif
