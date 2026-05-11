/*---------------------------------------------------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/*---------------------------------------------------------------------*/

#include "config.h"
#include "STC32G_GPIO.h"
#include "STC32G_ADC.h"
#include "STC32G_Exti.h"
#include "STC32G_I2C.h"
#include "STC32G_SPI.h"
#include "STC32G_Timer.h"
#include "STC32G_UART.h"
#include "STC32G_Compare.h"
#include "STC32G_Delay.h"
#include "STC32G_Switch.h"
#include "STC32G_WDT.h"
#include "STC32G_NVIC.h"
#include "app.h"
#include "..\Code_boweny\Function\Log\Log.h"
#include "..\Code_boweny\Function\AHRS\AHRS.h"
#include "..\Code_boweny\Device\QMC6309\QMC6309.h"
#include "..\Code_boweny\Device\QMI8658\QMI8658.h"
#include "..\Code_boweny\Device\GPS\GPS.h"
#include "..\Code_boweny\Device\WIRELESS\wireless.h"
#include "STC32G_Soft_UART.h"

u8 g_qmi8658_ready = 0;

void GPIO_config(void)
{
    P0_MODE_IO_PU(GPIO_Pin_All);
    P0_MODE_IN_HIZ(GPIO_Pin_0);
    P0_DIGIT_IN_DISABLE(GPIO_Pin_0);
    P1_MODE_IN_HIZ(GPIO_Pin_LOW);
    P1_MODE_OUT_OD(GPIO_Pin_4 | GPIO_Pin_5);
    P2_MODE_IO_PU(GPIO_Pin_All);
    P3_MODE_IO_PU(GPIO_Pin_LOW);
    P3_MODE_IO_PU(GPIO_Pin_HIGH);
    P4_MODE_IO_PU(GPIO_Pin_0 | GPIO_Pin_6 | GPIO_Pin_7);
    P6_MODE_IO_PU(GPIO_Pin_All);
    P7_MODE_IO_PU(GPIO_Pin_All);

    P1_PULL_UP_ENABLE(GPIO_Pin_4 | GPIO_Pin_5);
}

void Timer_config(void)
{
    TIM_InitTypeDef TIM_InitStructure;

    TIM_InitStructure.TIM_Mode = TIM_16BitAutoReload;
    TIM_InitStructure.TIM_ClkSource = TIM_CLOCK_1T;
    TIM_InitStructure.TIM_ClkOut = DISABLE;
    TIM_InitStructure.TIM_Value = (u16)(65536UL - (MAIN_Fosc / 1000UL));
    TIM_InitStructure.TIM_PS = 0;
    TIM_InitStructure.TIM_Run = ENABLE;
    Timer_Inilize(Timer0, &TIM_InitStructure);
    NVIC_Timer0_Init(ENABLE, Priority_0);
}

void ADC_config(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    ADC_InitStructure.ADC_SMPduty = 31;
    ADC_InitStructure.ADC_CsSetup = 0;
    ADC_InitStructure.ADC_CsHold = 1;
    ADC_InitStructure.ADC_Speed = ADC_SPEED_2X1T;
    ADC_InitStructure.ADC_AdjResult = ADC_RIGHT_JUSTIFIED;
    ADC_Inilize(&ADC_InitStructure);
    ADC_PowerControl(ENABLE);
    NVIC_ADC_Init(DISABLE, Priority_0);
}

void UART_config(void)
{
    COMx_InitDefine COMx_InitStructure;

    COMx_InitStructure.UART_Mode = UART_8bit_BRTx;
    COMx_InitStructure.UART_BRT_Use = BRT_Timer1;
    COMx_InitStructure.UART_BaudRate = LOG_UART_BAUDRATE;
    COMx_InitStructure.UART_RxEnable = ENABLE;
    COMx_InitStructure.BaudRateDouble = DISABLE;
    UART_Configuration(UART1, &COMx_InitStructure);
    NVIC_UART1_Init(ENABLE, Priority_1);
}

void I2C_config(void)
{
    I2C_InitTypeDef I2C_InitStructure;

    I2C_InitStructure.I2C_Mode = I2C_Mode_Master;
    I2C_InitStructure.I2C_Enable = ENABLE;
    I2C_InitStructure.I2C_MS_WDTA = DISABLE;
    I2C_InitStructure.I2C_Speed = SENSOR_I2C_SPEED_CFG;
    I2C_Init(&I2C_InitStructure);
    NVIC_I2C_Init(I2C_Mode_Master, DISABLE, Priority_0);
}

static void Sensor_I2C_prepare(void)
{
    P1_MODE_OUT_OD(GPIO_Pin_4 | GPIO_Pin_5);
    P1_PULL_UP_ENABLE(GPIO_Pin_4 | GPIO_Pin_5);
    P14 = 1;
    P15 = 1;
    I2C_SW(I2C_P14_P15);
    I2C_config();
}

void SPI_config(void)
{
    SPI_InitTypeDef SPI_InitStructure;

    SPI_InitStructure.SPI_Enable = ENABLE;
    SPI_InitStructure.SPI_SSIG = ENABLE;
    SPI_InitStructure.SPI_FirstBit = SPI_MSB;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Slave;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_Speed = SYSTEM_SPI_SPEED_CFG;
    SPI_Init(&SPI_InitStructure);
    NVIC_SPI_Init(ENABLE, Priority_3);
}

void CMP_config(void)
{
    CMP_InitDefine CMP_InitStructure;

    CMP_InitStructure.CMP_EN = ENABLE;
    CMP_InitStructure.CMP_P_Select = CMP_P_P37;
    CMP_InitStructure.CMP_N_Select = CMP_N_GAP;
    CMP_InitStructure.CMP_InvCMPO = DISABLE;
    CMP_InitStructure.CMP_100nsFilter = ENABLE;
    CMP_InitStructure.CMP_Outpt_En = ENABLE;
    CMP_InitStructure.CMP_OutDelayDuty = 16;
    CMP_Inilize(&CMP_InitStructure);
    NVIC_CMP_Init(RISING_EDGE | FALLING_EDGE, Priority_0);
}

void Exti_config(void)
{
    EXTI_InitTypeDef Exti_InitStructure;

    Exti_InitStructure.EXTI_Mode = EXT_MODE_Fall;
    Ext_Inilize(EXT_INT0, &Exti_InitStructure);
    NVIC_INT0_Init(ENABLE, Priority_0);

    Exti_InitStructure.EXTI_Mode = EXT_MODE_Fall;
    Ext_Inilize(EXT_INT1, &Exti_InitStructure);
    NVIC_INT1_Init(ENABLE, Priority_0);

    NVIC_INT2_Init(ENABLE, NULL);
    NVIC_INT3_Init(ENABLE, NULL);
    NVIC_INT4_Init(ENABLE, NULL);
}

void Switch_config(void)
{
    UART1_SW(UART1_SW_P30_P31);
    UART2_SW(UART2_SW_P10_P11);
    UART3_SW(UART3_SW_P00_P01);
    UART4_SW(UART4_SW_P02_P03);
    I2C_SW(I2C_P14_P15);
    SPI_SW(SPI_P22_P23_P24_P25);

    LCM_CTRL_SW(LCM_CTRL_P45_P44_P42);
    LCM_DATA_SW(LCM_D8_NA_P6);
}

void SYS_Init(void)
{
    EAXSFR();
    GPIO_config();
    Switch_config();
    Timer_config();
    ADC_config();
    UART_config();
    I2C_config();
    EA = 1;

    APP_config();
#if ENABLE_LOG_INIT
    log_init();
#endif

#if ENABLE_WIRELESS_MODULE && ENABLE_LT8920_CHIP
    LOGI("SYS", "wireless stack init start");
    Wireless_Init();
#endif

#if ENABLE_GPS_MODULE
    LOGI("SYS", "gps init start");
    GPS_Init();
#endif

#if ENABLE_MAG_MODULE || ENABLE_IMU_MODULE
    LOGI("SYS", "sensor path init start");
    Sensor_I2C_prepare();
    g_qmi8658_ready = 0;
    AHRS_Reset();
#if ENABLE_MAG_MODULE
    QMC6309_Init();
#endif
#if ENABLE_IMU_MODULE
#if QMI8658_INIT_NONBLOCKING
    QMI8658_RequestReinit();
    g_qmi8658_ready = 0;
#else
    g_qmi8658_ready = (QMI8658_Init() == 0) ? 1U : 0U;
    if (g_qmi8658_ready != 0U) {
        LOGI("SYS", "imu init ready");
    } else {
        LOGE("SYS", "imu init failed");
    }
#endif
#endif
#else
    g_qmi8658_ready = 0;
#endif
}
