/**
 * @file    QMI8658.c
 * @brief   QMI8658 6轴IMU驱动实现
 * @author  boweny
 * @date    2026-04-22
 * @version v1.1
 *
 * @details
 * - 实现 QMI8658 6轴惯性测量单元（加速度计+陀螺仪）的初始化和数据读取
 * - 支持 I2C 主/备地址自动探测、软复位、就绪检测、数据有效性检查
 * - 保留原始陀螺仪读取接口，并新增软件低通后的滤波读取接口
 * - 关键步骤嵌入 LOG 日志，便于调试和问题排查
 * - 导出函数: QMI8658_Init / QMI8658_ReadID / QMI8658_ReadAcc /
 *             QMI8658_ReadGyro / QMI8658_ReadGyroFiltered /
 *             QMI8658_ReadTemp / QMI8658_ReadAll /
 *             QMI8658_Enable / QMI8658_Disable /
 *             QMI8658_Wait_AccReady / QMI8658_Wait_GyroReady / QMI8658_BusRecover
 *
 * @hardware
 *   - I2C接口: P1.4(SDA) / P1.5(SCL)，硬件I2C
 *   - 系统时钟: Fosc = 24MHz
 *   - I2C地址: 0x6B (SA0=浮空/高) / 0x6A (SA0=地)
 *
 * @note    必须在 I2C_config() 初始化完成后调用 QMI8658_Init()
 * @note    禁止使用浮点运算，数据以 int16 定点整数传递
 *
 * @see     Code_boweny/Device/QMI8658/QMI8658.h
 */

#include "QMI8658.h"
#include "Filter.h"
#undef LOGD
#define LOGD(tag, ...)

/*==============================================================
 *                       外部 Driver 层 API
 *==============================================================*/

/*==============================================================
 *                       本地变量
 *==============================================================*/

u8 QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;  /**< 当前使用的7位I2C地址 */

static void QMI8658_Delay_ms(u16 ms);
static u8 QMI8658_ReadRegAtAddr(u8 addr, u8 reg_addr, u8 *ok);

static u8 QMI8658_SelectAddrByWhoAmI(u8 *selected_id)
{
    u8 retry;
    u8 primary_ok = 0;
    u8 alt_ok = 0;
    u8 primary_id = 0xFF;
    u8 alt_id = 0xFF;

    for (retry = 0; retry < 5; retry++) {
        primary_id = QMI8658_ReadRegAtAddr(QMI8658_I2C_ADDR_PRIMARY,
                                           QMI8658_REG_WHO_AM_I,
                                           &primary_ok);
        if ((primary_ok != 0) && (primary_id == QMI8658_CHIP_ID_VALUE)) {
            QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;
            if (selected_id != NULL) {
                *selected_id = primary_id;
            }
            LOGI("IMU", "id probe primary ok id=0x%02X addr=0x%02X",
                 primary_id, QMI8658_I2C_Addr);
            return 0;
        }

        alt_id = QMI8658_ReadRegAtAddr(QMI8658_I2C_ADDR_ALT,
                                       QMI8658_REG_WHO_AM_I,
                                       &alt_ok);
        if ((alt_ok != 0) && (alt_id == QMI8658_CHIP_ID_VALUE)) {
            QMI8658_I2C_Addr = QMI8658_I2C_ADDR_ALT;
            if (selected_id != NULL) {
                *selected_id = alt_id;
            }
            LOGI("IMU", "id probe alt ok id=0x%02X addr=0x%02X",
                 alt_id, QMI8658_I2C_Addr);
            return 0;
        }

        LOGW("IMU", "id probe try=%u p=0x%02X ok=%u a=0x%02X ok=%u",
             (u16)(retry + 1),
             primary_id,
             (u16)primary_ok,
             alt_id,
             (u16)alt_ok);
        QMI8658_BusRecover();
        QMI8658_Delay_ms(200);
    }

    LOGE("IMU", "id probe fail p=0x%02X ok=%u a=0x%02X ok=%u",
         primary_id,
         (u16)primary_ok,
         alt_id,
         (u16)alt_ok);
    return 1;
}

/*==============================================================
 *                       内部辅助函数
 *==============================================================*/

static void QMI8658_Delay_ms(u16 ms);

/**
 * @brief      I2C 单字节写入
 * @param[in]  reg_addr  寄存器地址
 * @param[in]  data      要写入的数据
 * @return     0=成功，1=失败
 *
 * @details
 * 调用 Driver 层 I2C_WriteNbyte() 写入 1 字节。
 * 驱动内部使用，不对外暴露。
 */
static u8 QMI8658_WriteReg(u8 reg_addr, u8 reg_val)
{
    u8 i2c_addr = QMI8658_I2C_WRITE(QMI8658_I2C_Addr);

    I2C_WriteNbyte(i2c_addr, reg_addr, &reg_val, 1);

    if (Get_MSBusy_Status()) {
        LOGW("IMU", "WR NACK reg=0x%02X val=0x%02X", reg_addr, reg_val);
        return 1;
    }
    return 0;
}

/**
 * @brief      I2C 单字节读取
 * @param[in]  reg_addr  寄存器地址
 * @return     读取到的数据，0xFF 表示读取失败
 *
 * @details
 * 调用 Driver 层 I2C_ReadNbyte() 读取 1 字节。
 * 驱动内部使用，不对外暴露。
 */
static u8 QMI8658_ReadReg(u8 reg_addr)
{
    u8 i2c_addr_w = QMI8658_I2C_WRITE(QMI8658_I2C_Addr);
    u8 reg_val = 0xFF;

    I2C_ReadNbyte(i2c_addr_w, reg_addr, &reg_val, 1);

    if (Get_MSBusy_Status()) {
        LOGE("IMU", "RD NACK reg=0x%02X", reg_addr);
        return 0xFF;
    }
    return reg_val;
}

static u8 QMI8658_ReadRegAtAddr(u8 addr, u8 reg_addr, u8 *ok)
{
    u8 i2c_addr_w = QMI8658_I2C_WRITE(addr);
    u8 reg_val = 0xFF;

    I2C_ReadNbyte(i2c_addr_w, reg_addr, &reg_val, 1);

    if (Get_MSBusy_Status()) {
        if (ok != NULL) {
            *ok = 0;
        }
        return 0xFF;
    }

    if (ok != NULL) {
        *ok = 1;
    }
    return reg_val;
}

/**
 * @brief      连续读取多字节数据 (地址自动递增)
 * @param[in]  start_reg  起始寄存器地址
 * @param[out] buf        数据缓冲区
 * @param[in]  len        要读取的字节数
 * @return     0=成功，1=失败
 *
 * @details
 * 调用 Driver 层 I2C_ReadNbyte() 连续读取 len 字节。
 * 驱动内部使用，不对外暴露。
 */
static u8 QMI8658_ReadNByte(u8 start_reg, u8 *buf, u8 len)
{
    u8 i2c_addr_w = QMI8658_I2C_WRITE(QMI8658_I2C_Addr);

    I2C_ReadNbyte(i2c_addr_w, start_reg, buf, len);

    if (Get_MSBusy_Status()) {
        LOGE("IMU", "RNB NACK reg=0x%02X len=%u", start_reg, len);
        return 1;
    }
    return 0;
}

static void QMI8658_LogDataPath(u8 *phase)
{
    u8 ctrl1;
    u8 ctrl2;
    u8 ctrl3;
    u8 ctrl5;
    u8 ctrl6;
    u8 ctrl7;
    u8 ctrl8;
    u8 ctrl9;
    u8 fifo_wtm;
    u8 fifo_ctrl;
    u8 fifo_status;
    u8 statusint;
    u8 status0;
    u8 ts_raw[3];
    u8 temp_raw[2];
    u8 raw[12];
    u32 ts;
    int16 temp;
    int16 ax;
    int16 ay;
    int16 az;
    int16 gx;
    int16 gy;
    int16 gz;

    ctrl1 = QMI8658_ReadReg(QMI8658_REG_CTRL1);
    ctrl2 = QMI8658_ReadReg(QMI8658_REG_CTRL2);
    ctrl3 = QMI8658_ReadReg(QMI8658_REG_CTRL3);
    ctrl5 = QMI8658_ReadReg(QMI8658_REG_CTRL5);
    ctrl6 = QMI8658_ReadReg(QMI8658_REG_CTRL6);
    ctrl7 = QMI8658_ReadReg(QMI8658_REG_CTRL7);
    ctrl8 = QMI8658_ReadReg(QMI8658_REG_CTRL8);
    ctrl9 = QMI8658_ReadReg(QMI8658_REG_CTRL9);
    fifo_wtm = QMI8658_ReadReg(QMI8658_REG_FIFO_WTM);
    fifo_ctrl = QMI8658_ReadReg(QMI8658_REG_FIFO_CTRL);
    fifo_status = QMI8658_ReadReg(QMI8658_REG_FIFO_STATUS);
    statusint = QMI8658_ReadReg(QMI8658_REG_STATUSINT);
    status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);

    LOGI("IMU", "%s regs c1=%02X c2=%02X c3=%02X c5=%02X c6=%02X",
         phase, ctrl1, ctrl2, ctrl3, ctrl5, ctrl6);
    LOGI("IMU", "%s regs c7=%02X c8=%02X c9=%02X fw=%02X fc=%02X fs=%02X",
         phase, ctrl7, ctrl8, ctrl9, fifo_wtm, fifo_ctrl, fifo_status);
    LOGI("IMU", "%s status int=%02X s0=%02X", phase, statusint, status0);

    if (QMI8658_ReadNByte(QMI8658_REG_TIMESTAMP_L, ts_raw, 3) == 0) {
        ts = ((u32)ts_raw[2] << 16) | ((u32)ts_raw[1] << 8) | ts_raw[0];
        LOGI("IMU", "%s ts=%lu", phase, ts);
    }

    if (QMI8658_ReadNByte(QMI8658_REG_TEMP_L, temp_raw, 2) == 0) {
        temp = (int16)((u16)temp_raw[1] << 8 | temp_raw[0]);
        LOGI("IMU", "%s temp_raw=%d", phase, temp);
    }

    if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 12) != 0) {
        LOGW("IMU", "%s raw read fail", phase);
        return;
    }

    ax = (int16)((u16)raw[1] << 8 | raw[0]);
    ay = (int16)((u16)raw[3] << 8 | raw[2]);
    az = (int16)((u16)raw[5] << 8 | raw[4]);
    gx = (int16)((u16)raw[7] << 8 | raw[6]);
    gy = (int16)((u16)raw[9] << 8 | raw[8]);
    gz = (int16)((u16)raw[11] << 8 | raw[10]);

    LOGI("IMU", "%s raw a=%d %d %d g=%d %d %d",
         phase, ax, ay, az, gx, gy, gz);
}

static u8 QMI8658_LogDataWindow(void)
{
    u8 i;
    u8 statusint;
    u8 status0;
    u8 ts_raw[3];
    u8 temp_raw[2];
    u8 raw[12];
    u32 ts;
    int16 temp;
    int16 ax;
    int16 ay;
    int16 az;
    int16 gx;
    int16 gy;
    int16 gz;
    u8 saw_data;

    saw_data = 0;
    LOGI("IMU", "data window start");

    for (i = 0; i < 10; i++) {
        statusint = QMI8658_ReadReg(QMI8658_REG_STATUSINT);
        status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);

        ts = 0;
        if (QMI8658_ReadNByte(QMI8658_REG_TIMESTAMP_L, ts_raw, 3) == 0) {
            ts = ((u32)ts_raw[2] << 16) | ((u32)ts_raw[1] << 8) | ts_raw[0];
        }

        temp = 0;
        if (QMI8658_ReadNByte(QMI8658_REG_TEMP_L, temp_raw, 2) == 0) {
            temp = (int16)((u16)temp_raw[1] << 8 | temp_raw[0]);
        }

        ax = 0; ay = 0; az = 0;
        gx = 0; gy = 0; gz = 0;
        if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 12) == 0) {
            ax = (int16)((u16)raw[1] << 8 | raw[0]);
            ay = (int16)((u16)raw[3] << 8 | raw[2]);
            az = (int16)((u16)raw[5] << 8 | raw[4]);
            gx = (int16)((u16)raw[7] << 8 | raw[6]);
            gy = (int16)((u16)raw[9] << 8 | raw[8]);
            gz = (int16)((u16)raw[11] << 8 | raw[10]);
        }

        LOGI("IMU", "poll%u si=%02X s0=%02X ts=%lu t=%d a=%d %d %d",
             (u16)i, statusint, status0, ts, temp, ax, ay, az);
        LOGI("IMU", "poll%u g=%d %d %d", (u16)i, gx, gy, gz);

        if ((status0 != 0U) || (ts != 0UL) || (temp != 0) ||
            (ax != 0) || (ay != 0) || (az != 0) ||
            (gx != 0) || (gy != 0) || (gz != 0)) {
            saw_data = 1;
            break;
        }

        QMI8658_Delay_ms(100);
    }

    LOGI("IMU", "data window result=%s", saw_data ? "DATA" : "ALL_ZERO");
    return saw_data;
}

static s8 QMI8658_ClearDataPath(void)
{
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00) != 0) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_CTRL6, QMI8658_CTRL6_INIT) != 0) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_CTRL8, QMI8658_CTRL8_INIT) != 0) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_FIFO_WTM, QMI8658_FIFO_WTM_INIT) != 0) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_FIFO_CTRL, QMI8658_FIFO_CTRL_BYPASS) != 0) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_CTRL9, QMI8658_CTRL9_CMD_RST_FIFO) != 0) {
        return -1;
    }
    QMI8658_Delay_ms(2);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL9, QMI8658_CTRL9_CMD_ACK) != 0) {
        return -1;
    }
    return 0;
}

/**
 * @brief   毫秒级延时
 * @param[in]  ms  延时毫秒数
 *
 * @details 调用 Driver 层 delay_ms()，依赖 Timer 已初始化
 */
static void QMI8658_Delay_ms(u16 ms)
{
    delay_ms(ms);
}

/**
 * @brief      探测有效 I2C 地址
 * @param[in]  addr  要探测的 7 位 I2C 地址
 * @return     0=该地址有设备响应，1=无响应
 *
 * @details
 * 发送 START + 设备写地址，检测是否有 ACK。
 */
static u8 QMI8658_ProbeAddr(u8 addr)
{
    u8 i2c_addr = QMI8658_I2C_WRITE(addr);
    u8 dummy = QMI8658_REG_WHO_AM_I;

    I2C_WriteNbyte(i2c_addr, QMI8658_REG_WHO_AM_I, &dummy, 0);

    if (Get_MSBusy_Status()) {
        return 1;  /* NACK: 无设备 */
    }
    return 0;  /* ACK: 有设备 */
}

/*==============================================================
 *                       公共 API 实现
 *==============================================================*/

/**
 * @brief   执行 I2C 总线恢复
 * @return  none
 *
 * @details
 * 当 SDA 或 SCL 被从机意外拉低时，尝试恢复总线。
 * 禁用 I2C 模块，将 P1.4/P1.5 切为 GPIO 开漏，
 * 手动发送 9 个时钟脉冲迫使从机释放 SDA，最后发送 STOP。
 *
 * @note    此函数直接操作引脚，适用于 I2C 总线死锁场景
 */
void QMI8658_BusRecover(void)
{
    LOGD("IMU", "bus recover start");

    I2C_Function(DISABLE);
    I2C_Master();

    P1_MODE_OUT_OD(GPIO_Pin_4 | GPIO_Pin_5);
    P1_PULL_UP_ENABLE(GPIO_Pin_4 | GPIO_Pin_5);

    {
        u8 i;
        for (i = 0; i < 9; i++) {
            P15 = 0;
            QMI8658_Delay_ms(1);
            P15 = 1;
            QMI8658_Delay_ms(1);
            if (P14 == 1) break;
        }
        P14 = 1;
        QMI8658_Delay_ms(1);
    }

    P1_MODE_OUT_OD(GPIO_Pin_4 | GPIO_Pin_5);
    I2C_Function(ENABLE);
    I2C_Master();

    LOGI("IMU", "bus recover done");
}

/**
 * @brief      读取 QMI8658 芯片ID
 * @return     WHO_AM_I 值 (0x05=正确)，0xFF=读取失败
 */
u8 QMI8658_ReadID(void)
{
    u8 id = QMI8658_ReadReg(QMI8658_REG_WHO_AM_I);
    LOGD("IMU", "ReadID=0x%02X expected=0x%02X", id, QMI8658_CHIP_ID_VALUE);
    if (id != QMI8658_CHIP_ID_VALUE) {
        LOGE("IMU", "ID mismatch got=0x%02X exp=0x%02X", id, QMI8658_CHIP_ID_VALUE);
    }
    return id;
}

/**
 * @brief      读取加速度数据
 * @param[out] x  指向X轴加速度的指针 (int16)
 * @param[out] y  指向Y轴加速度的指针 (int16)
 * @param[out] z  指向Z轴加速度的指针 (int16)
 * @return      0=成功，-1=数据无效或读取失败
 */
s8 QMI8658_ReadAcc(int16 *x, int16 *y, int16 *z)
{
    static u8 invalid_latched = 0;
    u8 raw[6];
    u8 status0;
    int16 ax, ay, az;

    if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 6) != 0) {
        LOGE("IMU", "ReadAcc I2C fail");
        return -1;
    }

    ax = (int16)((u16)raw[1] << 8 | raw[0]);
    ay = (int16)((u16)raw[3] << 8 | raw[2]);
    az = (int16)((u16)raw[5] << 8 | raw[4]);

    if (QMI8658_ACC_IS_ZERO(ax, ay, az) || QMI8658_DATA_IS_INVALID(ax, ay, az)) {
        if (!invalid_latched) {
            status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);
            LOGW("IMU", "acc invalid ax=%d ay=%d az=%d status0=0x%02X data_reg=0x%02X",
                 ax, ay, az, status0, QMI8658_REG_AX_L);
        }
        invalid_latched = 1;
        return -1;
    }

    invalid_latched = 0;
    *x = ax;
    *y = ay;
    *z = az;

    LOGD("IMU", "acc=%d %d %d", ax, ay, az);
    return 0;
}

/**
 * @brief      读取陀螺仪数据
 * @param[out] x  指向X轴角速度的指针 (int16)
 * @param[out] y  指向Y轴角速度的指针 (int16)
 * @param[out] z  指向Z轴角速度的指针 (int16)
 * @return      0=成功，-1=数据无效或读取失败
 */
s8 QMI8658_ReadGyro(int16 *x, int16 *y, int16 *z)
{
    u8 raw[6];
    int16 gx, gy, gz;

    if (QMI8658_ReadNByte(QMI8658_REG_GX_L, raw, 6) != 0) {
        LOGE("IMU", "ReadGyro I2C fail");
        return -1;
    }

    gx = (int16)((u16)raw[1] << 8 | raw[0]);
    gy = (int16)((u16)raw[3] << 8 | raw[2]);
    gz = (int16)((u16)raw[5] << 8 | raw[4]);

    if (QMI8658_GYRO_IS_ZERO(gx, gy, gz) || QMI8658_DATA_IS_INVALID(gx, gy, gz)) {
        LOGD("IMU", "gyro invalid gx=%d gy=%d gz=%d", gx, gy, gz);
        return -1;
    }

    *x = gx;
    *y = gy;
    *z = gz;

    LOGD("IMU", "gyro=%d %d %d", gx, gy, gz);
    return 0;
}

/**
 * @brief      读取低通滤波后的陀螺仪数据
 * @param[out] x  指向X轴角速度的指针 (int16)
 * @param[out] y  指向Y轴角速度的指针 (int16)
 * @param[out] z  指向Z轴角速度的指针 (int16)
 * @return      0=成功，-1=原始数据无效、滤波失败或读取失败
 *
 * @details
 * 先读取原始陀螺仪数据，再通过 Function/Filter 模块进行软件低通滤波。
 * 首帧有效数据直接作为滤波输出，避免初始化瞬态抖动。
 */
s8 QMI8658_ReadGyroFiltered(int16 *x, int16 *y, int16 *z)
{
    int16 gx;
    int16 gy;
    int16 gz;

    if ((x == 0) || (y == 0) || (z == 0)) {
        return -1;
    }

    if (QMI8658_ReadGyro(&gx, &gy, &gz) != 0) {
        return -1;
    }

    return Filter_GyroLowPass(gx, gy, gz, x, y, z);
}

/**
 * @brief      读取温度数据
 * @param[out] temp  指向温度值的指针 (int16)
 * @return      0=成功，-1=读取失败
 *
 * @details
 * 读取 2 字节 (0x33~0x34)，int12 有符号数，LSB在前。
 * 换算: 实际温度 = reg_value / 256 + 25 (°C)
 */
s8 QMI8658_ReadTemp(int16 *temp)
{
    u8 raw[2];
    int16 t;

    if (QMI8658_ReadNByte(QMI8658_REG_TEMP_L, raw, 2) != 0) {
        LOGE("IMU", "ReadTemp I2C fail");
        return -1;
    }

    t = (int16)((u16)raw[1] << 8 | raw[0]);
    *temp = t;

    LOGD("IMU", "temp raw=%d", t);
    return 0;
}

/**
 * @brief      读取完整9轴数据 (加速度+陀螺仪)
 * @param[out] ax  指向X轴加速度的指针
 * @param[out] ay  指向Y轴加速度的指针
 * @param[out] az  指向Z轴加速度的指针
 * @param[out] gx  指向X轴角速度的指针
 * @param[out] gy  指向Y轴角速度的指针
 * @param[out] gz  指向Z轴角速度的指针
 * @return      0=成功，-1=数据无效或读取失败
 *
 * @details
 * 使用 I2C 地址自动递增，一次读取 12 字节 (0x35~0x40)。
 */
s8 QMI8658_ReadAll(int16 *ax, int16 *ay, int16 *az,
                    int16 *gx, int16 *gy, int16 *gz)
{
    u8 raw[12];
    int16 aax, aay, aaz;
    int16 ggx, ggy, ggz;

    if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 12) != 0) {
        LOGE("IMU", "ReadAll I2C fail");
        return -1;
    }

    aax = (int16)((u16)raw[1] << 8 | raw[0]);
    aay = (int16)((u16)raw[3] << 8 | raw[2]);
    aaz = (int16)((u16)raw[5] << 8 | raw[4]);

    ggx = (int16)((u16)raw[7] << 8 | raw[6]);
    ggy = (int16)((u16)raw[9] << 8 | raw[8]);
    ggz = (int16)((u16)raw[11] << 8 | raw[10]);

    if ((QMI8658_ACC_IS_ZERO(aax, aay, aaz) || QMI8658_DATA_IS_INVALID(aax, aay, aaz)) &&
        (QMI8658_GYRO_IS_ZERO(ggx, ggy, ggz) || QMI8658_DATA_IS_INVALID(ggx, ggy, ggz))) {
        LOGD("IMU", "all data invalid");
        return -1;
    }

    *ax = aax;
    *ay = aay;
    *az = aaz;
    *gx = ggx;
    *gy = ggy;
    *gz = ggz;

    LOGD("IMU", "a=%d %d %d g=%d %d %d", aax, aay, aaz, ggx, ggy, ggz);
    return 0;
}

/**
 * @brief      轮询等待加速度数据就绪
 * @param[in]  timeout_ms  超时时间 (ms)
 * @return     0=数据就绪，-1=超时
 */
s8 QMI8658_Wait_AccReady(u16 timeout_ms)
{
    u16 i;
    u8 status0;

    LOGD("IMU", "wait acc ready timeout=%ums", timeout_ms);
    status0 = 0;

    for (i = 0; i < timeout_ms; i += 5) {
        status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);
        if ((status0 & QMI8658_STATUS0_A_DA) != 0) {
            LOGD("IMU", "acc ready in %ums status0=0x%02X", i, status0);
            return 0;
        }
        QMI8658_Delay_ms(5);
    }

    LOGE("IMU", "wait acc ready TIMEOUT status0=0x%02X", status0);
    return -1;
}

/**
 * @brief      轮询等待陀螺仪数据就绪
 * @param[in]  timeout_ms  超时时间 (ms)
 * @return     0=数据就绪，-1=超时
 */
s8 QMI8658_Wait_GyroReady(u16 timeout_ms)
{
    u16 i;

    LOGD("IMU", "wait gyro ready timeout=%ums", timeout_ms);

    for (i = 0; i < timeout_ms; i += 5) {
        u8 status = QMI8658_ReadReg(QMI8658_REG_STATUS0);
        if ((status & QMI8658_STATUS0_G_DA) != 0) {
            LOGD("IMU", "gyro ready in %ums status=0x%02X", i, status);
            return 0;
        }
        QMI8658_Delay_ms(5);
    }

    LOGE("IMU", "wait gyro ready TIMEOUT");
    return -1;
}

/**
 * @brief   使能 QMI8658 IMU 传感器
 * @return  0=成功，-1=失败
 *
 * @details 写入 CTRL7=0x03，使能加速度计和陀螺仪
 */
s8 QMI8658_Enable(void)
{
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, QMI8658_CTRL7_INIT) != 0) {
        LOGE("IMU", "Enable WR fail");
        return -1;
    }
    LOGI("IMU", "enabled CTRL7=0x%02X", QMI8658_CTRL7_INIT);
    return 0;
}

/**
 * @brief   禁用 QMI8658 IMU 传感器
 * @return  0=成功，-1=失败
 *
 * @details 写入 CTRL7=0x00，禁用加速度计和陀螺仪，进入低功耗模式
 */
s8 QMI8658_Disable(void)
{
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00) != 0) {
        LOGE("IMU", "Disable WR fail");
        return -1;
    }
    LOGI("IMU", "disabled");
    return 0;
}

void QMI8658_DumpRawRegs(void)
{
    u8 who;
    u8 rev;
    u8 reset_state;
    u8 ctrl1, ctrl2, ctrl3, ctrl5, ctrl7;
    u8 statusint, status0;
    u8 temp_l, temp_h;
    u8 ax_l, ax_h, ay_l, ay_h, az_l, az_h;
    u8 gx_l, gx_h, gy_l, gy_h, gz_l, gz_h;

    who = QMI8658_ReadReg(QMI8658_REG_WHO_AM_I);
    rev = QMI8658_ReadReg(QMI8658_REG_REVISION_ID);
    reset_state = QMI8658_ReadReg(QMI8658_REG_RESET_STATE);
    ctrl1 = QMI8658_ReadReg(QMI8658_REG_CTRL1);
    ctrl2 = QMI8658_ReadReg(QMI8658_REG_CTRL2);
    ctrl3 = QMI8658_ReadReg(QMI8658_REG_CTRL3);
    ctrl5 = QMI8658_ReadReg(QMI8658_REG_CTRL5);
    ctrl7 = QMI8658_ReadReg(QMI8658_REG_CTRL7);
    statusint = QMI8658_ReadReg(QMI8658_REG_STATUSINT);
    status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);
    temp_l = QMI8658_ReadReg(QMI8658_REG_TEMP_L);
    temp_h = QMI8658_ReadReg(QMI8658_REG_TEMP_H);
    ax_l = QMI8658_ReadReg(QMI8658_REG_AX_L);
    ax_h = QMI8658_ReadReg(QMI8658_REG_AX_H);
    ay_l = QMI8658_ReadReg(QMI8658_REG_AY_L);
    ay_h = QMI8658_ReadReg(QMI8658_REG_AY_H);
    az_l = QMI8658_ReadReg(QMI8658_REG_AZ_L);
    az_h = QMI8658_ReadReg(QMI8658_REG_AZ_H);
    gx_l = QMI8658_ReadReg(QMI8658_REG_GX_L);
    gx_h = QMI8658_ReadReg(QMI8658_REG_GX_H);
    gy_l = QMI8658_ReadReg(QMI8658_REG_GY_L);
    gy_h = QMI8658_ReadReg(QMI8658_REG_GY_H);
    gz_l = QMI8658_ReadReg(QMI8658_REG_GZ_L);
    gz_h = QMI8658_ReadReg(QMI8658_REG_GZ_H);

    LOGI("IMU", "dump id=%02X rev=%02X rst=%02X c1=%02X c2=%02X c3=%02X c5=%02X c7=%02X",
         who, rev, reset_state, ctrl1, ctrl2, ctrl3, ctrl5, ctrl7);
    LOGI("IMU", "dump st int=%02X s0=%02X temp=%02X %02X",
         statusint, status0, temp_l, temp_h);
    LOGI("IMU", "dump acc=%02X %02X %02X %02X %02X %02X",
         ax_l, ax_h, ay_l, ay_h, az_l, az_h);
    LOGI("IMU", "dump gyr=%02X %02X %02X %02X %02X %02X",
         gx_l, gx_h, gy_l, gy_h, gz_l, gz_h);
}

/**
 * @brief   初始化 QMI8658 IMU
 * @return  0=成功，-1=失败
 *
 * @details
 * 完整初始化流程:
 *   1. LOG 初始化开始
 *   2. 尝试主地址 (0x6B)，失败则尝试备地址 (0x6A)
 *   3. 读取 WHO_AM_I 确认芯片 (0x05)
 *   4. 检查 RESET_STATE: 0x80=已就绪, 0x00=需软复位
 *   5. 软复位 (写入 RESET=0xB0 到地址0x60) → 等待 500ms
 *   6. 分阶段写入控制寄存器
 *   7. 等待数据就绪 (STATUS0.aDA=1)
 *   8. LOG 初始化完成
 *
 * @note    必须在 I2C_config() 初始化完成后调用
 */
s8 QMI8658_Init(void)
{
    u8 id = 0xFF;
    u8 reset_state;
    u8 ctrl1_rb, ctrl2_rb, ctrl3_rb, ctrl5_rb, ctrl7_rb;

    LOGI("IMU", "========== QMI8658 Init Start ==========");
    LOGI("IMU", "stable bring-up: soft_reset=%u diag=%u",
         (u16)QMI8658_SOFT_RESET_ENABLE, (u16)QMI8658_DIAG_ENABLE);
    LOGD("IMU", "primary addr=0x%02X alt=0x%02X",
         QMI8658_I2C_WRITE(QMI8658_I2C_ADDR_PRIMARY),
         QMI8658_I2C_WRITE(QMI8658_I2C_ADDR_ALT));

    /*---------------------------- 地址探测 ----------------------------*/
    /*---------------------------- 上电等待 ----------------------------*/
    LOGD("IMU", "power-up wait %ums", QMI8658_PWR_UP_DELAY_MS);
    QMI8658_Delay_ms(QMI8658_PWR_UP_DELAY_MS);

    /*---------------------------- WHO_AM_I 检测 ------------------------*/
    if (QMI8658_SelectAddrByWhoAmI(&id) != 0) {
        LOGE("IMU", "WHO_AM_I error got=0x%02X exp=0x%02X", id, QMI8658_CHIP_ID_VALUE);
        return -1;
    }
    LOGD("IMU", "WHO_AM_I=0x%02X OK addr=0x%02X", id, QMI8658_I2C_Addr);

    /*---------------------------- RESET_STATE 检测 ------------------------*/
    reset_state = QMI8658_ReadReg(QMI8658_REG_RESET_STATE);
    LOGI("IMU", "reset_state before=0x%02X", reset_state);

#if QMI8658_SOFT_RESET_ENABLE
    if (reset_state != QMI8658_RESET_STATE_READY) {
        if (QMI8658_WriteReg(QMI8658_REG_RESET, 0xB0) != 0) {
            LOGE("IMU", "soft reset WR fail");
            return -1;
        }

        QMI8658_Delay_ms(QMI8658_RESET_DELAY_MS);
        reset_state = QMI8658_ReadReg(QMI8658_REG_RESET_STATE);
        LOGI("IMU", "reset_state after=0x%02X", reset_state);
    } else {
        LOGI("IMU", "reset_state ready, skip reset");
    }
#else
    LOGI("IMU", "soft reset disabled state=0x%02X", reset_state);
#endif

    if (reset_state == 0xFF) {
        LOGE("IMU", "reset_state read fail after reset");
        return -1;
    }

    id = QMI8658_ReadID();
    if (id != QMI8658_CHIP_ID_VALUE) {
        LOGE("IMU", "WHO_AM_I lost after reset got=0x%02X", id);
        return -1;
    }

#if QMI8658_CLEAR_DATAPATH_ENABLE
    if (QMI8658_ClearDataPath() != 0) {
        LOGE("IMU", "clear data path fail");
        return -1;
    }
    QMI8658_LogDataPath("after_clear");
#else
    LOGI("IMU", "skip ctrl6/8/fifo/ctrl9 clear");
#endif

    /*---------------------------- 配置写入 --------------------------*/

    /* Step 1: 禁用传感器 (安全写入) */
    LOGD("IMU", "step1 CTRL7=0x00 (disable sensors)");
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00) != 0) {
        LOGE("IMU", "step1 WR fail");
        return -1;
    }

    /* Step 2: 写入 CTRL1 */
    LOGD("IMU", "step2 CTRL1=0x%02X", QMI8658_CTRL1_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL1, QMI8658_CTRL1_INIT) != 0) {
        LOGE("IMU", "step2 WR fail");
        return -1;
    }

    /* Step 3: 写入 CTRL2 (加速度量程+ODR) */
    LOGD("IMU", "step3 CTRL2=0x%02X (ACC range+ODR)", QMI8658_CTRL2_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL2, QMI8658_CTRL2_INIT) != 0) {
        LOGE("IMU", "step3 WR fail");
        return -1;
    }

    /* Step 4: 写入 CTRL3 (陀螺音量程+ODR) */
    LOGD("IMU", "step4 CTRL3=0x%02X (GYRO range+ODR)", QMI8658_CTRL3_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL3, QMI8658_CTRL3_INIT) != 0) {
        LOGE("IMU", "step4 WR fail");
        return -1;
    }

    /* Step 5: 写入 CTRL5 */
    LOGD("IMU", "step5 CTRL5=0x%02X", QMI8658_CTRL5_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL5, QMI8658_CTRL5_INIT) != 0) {
        LOGE("IMU", "step5 WR fail");
        return -1;
    }

    /* Step 6: 使能加速度计+陀螺仪 */
    LOGD("IMU", "step6 CTRL7=0x%02X (enable sensors)", QMI8658_CTRL7_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, QMI8658_CTRL7_INIT) != 0) {
        LOGE("IMU", "step6 WR fail");
        return -1;
    }
    QMI8658_Delay_ms(QMI8658_ENABLE_DELAY_MS);

    /*---------------------------- 读回验证 --------------------------*/
    ctrl1_rb = QMI8658_ReadReg(QMI8658_REG_CTRL1);
    ctrl2_rb = QMI8658_ReadReg(QMI8658_REG_CTRL2);
    ctrl3_rb = QMI8658_ReadReg(QMI8658_REG_CTRL3);
    ctrl5_rb = QMI8658_ReadReg(QMI8658_REG_CTRL5);
    ctrl7_rb = QMI8658_ReadReg(QMI8658_REG_CTRL7);

    LOGI("IMU", "readback CTRL1=0x%02X CTRL2=0x%02X CTRL3=0x%02X CTRL5=0x%02X CTRL7=0x%02X",
         ctrl1_rb, ctrl2_rb, ctrl3_rb, ctrl5_rb, ctrl7_rb);

    if (ctrl1_rb != QMI8658_CTRL1_INIT) {
        LOGW("IMU", "CTRL1 mismatch read=0x%02X exp=0x%02X", ctrl1_rb, QMI8658_CTRL1_INIT);
    }
    if (ctrl2_rb != QMI8658_CTRL2_INIT) {
        LOGW("IMU", "CTRL2 mismatch read=0x%02X exp=0x%02X", ctrl2_rb, QMI8658_CTRL2_INIT);
    }
    if (ctrl3_rb != QMI8658_CTRL3_INIT) {
        LOGW("IMU", "CTRL3 mismatch read=0x%02X exp=0x%02X", ctrl3_rb, QMI8658_CTRL3_INIT);
    }
    if (ctrl5_rb != QMI8658_CTRL5_INIT) {
        LOGW("IMU", "CTRL5 mismatch read=0x%02X exp=0x%02X", ctrl5_rb, QMI8658_CTRL5_INIT);
    }
    if (ctrl7_rb != QMI8658_CTRL7_INIT) {
        LOGW("IMU", "CTRL7 mismatch read=0x%02X exp=0x%02X", ctrl7_rb, QMI8658_CTRL7_INIT);
    }

    /*---------------------------- 等待数据就绪 --------------------------*/
#if QMI8658_DIAG_ENABLE
    QMI8658_LogDataPath("after_enable");
#endif

    LOGD("IMU", "wait sensor data ready...");
    if (QMI8658_Wait_AccReady(QMI8658_READY_TIMEOUT_MS) != 0) {
        LOGW("IMU", "acc not ready in %ums, continue anyway", QMI8658_READY_TIMEOUT_MS);
#if QMI8658_DIAG_ENABLE
        QMI8658_LogDataPath("not_ready");
        QMI8658_LogDataWindow();
#endif
    } else {
#if QMI8658_DIAG_ENABLE
        QMI8658_LogDataPath("ready");
#endif
    }

    Filter_ResetGyroLowPass();

    /*---------------------------- 完成 --------------------------*/
    LOGI("IMU", "========== QMI8658 Init Done ==========");
    LOGI("IMU", "config: CTRL2=0x%02X CTRL3=0x%02X CTRL5=0x%02X CTRL7=0x%02X",
         QMI8658_CTRL2_INIT, QMI8658_CTRL3_INIT, QMI8658_CTRL5_INIT, QMI8658_CTRL7_INIT);
    return 0;
}
