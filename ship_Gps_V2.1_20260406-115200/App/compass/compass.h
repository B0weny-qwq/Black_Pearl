#ifndef _COMPASS_H_
#define _COMPASS_H_
    #include "config.h"
    #include <math.h>
    #include "qmc5833.h"

    void Compass_Init(void);
    int16_t Compass_Get_Azimuth(void);
    void Compass_Read_XYZ(void);
    void Compass_Get_Angel_Handle(void);
	float compass_get_angle(void);
	void Compass_calib_handle(void);
	void Compass_calib_start(void);
	
#endif
