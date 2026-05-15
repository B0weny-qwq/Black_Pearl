#include "KalmanFiltering.h"


    
//2.定义卡尔曼结构体参数并初始化
KFPTypeS kfpVar = 
{
    0.001, //估算协方差. 初始化值为 0.02
    0, //卡尔曼增益. 初始化值为 0
    0.003, //过程噪声协方差,Q增大，动态响应变快，收敛稳定性变坏. 初始化值为 0.001
    0.5, //测量噪声协方差,R增大，动态响应变慢，收敛稳定性变好. 初始化值为 1
    0 //卡尔曼滤波器输出. 初始化值为 0
};
/**
  ******************************************************************************
  * @brief  卡尔曼滤波器 函数
  * @param  *kfp    - 卡尔曼结构体参数
  * @param  input   - 需要滤波的参数的测量值（即传感器的采集值）
  * @return 卡尔曼滤波器输出值（最优值）
  * @note   
  ******************************************************************************
  */
float KalmanFilter(KFPTypeS *kfp, float input)
{
    //估算协方差方程：当前 估算协方差 = 上次更新 协方差 + 过程噪声协方差
    kfp->P = kfp->P + kfp->Q;
 
    //卡尔曼增益方程：当前 卡尔曼增益 = 当前 估算协方差 / （当前 估算协方差 + 测量噪声协方差）
    kfp->G = kfp->P / (kfp->P + kfp->R);
 
    //更新最优值方程：当前 最优值 = 当前 估算值 + 卡尔曼增益 * （当前 测量值 - 当前 估算值）
    kfp->Output = kfp->Output + kfp->G * (input - kfp->Output); //当前 估算值 = 上次 最优值
 
    //更新 协方差 = （1 - 卡尔曼增益） * 当前 估算协方差。
    kfp->P = (1 - kfp->G) * kfp->P;
 
    return kfp->Output;
}
 
/**
  ******************************************************************************
  * @brief  主函数
  * @param  None
  * @return None
  * @note   
  ******************************************************************************
  */
int16_t Kalman_Get_Output(int16_t data_m){
    return (int16_t)KalmanFilter(&kfpVar, data_m);
}

// void main(void)
// {
//     for(int i=0; i<TEST_PULSE_BUF_LEN; i++)
//     {
//         printf("%4d\r\n", (int)KalmanFilter(&kfpVar, testPulseBuf[i])); //打印
//     }
// }