#include "..\..\..\User\Config.h"
#include "QMI8658_port.h"
#include "..\..\..\Driver\inc\STC32G_Delay.h"
#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\..\Driver\inc\STC32G_I2C.h"
#include "..\..\..\Driver\inc\STC32G_NVIC.h"
#include "..\..\..\Driver\inc\STC32G_Switch.h"

static void QMI8658Port_ConfigPins(void)
{
    EAXSFR();
    P1_MODE_OUT_OD(GPIO_Pin_4 | GPIO_Pin_5);
    P1_PULL_UP_ENABLE(GPIO_Pin_4 | GPIO_Pin_5);
    I2C_SW(I2C_P14_P15);
}

static void QMI8658Port_HardI2cRestore(void)
{
    I2C_InitTypeDef i2c_init;

    QMI8658Port_ConfigPins();
    i2c_init.I2C_Mode = I2C_Mode_Master;
    i2c_init.I2C_Enable = ENABLE;
    i2c_init.I2C_MS_WDTA = DISABLE;
    i2c_init.I2C_Speed = SENSOR_I2C_SPEED_CFG;
    I2C_Init(&i2c_init);
    NVIC_I2C_Init(I2C_Mode_Master, DISABLE, Priority_0);
}

#if QMI8658_I2C_USE_SOFT
static void QMI8658Port_SoftEnter(void)
{
    I2C_Function(DISABLE);
    I2C_Master();
    QMI8658Port_ConfigPins();
    P14 = 1;
    P15 = 1;
}

static void QMI8658Port_SoftLeave(void)
{
    QMI8658Port_HardI2cRestore();
}

static void QMI8658Port_SoftDelay(void)
{
    if (QMI8658_SOFT_I2C_DELAY_US != 0U) {
        QMI8658Port_DelayUs(QMI8658_SOFT_I2C_DELAY_US);
    }
}

static void QMI8658Port_SoftStart(void)
{
    P14 = 1;
    P15 = 1;
    QMI8658Port_SoftDelay();
    P14 = 0;
    QMI8658Port_SoftDelay();
    P15 = 0;
    QMI8658Port_SoftDelay();
}

static void QMI8658Port_SoftStop(void)
{
    P14 = 0;
    QMI8658Port_SoftDelay();
    P15 = 1;
    QMI8658Port_SoftDelay();
    P14 = 1;
    QMI8658Port_SoftDelay();
}

static u8 QMI8658Port_SoftWriteByte(u8 value)
{
    u8 bit_mask;

    for (bit_mask = 0x80U; bit_mask != 0U; bit_mask >>= 1) {
        P14 = ((value & bit_mask) != 0U) ? 1 : 0;
        QMI8658Port_SoftDelay();
        P15 = 1;
        QMI8658Port_SoftDelay();
        P15 = 0;
        QMI8658Port_SoftDelay();
    }

    P14 = 1;
    QMI8658Port_SoftDelay();
    P15 = 1;
    QMI8658Port_SoftDelay();
    bit_mask = (P14 != 0U) ? 1U : 0U;
    P15 = 0;
    QMI8658Port_SoftDelay();
    return bit_mask;
}

static u8 QMI8658Port_SoftReadByte(u8 send_ack)
{
    u8 value;
    u8 i;

    value = 0U;
    P14 = 1;
    for (i = 0; i < 8U; i++) {
        value <<= 1;
        P15 = 1;
        QMI8658Port_SoftDelay();
        if (P14 != 0U) {
            value |= 0x01U;
        }
        P15 = 0;
        QMI8658Port_SoftDelay();
    }

    P14 = (send_ack != 0U) ? 0 : 1;
    QMI8658Port_SoftDelay();
    P15 = 1;
    QMI8658Port_SoftDelay();
    P15 = 0;
    QMI8658Port_SoftDelay();
    P14 = 1;
    return value;
}
#endif

s8 QMI8658Port_Init(void)
{
    QMI8658Port_ConfigPins();
    QMI8658Port_HardI2cRestore();
    return SUCCESS;
}

char *QMI8658Port_BackendName(void)
{
#if QMI8658_I2C_USE_SOFT
    return "soft";
#else
    return "hard";
#endif
}

void QMI8658Port_DelayMs(u16 ms)
{
    delay_ms(ms);
}

void QMI8658Port_DelayUs(u16 us)
{
    u8 dly;

    while (us > 0U) {
        dly = (u8)(MAIN_Fosc / 2000000UL);
        while (--dly) {
        }
        us--;
    }
}

u8 QMI8658Port_BusNeedsRecover(void)
{
    if (Get_MSBusy_Status()) {
        return 1U;
    }
    if (P14 == 0) {
        return 1U;
    }
    if (P15 == 0) {
        return 1U;
    }
    return 0U;
}

void QMI8658Port_BusRecover(void)
{
    u8 i;

    I2C_Function(DISABLE);
    I2C_Master();
    QMI8658Port_ConfigPins();

    P14 = 1;
    P15 = 1;
    QMI8658Port_DelayUs(5U);
    for (i = 0; i < 9U; i++) {
        P15 = 0;
        QMI8658Port_DelayUs(5U);
        P15 = 1;
        QMI8658Port_DelayUs(5U);
        if (P14 != 0U) {
            break;
        }
    }

    P14 = 0;
    QMI8658Port_DelayUs(5U);
    P15 = 1;
    QMI8658Port_DelayUs(5U);
    P14 = 1;
    QMI8658Port_DelayUs(5U);

    QMI8658Port_HardI2cRestore();
}

u8 QMI8658Port_WriteReg(u8 addr, u8 reg_addr, u8 reg_val, u8 *err_code)
{
    if (err_code != NULL) {
        *err_code = QMI8658_I2C_OK;
    }

    if (QMI8658Port_BusNeedsRecover() != 0U) {
        QMI8658Port_BusRecover();
        if (QMI8658Port_BusNeedsRecover() != 0U) {
            if (err_code != NULL) {
                *err_code = QMI8658_I2C_ERR_BUSY;
            }
            return 1U;
        }
    }

#if QMI8658_I2C_USE_SOFT
    QMI8658Port_SoftEnter();
    QMI8658Port_SoftStart();
    if (QMI8658Port_SoftWriteByte(QMI8658_I2C_WRITE(addr)) != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_DEVW_NACK;
        }
        QMI8658Port_SoftStop();
        QMI8658Port_SoftLeave();
        return 1U;
    }
    if (QMI8658Port_SoftWriteByte(reg_addr) != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_REG_NACK;
        }
        QMI8658Port_SoftStop();
        QMI8658Port_SoftLeave();
        return 1U;
    }
    if (QMI8658Port_SoftWriteByte(reg_val) != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_DATA_NACK;
        }
        QMI8658Port_SoftStop();
        QMI8658Port_SoftLeave();
        return 1U;
    }
    QMI8658Port_SoftStop();
    QMI8658Port_SoftLeave();
    return 0U;
#else
    I2C_WriteNbyte(QMI8658_I2C_WRITE(addr), reg_addr, &reg_val, 1U);
    if (Get_MSBusy_Status() != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_BUSY;
        }
        return 1U;
    }
    return 0U;
#endif
}

u8 QMI8658Port_ReadN(u8 addr, u8 start_reg, u8 *buf, u8 len, u8 *err_code)
{
#if QMI8658_I2C_USE_SOFT
    u8 i;
#endif

    if ((buf == NULL) || (len == 0U)) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_PARAM;
        }
        return 1U;
    }

    if (err_code != NULL) {
        *err_code = QMI8658_I2C_OK;
    }

    if (QMI8658Port_BusNeedsRecover() != 0U) {
        QMI8658Port_BusRecover();
        if (QMI8658Port_BusNeedsRecover() != 0U) {
            if (err_code != NULL) {
                *err_code = QMI8658_I2C_ERR_BUSY;
            }
            return 1U;
        }
    }

#if QMI8658_I2C_USE_SOFT
    QMI8658Port_SoftEnter();
    QMI8658Port_SoftStart();
    if (QMI8658Port_SoftWriteByte(QMI8658_I2C_WRITE(addr)) != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_DEVW_NACK;
        }
        QMI8658Port_SoftStop();
        QMI8658Port_SoftLeave();
        return 1U;
    }
    if (QMI8658Port_SoftWriteByte(start_reg) != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_REG_NACK;
        }
        QMI8658Port_SoftStop();
        QMI8658Port_SoftLeave();
        return 1U;
    }
    QMI8658Port_SoftStart();
    if (QMI8658Port_SoftWriteByte(QMI8658_I2C_READ(addr)) != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_DEVR_NACK;
        }
        QMI8658Port_SoftStop();
        QMI8658Port_SoftLeave();
        return 1U;
    }
    for (i = 0U; i < len; i++) {
        buf[i] = QMI8658Port_SoftReadByte(((u8)(i + 1U) < len) ? 1U : 0U);
    }
    QMI8658Port_SoftStop();
    QMI8658Port_SoftLeave();
    return 0U;
#else
    I2C_ReadNbyte(QMI8658_I2C_WRITE(addr), start_reg, buf, len);
    if (Get_MSBusy_Status() != 0U) {
        if (err_code != NULL) {
            *err_code = QMI8658_I2C_ERR_BUSY;
        }
        return 1U;
    }
    return 0U;
#endif
}
