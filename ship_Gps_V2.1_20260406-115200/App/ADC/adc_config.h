#ifndef _ADC_CONFIG_H_
#define _ADC_CONFIG_H_
    #include "config.h"
    #include "STC8G_H_GPIO.h"
    #include "STC8G_H_ADC.h"
    #include "STC8G_H_NVIC.h"

    void ADC_Config(void);

    void NrefCheck(void);

    uint16_t Adc_Get_Nrf(void);

#endif
