#include "wireless_port.h"

#include "..\..\..\Driver\inc\STC32G_Delay.h"
#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\..\Driver\inc\STC32G_NVIC.h"
#include "..\..\..\Driver\inc\STC32G_SPI.h"
#include "..\..\..\Driver\inc\STC32G_Switch.h"

static u8 g_wireless_port_initialized = 0U;

s8 WirelessPort_Init(void)
{
    SPI_InitTypeDef spi_init;

    EAXSFR();

    P1_MODE_OUT_PP(GPIO_Pin_3);
    P1_PULL_UP_DISABLE(GPIO_Pin_3);
    P1_SPEED_HIGH(GPIO_Pin_3);

    P3_MODE_OUT_PP(GPIO_Pin_2 | GPIO_Pin_4 | GPIO_Pin_5);
    P3_MODE_IN_HIZ(GPIO_Pin_3);
    P3_PULL_UP_ENABLE(GPIO_Pin_3);
    P3_SPEED_HIGH(GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5);

    P5_MODE_OUT_PP(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4);
    P5_PULL_UP_DISABLE(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4);
    P5_SPEED_HIGH(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4);

    WirelessPort_SetCs(1U);
    WirelessPort_SetRst(1U);
    WirelessPort_SetTxEn(0U);
    WirelessPort_SetRxEn(0U);
    WirelessPort_SetAntSel(WIRELESS_PORT_ANT1);

    SPI_SW(SPI_P35_P34_P33_P32);

    spi_init.SPI_Enable = ENABLE;
    spi_init.SPI_SSIG = ENABLE;
    spi_init.SPI_FirstBit = SPI_MSB;
    spi_init.SPI_Mode = SPI_Mode_Master;
    spi_init.SPI_CPOL = SPI_CPOL_Low;
    spi_init.SPI_CPHA = SPI_CPHA_2Edge;
    spi_init.SPI_Speed = SPI_Speed_16;
    SPI_Init(&spi_init);
    NVIC_SPI_Init(DISABLE, Priority_0);

    g_wireless_port_initialized = 1U;
    return SUCCESS;
}

s8 WirelessPort_Deinit(void)
{
    if (!g_wireless_port_initialized) {
        return SUCCESS;
    }

    WirelessPort_SetTxEn(0U);
    WirelessPort_SetRxEn(0U);
    WirelessPort_SetCs(1U);
    g_wireless_port_initialized = 0U;
    return SUCCESS;
}

void WirelessPort_SetCs(u8 level)
{
    P35 = (level != 0U) ? 1 : 0;
}

void WirelessPort_SetRst(u8 level)
{
    P50 = (level != 0U) ? 1 : 0;
}

void WirelessPort_SetAntSel(u8 ant_sel)
{
    P51 = (ant_sel == WIRELESS_PORT_ANT2) ? 1 : 0;
}

void WirelessPort_SetRxEn(u8 level)
{
    P13 = (level != 0U) ? 1 : 0;
}

void WirelessPort_SetTxEn(u8 level)
{
    P54 = (level != 0U) ? 1 : 0;
}

void WirelessPort_DelayMs(u16 ms)
{
    delay_ms(ms);
}

void WirelessPort_DelayUs(u16 us)
{
    u8 dly;

    while (us > 0U) {
        dly = (u8)(MAIN_Fosc / 2000000UL);
        while (--dly) {
        }
        us--;
    }
}

u8 WirelessPort_SpiTransfer(u8 value)
{
    SPDAT = value;
    while (SPIF == 0) {
    }
    SPI_ClearFlag();
    return SPDAT;
}
