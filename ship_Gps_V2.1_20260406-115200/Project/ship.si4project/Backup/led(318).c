#include "led.h"
uint8_t shipLedRefreshMode = LED_ON;

void Led_IO_Init(void){
	GPIO_InitTypeDef	GPIO_InitStructure;   
    // red led     
	GPIO_InitStructure.Pin  = GPIO_Pin_4;
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P5,&GPIO_InitStructure);

    // ship led
    GPIO_InitStructure.Pin  = GPIO_Pin_1;
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);
}

void Red_Led_Switch(uint8_t onfoff){
    P54 = onfoff;
}

void Red_Led_Refresh_Handle(void){
    static uint8_t redLedRefreshFlag = 0,redLedRefreshWaitTimes = 0;

    redLedRefreshWaitTimes++;
    if(redLedRefreshWaitTimes<50){
        redLedRefreshWaitTimes++;
        return;
    }
    redLedRefreshWaitTimes = 0;
    
    if(redLedRefreshFlag){
        redLedRefreshFlag = 0;
    }else{
        redLedRefreshFlag = 1;
    }
    Red_Led_Switch(redLedRefreshFlag);

}

// -----
void Ship_Led_Switch(uint8_t onfoff){
    P31 = onfoff;
}

void Ship_Led_Refresh_Handle(void){
    static uint8_t shipLedRefreshWaitTimes = 0,shipLedRefreshFlag = 0;
    shipLedRefreshWaitTimes++;
    if(shipLedRefreshWaitTimes<20){
        shipLedRefreshWaitTimes++;
        return;
    }
    shipLedRefreshWaitTimes = 0;
    
    if (shipLedRefreshMode==LED_ON)
    {
        shipLedRefreshFlag = 1;
    }else if(shipLedRefreshMode==LED_REFRESH){
        if(shipLedRefreshFlag){
        shipLedRefreshFlag = 0;
    }else{
        shipLedRefreshFlag = 1;
    }
    }else{
        shipLedRefreshFlag = 0;
    }
    

    Ship_Led_Switch(shipLedRefreshFlag);

}

void Ship_LED_Set_Handle(void){
    shipLedRefreshMode++;
    if(shipLedRefreshMode > LED_OFF){
        shipLedRefreshMode = LED_ON;
    }
}
