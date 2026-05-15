#ifndef _KALMANFILTERING_H_
#define _KALMANFILTERING_H_

    #include <math.h>
    #include "config.h"
    #include "STC8G_H_Soft_I2C.h"
    #include "STC8G_H_Delay.h"


    //1. 结构体类型定义
    typedef struct 
    {
        float P; //估算协方差
        float G; //卡尔曼增益
        float Q; //过程噪声协方差,Q增大，动态响应变快，收敛稳定性变坏
        float R; //测量噪声协方差,R增大，动态响应变慢，收敛稳定性变好
        float Output; //卡尔曼滤波器输出 
    }KFPTypeS; //Kalman Filter parameter type Struct
    

    int16_t Kalman_Get_Output(int16_t data_m);
    


#endif
