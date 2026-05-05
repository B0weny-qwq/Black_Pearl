# QMI8658 六轴 IMU 驱动

## 概述

QMI8658 是一款六轴惯性测量单元，包含三轴加速度计和三轴陀螺仪，通过 I2C 与 STC32G 通信。本驱动支持地址自动探测、就绪检测、数据有效性检查、低通滤波读取和日志诊断。

| 项目 | 说明 |
|------|------|
| 芯片 | QMI8658 |
| 总线 | STC32G 硬件 I2C，`P1.4=SDA`，`P1.5=SCL` |
| I2C 地址 | `0x6B` 或 `0x6A` |
| 当前配置 | legacy retest：`CTRL2=0x07`，`CTRL3=0x07` |
| 加速度默认假设 | +/-4g，8192 LSB/g |
| 陀螺仪当前假设 | 2048 LSB/(deg/s) |
| 系统时钟 | Fosc = 24MHz |

## 当前诊断模式

- 当前使用稳定 bring-up 寄存器组合：`CTRL2=0x07`、`CTRL3=0x07`。
- `QMI8658_CLEAR_DATAPATH_ENABLE=0`，跳过额外 `CTRL6/CTRL8/FIFO/CTRL9` 清理路径。
- `QMI8658_SOFT_RESET_ENABLE=0`，正常 bring-up 中不主动写 `RESET=0xB0`。
- `QMI8658_DIAG_ENABLE=0`，初始化阶段不再额外读取 timestamp/temp/raw。
- 若 `ReadAcc()` 重复失败，可用 `QMI8658_DumpRawRegs()` 单字节读取关键寄存器，区分自动递增读问题和数据域异常。

## 文件结构

```text
Code_boweny/Device/QMI8658/
├── QMI8658.h      # 寄存器定义、宏、API 声明
├── QMI8658.c      # 驱动实现和日志诊断
└── README.md      # 本文档
```

## 硬件连接

```text
STC32G          QMI8658
-------         -------
P1.4 (SDA)  <-> SDA
P1.5 (SCL)  <-> SCL
3.3V         <-> VDD
GND          <-> GND
SA0          <-> 浮空/高电平或接地，用于决定 I2C 地址
```

## 快速使用

```c
#include "STC32G_I2C.h"
#include "QMI8658.h"

I2C_config();

if (QMI8658_Init() != 0) {
    LOGE("MAIN", "QMI8658 init fail");
    while (1);
}

LOGI("MAIN", "QMI8658 ready");
```

读取数据：

```c
int16 ax, ay, az, gx, gy, gz;

if (QMI8658_ReadAll(&ax, &ay, &az, &gx, &gy, &gz) == 0) {
    LOGI("IMU", "acc=%d %d %d", ax, ay, az);
    LOGI("IMU", "gyro=%d %d %d", gx, gy, gz);
}
```

## 当前已验证状态

- 已接入 Keil 工程。
- 与 QMC6309 共用 I2C 总线 `P1.4/P1.5`。
- 启动顺序为 `APP_config() -> log_init() -> Sensor_I2C_prepare() -> QMC6309_Init() -> QMI8658 自检`。
- 可稳定读取 `WHO_AM_I=0x05`。
- 主循环中已接入高频加速度读取。

## API

```c
s8 QMI8658_Init(void);
u8 QMI8658_ReadID(void);
s8 QMI8658_ReadAcc(int16 *x, int16 *y, int16 *z);
s8 QMI8658_ReadGyro(int16 *x, int16 *y, int16 *z);
s8 QMI8658_ReadGyroFiltered(int16 *x, int16 *y, int16 *z);
s8 QMI8658_ReadTemp(int16 *temp);
s8 QMI8658_ReadAll(int16 *ax, int16 *ay, int16 *az,
                   int16 *gx, int16 *gy, int16 *gz);
s8 QMI8658_Wait_AccReady(u16 timeout_ms);
s8 QMI8658_Wait_GyroReady(u16 timeout_ms);
void QMI8658_BusRecover(void);
s8 QMI8658_Enable(void);
s8 QMI8658_Disable(void);
void QMI8658_DumpRawRegs(void);
```

返回值约定：

- `0`：成功
- `-1`：失败、超时、参数无效或数据无效

## 滤波读取

```text
QMI8658_ReadGyroFiltered()
  -> QMI8658_ReadGyro()
  -> Filter_GyroLowPass()
```

首帧有效陀螺仪数据直接作为滤波输出，后续数据按 `Function/Filter` 模块的一阶 IIR 低通平滑。

## 注意事项

- 所有物理量转换建议使用定点整数，不要在固件中使用浮点。
- 调用顺序为 `I2C_config() -> QMI8658_Init() -> QMI8658_Read*()`。
- 初始化会自动尝试 `0x6B` 和 `0x6A` 两个地址，最终地址保存在 `QMI8658_I2C_Addr`。
- QMI8658 与 QMC6309 共用同一条 I2C 总线，初始化前应确认 `P1.4/P1.5` 已恢复为 I2C 模式。
- `QMI8658_Wait_AccReady()` 和 `QMI8658_Wait_GyroReady()` 使用 `STATUS0` 数据就绪位，避免误用会清锁存状态的 `STATUSINT`。

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-22 | v1.0 | 初版驱动，支持地址探测、软复位、量程/ODR 配置和日志诊断 |
| 2026-04-23 | v1.1 | 完成 Keil 工程接入、共享 I2C 自检和高频加速度读取验证 |
| 2026-04-23 | v1.2 | 新增 `QMI8658_ReadGyroFiltered()` 软件低通接口 |
