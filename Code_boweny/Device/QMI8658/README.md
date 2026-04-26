# QMI8658 6轴IMU驱动

## 概述

QMI8658 是一款 6 轴惯性测量单元（3轴加速度计 + 3轴陀螺仪），通过 I2C 接口与 STC32G 通信。本驱动支持地址自动探测、软复位、就绪检测、数据有效性检查、分级 LOG 调试等功能。

||| 项目 | 说明 |
||------|------|
|| 芯片 | QMI8658 |
|| 总线 | STC32G 硬件 I2C (P1.4=SDA, P1.5=SCL) |
|| I2C 地址 | 0x6B (SA0=浮空/高) / 0x6A (SA0=地) |
|| 默认配置 | ACC ±4G / ODR=117Hz, GYRO ±128°/s / ODR=117Hz |
|| 加速度灵敏度 | 8192 LSB/g (±4G) |
|| 陀螺仪灵敏度 | 256 LSB/°/s (±128°/s) |
|| 系统时钟 | Fosc = 24MHz |

---

## 文件结构

```
Code_boweny/Device/QMI8658/
├── QMI8658.h      # 寄存器定义、宏、API声明、DOxygen注释
├── QMI8658.c      # 驱动实现、LOG调试
└── README.md      # 本文档
```

---

## 硬件连接

```
STC32G          QMI8658
-------         -------
P1.4 (SDA)  <->  SDA
P1.5 (SCL)  <->  SCL
3.3V         <->  VDD
GND          <->  GND
(SA0引脚)   <->  浮空或接地 (决定I2C地址)
```

---

## 快速使用

### 1. 初始化 (System_init.c 或 main.c)

```c
#include "STC32G_I2C.h"
#include "QMI8658.h"

// 先确保 I2C 已初始化
I2C_config();

// 初始化 IMU
if (QMI8658_Init() != 0) {
    // 初始化失败，检查 LOG 输出
    LOGE("MAIN", "QMI8658 init fail");
    while(1);
}

LOGI("MAIN", "QMI8658 ready");
```

### 2. 读取数据

```c
// 读取全部9轴数据 (加速度+陀螺仪)
int16 ax, ay, az, gx, gy, gz;
if (QMI8658_ReadAll(&ax, &ay, &az, &gx, &gy, &gz) == 0) {
    LOGI("IMU", "ax=%d ay=%d az=%d", ax, ay, az);
    LOGI("IMU", "gx=%d gy=%d gz=%d", gx, gy, gz);
}

// 或分别读取
int16 ax, ay, az;
QMI8658_ReadAcc(&ax, &ay, &az);

int16 gx, gy, gz;
QMI8658_ReadGyro(&gx, &gy, &gz);

// 读取温度
int16 temp;
QMI8658_ReadTemp(&temp);
```

### 3. 最小测试单元

```c
void IMU_TestTask(void)
{
    int16 ax, ay, az, gx, gy, gz;
    u8 id;

    // 读取芯片ID验证通信
    id = QMI8658_ReadID();
    if (id != QMI8658_CHIP_ID_VALUE) {
        LOGE("IMU", "ID error id=0x%02X", id);
        return;
    }
    LOGI("IMU", "WHO_AM_I=0x%02X OK", id);

    // 读取加速度
    if (QMI8658_ReadAcc(&ax, &ay, &az) == 0) {
        // 转换为物理量 (定点整数方式, 无FPU)
        // ±4G量程: 灵敏度8192 LSB/g
        // 例如: ax=8192 表示 1g
        LOGI("IMU", "acc: %d %d %d (LSB/g)", ax, ay, az);
    }

    // 读取陀螺仪
    if (QMI8658_ReadGyro(&gx, &gy, &gz) == 0) {
        // ±128°/s量程: 灵敏度256 LSB/°/s
        // 例如: gx=256 表示 1°/s
        LOGI("IMU", "gyro: %d %d %d (LSB/dps)", gx, gy, gz);
    }
}
```

---

## 当前已验证状态

截至 `2026-04-23`，QMI8658 已在本工程中完成以下实机验证：

- 已加入 `RVMDK/STC32G-LIB.uvproj` 编译，Keil 工程可正常识别并通过编译
- 与 QMC6309 共用 STC32G 硬件 I2C 总线 `P1.4/P1.5`，两颗器件可同时初始化
- 启动顺序固定为 `APP_config() -> log_init() -> Sensor_I2C_prepare() -> QMC6309_Init() -> QMI8658 自检`
- 上电阶段可稳定完成 `WHO_AM_I=0x05` 校验，并读出首帧加速度数据
- `main()` 空循环中已接入高频 `QMI8658_ReadAcc()` 轮询，串口按抽样比例输出加速度

### 当前运行配置

- I2C 地址：`0x6B`
- 总线：硬件 I2C，`P1.4=SDA`，`P1.5=SCL`
- 当前 bring-up 配置：
  - `CTRL2 = 0x07`
  - `CTRL3 = 0x07`
  - `CTRL5 = 0x11`
  - `CTRL7 = 0x03`
- Ready 轮询：`QMI8658_Wait_AccReady()` 只检查 `STATUS0.bit0(aDA)`，避免混用 `STATUSINT`
- 启动自检失败时仅记日志，不阻断系统继续启动

### 串口验证示例

```text
[IMU] I: WHO_AM_I=0x05 i2c_addr=0x6B
[IMU] I: ready addr=0x6B id=0x05 acc=-3672 -3700 -3250
[IMU] I: acc=-11294 -11739 -6268
[IMU] I: acc=-9100 -10224 3432
```

这组日志说明：

- I2C 设备地址探测正常
- QMI8658 与 QMC6309 共线工作正常
- 上电自检成功
- 高频加速度读取链路已经跑通

---

## API 参考

### QMI8658_Init

```c
s8 QMI8658_Init(void);
```

完整初始化流程：地址探测 → WHO_AM_I检测 → RESET_STATE检测 → 软复位(必要时) → 分阶段配置写入 → 读回验证 → 等待数据就绪。

返回: `0`=成功, `-1`=失败。

### QMI8658_ReadID

```c
u8 QMI8658_ReadID(void);
```

读取芯片标识寄存器，期望值 `0x05`。返回 `0xFF` 表示 I2C 通信失败。

### QMI8658_ReadAcc

```c
s8 QMI8658_ReadAcc(int16 *x, int16 *y, int16 *z);
```

读取三轴加速度。默认量程 ±4G，灵敏度 8192 LSB/g。返回: `0`=成功, `-1`=失败。

### QMI8658_ReadGyro

```c
s8 QMI8658_ReadGyro(int16 *x, int16 *y, int16 *z);
```

读取三轴角速度。默认量程 ±128°/s，灵敏度 256 LSB/°/s。返回: `0`=成功, `-1`=失败。

### QMI8658_ReadGyroFiltered

```c
s8 QMI8658_ReadGyroFiltered(int16 *x, int16 *y, int16 *z);
```

读取低通滤波后的三轴角速度。内部调用顺序为：

```text
QMI8658_ReadGyro() -> Filter_GyroLowPass()
```

说明：

- 原始 `QMI8658_ReadGyro()` 接口保持不变
- 首帧有效数据直接作为滤波输出
- 后续样本按 `Function/Filter` 模块的一阶 IIR 低通进行平滑

### QMI8658_ReadTemp

```c
s8 QMI8658_ReadTemp(int16 *temp);
```

读取芯片温度。换算公式: 实际温度(°C) = `temp / 256 + 25`。STC32G 无 FPU，外部需自行转换。

```c
// 示例: 定点整数温度输出 (精度 0.1°C)
int16 temp_raw;
if (QMI8658_ReadTemp(&temp_raw) == 0) {
    int temp_c = ((int32)temp_raw * 10 / 256) + 250;
    LOGI("IMU", "temp=%d.%d", temp_c/10, (temp_c<0?(-temp_c)%10:temp_c%10));
}
```

### QMI8658_ReadAll

```c
s8 QMI8658_ReadAll(int16 *ax, int16 *ay, int16 *az,
                   int16 *gx, int16 *gy, int16 *gz);
```

一次读取 12 字节（加速度+陀螺仪），效率高于分别调用 ReadAcc + ReadGyro。返回: `0`=成功, `-1`=失败。

### QMI8658_Enable / QMI8658_Disable

```c
s8 QMI8658_Enable(void);
s8 QMI8658_Disable(void);
```

使能/禁用传感器。进入掉电模式可降低功耗。

### QMI8658_Wait_AccReady / QMI8658_Wait_GyroReady

```c
s8 QMI8658_Wait_AccReady(u16 timeout_ms);
s8 QMI8658_Wait_GyroReady(u16 timeout_ms);
```

轮询等待加速度/陀螺仪数据就绪（STATUS0.aDA/gDA）。超时返回 `-1`。

### QMI8658_BusRecover

```c
void QMI8658_BusRecover(void);
```

当 I2C 总线死锁（SDA/SCL 被拉低）时调用，发送 9 个时钟脉冲恢复总线。

---

## 量程和灵敏度

### 加速度

| 量程 | 灵敏度 | 说明 |
|------|--------|------|
| ±2G | 16384 LSB/g | 高精度低速场景 |
| ±4G | 8192 LSB/g | **默认**，常规运动 |
| ±8G | 4096 LSB/g | 中高速运动 |
| ±16G | 2048 LSB/g | 高冲击场景 |

### 陀螺仪

| 量程 | 灵敏度 | 说明 |
|------|--------|------|
| ±16°/s | 2048 LSB/°/s | 极低速旋转 |
| ±32°/s | 1024 LSB/°/s | 低速旋转 |
| ±64°/s | 512 LSB/°/s | - |
| ±125°/s | 256 LSB/°/s | - |
| ±250°/s | 128 LSB/°/s | 常规运动 |
| ±512°/s | 64 LSB/°/s | 高速旋转 |
| ±1024°/s | 32 LSB/°/s | 极高速 |
| ±2048°/s | 16 LSB/°/s | 超高速 |

修改默认量程：在 `QMI8658_Init()` 调用前修改 `QMI8658.h` 中的 `QMI8658_CTRL2_INIT` / `QMI8658_CTRL3_INIT` 宏。

---

## LOG 日志标签

驱动内部使用 `[IMU]` 标签输出分级日志：

| 级别 | 标签 | 含义 |
|------|------|------|
| INFO | `[IMU] I:` | 初始化成功、关键状态变化 |
| WARN | `[IMU] W:` | NACK、读回校验不一致、数据无效 |
| ERROR | `[IMU] E:` | I2C 失败、WHO_AM_I 错误、初始化失败 |
| DEBUG | `[IMU] D:` | 初始化每一步详细状态 |

---

## 依赖关系

```
QMI8658.h
├── STC32G_I2C.h    (I2C_WriteNbyte, I2C_ReadNbyte, Get_MSBusy_Status)
├── STC32G_Delay.h  (delay_ms 延时)
├── STC32G_GPIO.h   (BusRecover 中 GPIO 引脚操作: P1_MODE_OUT_OD, P1_PULL_UP_ENABLE, P14, P15)
└── Log.h           (LOGI/LOGW/LOGE/LOGD 日志宏)
```

> 注意: 本驱动直接调用 Driver 层的 `I2C_WriteNbyte()` / `I2C_ReadNbyte()`，不重复实现 I2C 基础操作，遵循 Driver 层不可修改原则。

`QMI8658_ReadGyroFiltered()` 额外依赖 `Code_boweny/Function/Filter/Filter.h` 提供的软件低通接口。

---

## 注意事项

### 禁止浮点运算

STC32G 无 FPU，所有数据以 int16 整数传递。如需转换为物理量，应在应用层用定点整数方式处理：

```c
// 错误
float accel_g = ax / 8192.0f;  // 禁止浮点运算

// 正确: 定点整数方式输出 (以 0.01g 为单位)
// ax=8192 → 100 (即 1.00g)
int accel_01g = (ax * 100) / 8192;
LOGI("IMU", "acc=%d.%02dg", accel_01g/100, accel_01g%100);
```

### 调用顺序

```
 I2C_config() → QMI8658_Init() → QMI8658_ReadAcc/ReadGyro/ReadGyroFiltered/ReadAll/...
```

### 地址自动切换

驱动初始化时自动尝试两个地址：先 0x6B，失败则尝试 0x6A。外部可访问 `QMI8658_I2C_Addr` 变量确认最终使用的地址。

### I2C 总线共享

QMI8658 和 QMC6309 共用同一 I2C 总线（P1.4/P1.5），可同时使用。驱动互不影响。

### STATUS0 轮询注意

`STATUS0` 寄存器用于检测数据就绪。由于 QMI8658 在读取 `STATUSINT` 后会清除锁存状态，`QMI8658_Wait_AccReady` 内部使用 `STATUS0`（非 STATUSINT），避免了锁存问题。

---

## 相关文档

| 文档 | 路径 |
|------|------|
| 工程总览 | `doc/project_doc/total.md` |
| 开发日志 | `doc/project_doc/date.md` |
| IMU硬件文档 | `doc/device_doc/IMU_QMI8658.md` |
| QMC6309地磁计 | `Code_boweny/Device/QMC6309/README.md` |
| LOG系统文档 | `doc/device_doc/LOG.md` |

---

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-22 | v1.0 | 初版驱动，支持地址探测/软复位/量程配置/ODR配置/LOG调试 |
| 2026-04-23 | v1.1 | 完成 Keil 工程接入、共享 I2C 上电自检、高频加速度读取验证，日志口径收敛到 `WHO_AM_I` / `STATUS0` / `acc` |
| 2026-04-23 | v1.2 | 新增 `QMI8658_ReadGyroFiltered()` 软件低通接口，接入 `Function/Filter` 模块 |

---
