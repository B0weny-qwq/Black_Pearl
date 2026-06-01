/**
 * @file    QMC6309_port.c
 * @brief   QMC6309 共享 I2C 端口适配实现。
 *
 * @details
 * 当前磁力计与 QMI8658 共用同一条传感器 I2C，因此本文件直接复用
 * `QMI8658_port` 的底层实现，只保留磁力计侧命名与接口包装。
 *
 * 这样可以保证：
 * - 总线恢复策略一致；
 * - 引脚模式与 I2C 路由一致；
 * - 上层 `QMC6309.c` 不需要知道共享总线的实现细节。
 */
/**
 * @note 当前职责边界：
 * - 本文件只提供 QMC6309 对共享 I2C 端口的适配包装。
 * - 底层总线恢复策略直接复用 `QMI8658_port`，保证两颗传感器行为一致。
 * - 磁力计融合、航向估计和导航修正不在本文件内实现。
 */
#include "QMC6309_port.h"

s8 QMC6309Port_Init(void)
{
    /* 复用 IMU 端口后端，让两个传感器共享同一套 I2C 恢复路径。 */
    return QMI8658Port_Init();
}

char *QMC6309Port_BackendName(void)
{
    return QMI8658Port_BackendName();
}

void QMC6309Port_DelayMs(u16 ms)
{
    QMI8658Port_DelayMs(ms);
}

u8 QMC6309Port_BusNeedsRecover(void)
{
    return QMI8658Port_BusNeedsRecover();
}

void QMC6309Port_BusRecover(void)
{
    QMI8658Port_BusRecover();
}

u8 QMC6309Port_WriteReg(u8 addr, u8 reg_addr, u8 reg_val, u8 *err_code)
{
    return QMI8658Port_WriteReg(addr, reg_addr, reg_val, err_code);
}

u8 QMC6309Port_ReadN(u8 addr, u8 start_reg, u8 *buf, u8 len, u8 *err_code)
{
    return QMI8658Port_ReadN(addr, start_reg, buf, len, err_code);
}
