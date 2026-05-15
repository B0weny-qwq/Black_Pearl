# Filter 低通滤波模块

## 概述

`Code_boweny/Function/Filter/` 提供面向三轴陀螺仪与三轴地磁数据的软件低通滤波接口。

当前模块特性：

- 使用一阶 IIR 低通
- 内部状态采用 Q8 定点整数
- 不依赖浮点运算
- 陀螺仪与地磁各自维护独立三轴状态
- 首帧有效数据直接灌入状态，首次输出等于输入
- 原始数据接口保持不变，滤波读取通过新增 API 接入

---

## 文件结构

```text
Code_boweny/Function/Filter/
├── Filter.h      # 宏定义、API声明、DOxygen注释
├── Filter.c      # 低通滤波实现
└── README.md     # 本文档
```

---

## 低通公式

模块内部使用的一阶 IIR 公式为：

```c
state += (((input << Q) - state) >> shift);
output = state >> Q;
```

当前默认配置：

```c
#define FILTER_LPF_STATE_Q       8
#define FILTER_GYRO_LPF_SHIFT    2
#define FILTER_MAG_LPF_SHIFT     2
```

说明：

- `Q=8` 表示内部状态保留 8 位小数
- `shift` 越大，滤波越平滑，但响应越慢
- 当前 `gyro` 与 `mag` 默认都使用 `shift=2`

---

## API 说明

### 低通状态复位

```c
void Filter_ResetGyroLowPass(void);
void Filter_ResetMagLowPass(void);
```

用于清空对应通道的内部状态。复位后，下一个有效输入样本会直接作为首帧灌入状态。

### 三轴低通处理

```c
s8 Filter_GyroLowPass(int16 in_x, int16 in_y, int16 in_z,
                      int16 *out_x, int16 *out_y, int16 *out_z);

s8 Filter_MagLowPass(int16 in_x, int16 in_y, int16 in_z,
                     int16 *out_x, int16 *out_y, int16 *out_z);
```

返回值：

- `0`: 成功
- `-1`: 空指针或输入帧无效

输入帧判定规则：

- 三轴全 `0` 视为无效
- 三轴全 `-1` 视为无效

无效输入不会推进内部滤波状态。

---

## 原始接口与滤波接口关系

本模块不替换现有传感器原始读取接口，而是作为上层功能模块接入：

- 原始陀螺仪接口：`QMI8658_ReadGyro()`
- 滤波陀螺仪接口：`QMI8658_ReadGyroFiltered()`
- 原始地磁接口：`QMC6309_ReadXYZ()`
- 滤波地磁接口：`QMC6309_ReadXYZFiltered()`

调用链路如下：

```text
QMI8658_ReadGyroFiltered()
  -> QMI8658_ReadGyro()
  -> Filter_GyroLowPass()

QMC6309_ReadXYZFiltered()
  -> QMC6309_ReadXYZ()
  -> Filter_MagLowPass()
```

---

## 注意事项

### 无 FPU 限制

STC32G 无 FPU，本模块严格使用定点整数实现，不允许引入浮点运算。

### 首帧直通

滤波器首次接收到有效数据时，不做平滑，直接输出原始值，避免初值偏置。

### 初始化后复位状态

`QMI8658_Init()` 与 `QMC6309_Init()` 成功完成后，都会主动复位各自的低通状态，避免重初始化后沿用旧状态。

---

## 相关文档

| 文档 | 路径 |
|------|------|
| 工程总览 | `doc/project_doc/total.md` |
| 开发日志 | `doc/project_doc/date.md` |
| QMI8658 驱动说明 | `Code_boweny/Device/QMI8658/README.md` |
| QMC6309 驱动说明 | `Code_boweny/Device/QMC6309/README.md` |

---

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-23 | v1.0 | 初版实现，新增 gyro/mag 三轴固定点低通滤波接口 |
