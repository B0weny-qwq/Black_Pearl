#ifndef _DCMOTOR_GPIO_H_
#define _DCMOTOR_GPIO_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"

    void DcMotor_Gpio_Config(void);

    void DcMotor_FRONT_Set(uint8_t onoff);

    void DcMotor_BACK_Set(uint8_t onoff);

    void DcMotor_Front_Set_Run_MillSeconds(uint16_t millSeconds);

    void DcMotor_Back_Set_Run_MillSeconds(uint16_t millSeconds);

    void DcMotor_Handle(void); 
#endif
