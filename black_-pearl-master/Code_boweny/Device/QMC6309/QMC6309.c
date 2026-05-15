/**
 * @file    QMC6309.c
 * @brief   QMC6309 3轴地磁计驱动实现
 * @author  boweny
 * @date    2026-04-22
 * @version v1.1
 *
 * @details
 * - 实现 QMC6309 3轴地磁计的初始化、原始数据读取、ODR 配置与寄存器调试
 * - 支持 I2C 地址探测、软复位、上电就绪检测、总线恢复与数据有效性检查
 * - 保留原始 XYZ 读取接口，并新增软件低通后的滤波读取接口
 * - 导出函数：QMC6309_Init() / QMC6309_ReadXYZ() / QMC6309_ReadXYZFiltered() /
 *             QMC6309_ReadID() / QMC6309_SetODR() / QMC6309_Wait_Ready() /
 *             QMC6309_DumpRegs()
 *
 * @hardware
 *   - I2C接口: P1.4(SDA) / P1.5(SCL)，硬件I2C
 *   - 系统时钟: Fosc = 24MHz
 *
 * @note    必须在 I2C_config() 初始化完成后调用 QMC6309_Init()
 * @note    禁止使用浮点运算，原始数据与滤波数据均以 int16 传递
 *
 * @see     Code_boweny/Device/QMC6309/QMC6309.h
 */

#include "QMC6309.h"
#include "STC32G_GPIO.h"
#include "STC32G_I2C.h"
#include "Filter.h"
#include "Log.h"

#define  QMC6309_I2C_WRITE(addr)        ((addr) << 1)
#define  QMC6309_I2C_READ(addr)         (((addr) << 1) | 1)

#define  QMC6309_REG_CHIP_ID            0x00
#define  QMC6309_REG_DATA_START         0x01
#define  QMC6309_REG_CONTROL_1          0x0A
#define  QMC6309_REG_CONTROL_2          0x0B

#define  QMC6309_CTRL1_STANDBY          0x00
#define  QMC6309_CTRL1_ACTIVE           0x1B
#define  QMC6309_CTRL2_INIT             0x10

#define  QMC6309_PWR_UP_DELAY_MS        1000
#define  QMC6309_MSACKI_MASK            0x02
#define  QMC6309_BUS_RECOVER_PULSES     9
#define  QMC6309_BUS_RECOVER_DELAY_MS   1

static u8  qmc6309_addr = QMC6309_I2C_ADDR_PRIMARY;

static s8   QMC6309_WriteReg(u8 reg, u8 dat);
static u8   QMC6309_ReadReg(u8 reg);
static s8   QMC6309_Reset(void);
static void QMC6309_Delay_ms(u16 ms);
static void QMC6309_BusReset(void);
static void QMC6309_RestoreI2CMaster(void);
static u8   QMC6309_ProbeAddr(u8 addr);
static u8   QMC6309_RecvAck(void);
static u8   QMC6309_EnsureBusIdle(void);
static u8   QMC6309_BusNeedsRecover(void);

extern u8   Get_MSBusy_Status(void);
extern void Start(void);
extern void SendData(char dat);
extern void RecvACK(void);
extern char RecvData(void);
extern void SendACK(void);
extern void SendNAK(void);
extern void Stop(void);

static u8 QMC6309_RecvAck(void)
{
    RecvACK();
    return ((I2CMSST & QMC6309_MSACKI_MASK) != 0);
}

static u8 QMC6309_BusNeedsRecover(void)
{
    if (Get_MSBusy_Status()) {
        return 1;
    }

    if (P14 == 0) {
        return 1;
    }

    if (P15 == 0) {
        return 1;
    }

    return 0;
}

static u8 QMC6309_EnsureBusIdle(void)
{
    if (!QMC6309_BusNeedsRecover()) {
        return 0;
    }

    LOGW("MAG", "bus not idle, try recover busy=%s sda=%s scl=%s",
         Get_MSBusy_Status() ? "Y" : "N",
         (P14 == 0) ? "L" : "H",
         (P15 == 0) ? "L" : "H");
    QMC6309_BusReset();
    if (QMC6309_BusNeedsRecover()) {
        LOGE("MAG", "bus recover failed busy=%s sda=%s scl=%s",
             Get_MSBusy_Status() ? "Y" : "N",
             (P14 == 0) ? "L" : "H",
             (P15 == 0) ? "L" : "H");
        return 1;
    }

    return 0;
}

static s8 QMC6309_WriteReg(u8 reg, u8 dat)
{
    u8 dev_addr;

    dev_addr = QMC6309_I2C_WRITE(qmc6309_addr);
    if (QMC6309_EnsureBusIdle()) {
        LOGW("MAG", "WriteReg bus busy reg=0x%02X", reg);
        return -1;
    }

    Start();
    SendData(dev_addr);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "WriteReg dev NACK dev=0x%02X reg=0x%02X", dev_addr, reg);
        return -1;
    }

    SendData(reg);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "WriteReg reg NACK dev=0x%02X reg=0x%02X", dev_addr, reg);
        return -1;
    }

    SendData(dat);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "WriteReg data NACK reg=0x%02X val=0x%02X", reg, dat);
        return -1;
    }

    Stop();
    LOGI("MAG", "WriteReg OK reg=0x%02X val=0x%02X", reg, dat);
    return 0;
}

static u8 QMC6309_ReadReg(u8 reg)
{
    u8 dev_addr_w;
    u8 dev_addr_r;
    u8 reg_value;

    dev_addr_w = QMC6309_I2C_WRITE(qmc6309_addr);
    dev_addr_r = QMC6309_I2C_READ(qmc6309_addr);
    reg_value = 0xFF;

    if (QMC6309_EnsureBusIdle()) {
        LOGW("MAG", "ReadReg bus busy reg=0x%02X", reg);
        return 0xFF;
    }

    Start();
    SendData(dev_addr_w);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "ReadReg devW NACK dev=0x%02X reg=0x%02X", dev_addr_w, reg);
        return 0xFF;
    }

    SendData(reg);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "ReadReg reg NACK dev=0x%02X reg=0x%02X", dev_addr_w, reg);
        return 0xFF;
    }

    Start();
    SendData(dev_addr_r);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "ReadReg devR NACK dev=0x%02X reg=0x%02X", dev_addr_r, reg);
        return 0xFF;
    }

    reg_value = (u8)RecvData();
    SendNAK();
    Stop();

    return reg_value;
}

static void QMC6309_Delay_ms(u16 ms)
{
    u16 i;

    while (ms--) {
        for (i = 0; i < 1000; i++) {
            _nop_();
        }
    }
}

static void QMC6309_RestoreI2CMaster(void)
{
    I2C_Function(DISABLE);
    I2C_Master();
    I2CMSST = 0x00;
    I2CMSCR = 0x00;
    I2C_SetSpeed(QMC6309_I2C_SPEED_CFG);
    I2C_WDTA_DIS();
    I2C_Function(ENABLE);
}

static void QMC6309_BusReset(void)
{
    u8 i;
    u8 sda_gpio_high;
    u8 scl_gpio_high;
    u16 pulse_count;

    LOGW("MAG", "bus recover start");

    I2C_Function(DISABLE);
    I2C_Master();

    P1_MODE_OUT_OD(GPIO_Pin_4 | GPIO_Pin_5);
    P1_PULL_UP_ENABLE(GPIO_Pin_4 | GPIO_Pin_5);

    P14 = 1;
    P15 = 1;
    QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);

    pulse_count = 0;
    for (i = 0; i < QMC6309_BUS_RECOVER_PULSES; i++) {
        if ((P14 == 1) && (P15 == 1)) {
            break;
        }

        P15 = 0;
        QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);
        P15 = 1;
        QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);
        pulse_count++;
    }

    P14 = 0;
    QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);
    P15 = 1;
    QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);
    P14 = 1;
    QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);

    sda_gpio_high = (P14 == 0) ? 0 : 1;
    scl_gpio_high = (P15 == 0) ? 0 : 1;
    LOGI("MAG", "bus recover gpio sda=%s scl=%s pulses=%u",
         sda_gpio_high ? "H" : "L",
         scl_gpio_high ? "H" : "L",
         pulse_count);

    QMC6309_RestoreI2CMaster();
    QMC6309_Delay_ms(QMC6309_BUS_RECOVER_DELAY_MS);

    LOGI("MAG", "bus recover done busy=%s sda=%s scl=%s",
         Get_MSBusy_Status() ? "Y" : "N",
         (P14 == 0) ? "L" : "H",
         (P15 == 0) ? "L" : "H");
}

static u8 QMC6309_ProbeAddr(u8 addr)
{
    u8 dev_addr;

    dev_addr = QMC6309_I2C_WRITE(addr);
    if (QMC6309_EnsureBusIdle()) {
        LOGW("MAG", "Probe bus busy addr=0x%02X", addr);
        return 1;
    }

    Start();
    SendData(dev_addr);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGI("MAG", "Probe addr=0x%02X write=0x%02X: NACK", addr, dev_addr);
        return 1;
    }

    Stop();
    qmc6309_addr = addr;
    LOGI("MAG", "Probe addr=0x%02X write=0x%02X: ACK", addr, dev_addr);
    return 0;
}

static s8 QMC6309_Reset(void)
{
    if (QMC6309_WriteReg(QMC6309_REG_CONTROL_2, 0x80) != 0) {
        LOGE("MAG", "soft reset enter failed");
        return -1;
    }
    QMC6309_Delay_ms(5);

    if (QMC6309_WriteReg(QMC6309_REG_CONTROL_2, 0x00) != 0) {
        LOGE("MAG", "soft reset exit failed");
        return -1;
    }
    QMC6309_Delay_ms(10);
    return 0;
}

s8 QMC6309_Wait_Ready(u16 timeout_ms)
{
    u16 i;
    u8 id;

    for (i = 0; i < timeout_ms; i += 10) {
        id = QMC6309_ReadReg(QMC6309_REG_CHIP_ID);
        if (id == QMC6309_CHIP_ID_VALUE) {
            LOGI("MAG", "ready id=0x%02X in %ums", id, i);
            return 0;
        }
        QMC6309_Delay_ms(10);
    }

    LOGE("MAG", "wait ready TIMEOUT");
    return -1;
}

u8 QMC6309_ReadID(void)
{
    u8 id;

    id = QMC6309_ReadReg(QMC6309_REG_CHIP_ID);
    if (id != QMC6309_CHIP_ID_VALUE) {
        LOGE("MAG", "ID mismatch got=0x%02X exp=0x%02X", id, QMC6309_CHIP_ID_VALUE);
    }
    return id;
}

void QMC6309_DumpRegs(u8 target_addr)
{
    u8 old_addr;
    u8 reg00;
    u8 reg0A;
    u8 reg0B;

    old_addr = qmc6309_addr;
    qmc6309_addr = target_addr;

    reg00 = QMC6309_ReadReg(QMC6309_REG_CHIP_ID);
    reg0A = QMC6309_ReadReg(QMC6309_REG_CONTROL_1);
    reg0B = QMC6309_ReadReg(QMC6309_REG_CONTROL_2);

    LOGI("MAG", "Dump addr=0x%02X write=0x%02X read=0x%02X reg00=0x%02X reg0A=0x%02X reg0B=0x%02X",
         target_addr,
         QMC6309_I2C_WRITE(target_addr),
         QMC6309_I2C_READ(target_addr),
         reg00,
         reg0A,
         reg0B);

    qmc6309_addr = old_addr;
}

s8 QMC6309_ReadXYZ(int16 *x, int16 *y, int16 *z)
{
    u8 dev_addr_w;
    u8 dev_addr_r;
    u8 raw[6];

    dev_addr_w = QMC6309_I2C_WRITE(qmc6309_addr);
    dev_addr_r = QMC6309_I2C_READ(qmc6309_addr);

    if (QMC6309_EnsureBusIdle()) {
        LOGW("MAG", "ReadXYZ bus busy");
        return -1;
    }

    Start();
    SendData(dev_addr_w);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "ReadXYZ devW NACK dev=0x%02X", dev_addr_w);
        return -1;
    }

    SendData(QMC6309_REG_DATA_START);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "ReadXYZ reg NACK reg=0x%02X", QMC6309_REG_DATA_START);
        return -1;
    }

    Start();
    SendData(dev_addr_r);
    if (QMC6309_RecvAck()) {
        Stop();
        LOGW("MAG", "ReadXYZ devR NACK dev=0x%02X", dev_addr_r);
        return -1;
    }

    raw[0] = (u8)RecvData(); SendACK();
    raw[1] = (u8)RecvData(); SendACK();
    raw[2] = (u8)RecvData(); SendACK();
    raw[3] = (u8)RecvData(); SendACK();
    raw[4] = (u8)RecvData(); SendACK();
    raw[5] = (u8)RecvData(); SendNAK();
    Stop();

    *x = (int16)((u16)raw[1] << 8 | raw[0]);
    *y = (int16)((u16)raw[3] << 8 | raw[2]);
    *z = (int16)((u16)raw[5] << 8 | raw[4]);

    LOGD("MAG", "XYZ: X=%d Y=%d Z=%d", *x, *y, *z);

    if ((raw[0] == 0xFF) && (raw[1] == 0xFF) &&
        (raw[2] == 0xFF) && (raw[3] == 0xFF) &&
        (raw[4] == 0xFF) && (raw[5] == 0xFF)) {
        LOGW("MAG", "XYZ all 0xFF, data invalid");
        return -1;
    }

    if ((*x == 0) && (*y == 0) && (*z == 0)) {
        LOGW("MAG", "XYZ all zero, data may be invalid");
        return -1;
    }

    return 0;
}

/**
 * @brief      读取低通滤波后的地磁 XYZ 数据
 * @param[out] x  指向 X 轴滤波后磁场数据的指针
 * @param[out] y  指向 Y 轴滤波后磁场数据的指针
 * @param[out] z  指向 Z 轴滤波后磁场数据的指针
 * @return      0=成功，-1=原始数据无效、滤波失败或读取失败
 *
 * @details
 * 先读取原始三轴地磁数据，再通过 Function/Filter 模块执行软件低通滤波。
 * 首帧有效数据直接作为滤波输出，避免上电瞬态影响。
 */
s8 QMC6309_ReadXYZFiltered(int16 *x, int16 *y, int16 *z)
{
    int16 mx;
    int16 my;
    int16 mz;

    if ((x == 0) || (y == 0) || (z == 0)) {
        return -1;
    }

    if (QMC6309_ReadXYZ(&mx, &my, &mz) != 0) {
        return -1;
    }

    return Filter_MagLowPass(mx, my, mz, x, y, z);
}

s8 QMC6309_SetODR(u8 odr)
{
    if (QMC6309_WriteReg(QMC6309_REG_CONTROL_2, odr) != 0) {
        LOGE("MAG", "SetODR WR fail odr=0x%02X", odr);
        return -1;
    }

    LOGI("MAG", "SetODR=0x%02X", odr);
    return 0;
}

s8 QMC6309_Init(void)
{
    u8 id;
    u8 retry;
    u8 ctrl1_readback;
    u8 ctrl2_readback;
    u16 speed_cfg;
    u32 bus_hz;
    u16 retry_num;

    speed_cfg = (u16)QMC6309_I2C_SPEED_CFG;
    bus_hz = ((u32)MAIN_Fosc / 2UL) /
             ((((u32)speed_cfg) * 2UL) + 4UL);

    LOGI("MAG", "========== QMC6309 Init Start ==========");
    LOGI("MAG", "I2C route=P1.4/P1.5 speed_cfg=%u (~%luHz)", speed_cfg, bus_hz);
    LOGI("MAG", "probe order primary=0x%02X alt=0x%02X",
         QMC6309_I2C_ADDR_PRIMARY, QMC6309_I2C_ADDR_ALT);

    if (QMC6309_EnsureBusIdle()) {
        LOGE("MAG", "bus idle check failed before probe");
        return -1;
    }

    for (retry = 0; retry < 3; retry++) {
        retry_num = (u16)retry + 1U;
        if (QMC6309_ProbeAddr(QMC6309_I2C_ADDR_PRIMARY) == 0) {
            break;
        }

        LOGW("MAG", "primary addr no ACK, try alt");
        if (QMC6309_ProbeAddr(QMC6309_I2C_ADDR_ALT) == 0) {
            break;
        }

        LOGW("MAG", "addr probe fail try=%u, wait 200ms retry", retry_num);
        QMC6309_Delay_ms(200);
    }

    if (retry >= 3) {
        LOGE("MAG", "no device found after 3 tries");
        return -1;
    }

    LOGI("MAG", "selected addr=0x%02X write=0x%02X read=0x%02X",
         qmc6309_addr,
         QMC6309_I2C_WRITE(qmc6309_addr),
         QMC6309_I2C_READ(qmc6309_addr));

    if (QMC6309_Wait_Ready(QMC6309_PWR_UP_DELAY_MS) != 0) {
        LOGE("MAG", "wait ready failed");
        return -1;
    }

    id = QMC6309_ReadID();
    if (id != QMC6309_CHIP_ID_VALUE) {
        LOGE("MAG", "CHIP_ID error got=0x%02X exp=0x%02X", id, QMC6309_CHIP_ID_VALUE);
        return -1;
    }
    LOGI("MAG", "CHIP_ID=0x%02X OK", id);

    if (QMC6309_Reset() != 0) {
        return -1;
    }

    if (QMC6309_WriteReg(QMC6309_REG_CONTROL_1, QMC6309_CTRL1_STANDBY) != 0) {
        return -1;
    }
    if (QMC6309_WriteReg(QMC6309_REG_CONTROL_2, QMC6309_CTRL2_INIT) != 0) {
        return -1;
    }
    if (QMC6309_WriteReg(QMC6309_REG_CONTROL_1, QMC6309_CTRL1_ACTIVE) != 0) {
        return -1;
    }
    QMC6309_Delay_ms(2);

    ctrl1_readback = QMC6309_ReadReg(QMC6309_REG_CONTROL_1);
    ctrl2_readback = QMC6309_ReadReg(QMC6309_REG_CONTROL_2);
    LOGI("MAG", "CTRL1=0x%02X CTRL2=0x%02X", ctrl1_readback, ctrl2_readback);

    if ((ctrl1_readback == 0xFF) || (ctrl2_readback == 0xFF)) {
        LOGE("MAG", "register readback error");
        return -1;
    }

    if (ctrl1_readback != QMC6309_CTRL1_ACTIVE) {
        LOGW("MAG", "CTRL1 mismatch read=0x%02X exp=0x%02X",
             ctrl1_readback, QMC6309_CTRL1_ACTIVE);
    }
    if (ctrl2_readback != QMC6309_CTRL2_INIT) {
        LOGW("MAG", "CTRL2 mismatch read=0x%02X exp=0x%02X",
             ctrl2_readback, QMC6309_CTRL2_INIT);
    }

    Filter_ResetMagLowPass();

    LOGI("MAG", "========== QMC6309 Init OK ==========");
    return 0;
}
