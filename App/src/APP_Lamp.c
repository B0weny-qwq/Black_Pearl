/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#include	"APP_Lamp.h"
#include	"STC32G_GPIO.h"

/***************	Function Description	****************

This module blinks a single LED on P3.6.

Download with 24MHz system clock (configurable in config.h).

******************************************/

//========================================================================
// Function: Lamp_init
// Description: User initialization routine.
// Parameter: None.
// Return: None.
// Version: V1.0, 2020-09-28
//========================================================================
void Lamp_init(void)
{
	P3_MODE_OUT_PP(GPIO_Pin_6);		// P3.6 push-pull output
	P36 = 1;						// default idle level
}

//========================================================================
// Function: Sample_Lamp
// Description: User application routine.
// Parameter: None.
// Return: None.
// Version: V1.0, 2020-09-23
//========================================================================
void Sample_Lamp(void)
{
	P36 = !P36;
}
