#ifndef _UTILS_PRINTF_H_
#define _UTILS_PRINTF_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_Delay.h"

    #define PRINT_BOUND_DELAY	6		//104us 9600bps; 52us 19200; 26us 38400 8.7us 115200
    //#define DEBUG_SWITCH        TRUE
    #define DEBUG_SWITCH        FALSE

    void Gpio_Print_Init(void);
    void uprintf_array(uint8_t *arraydata,uint16_t len);
#endif
