# Black Pearl v1.1

本文件是中文 README。主 README 已统一改为中文，本文件保留为兼容入口，内容与项目总览保持一致。

## 项目简介

Black Pearl v1.1 是一个基于 STC32G MCU 的嵌入式控制工程，使用 Keil MDK / C251 开发。当前固件集成 GPS、IMU、地磁计、无线通信、电机 PWM 输出、日志、滤波和定点 PID 控制模块。

## 主要模块

| 模块 | 路径 | 说明 |
|------|------|------|
| GPS | `Code_boweny/Device/GPS/` | NMEA0183 解析，使用 UART2 |
| IMU | `Code_boweny/Device/QMI8658/` | QMI8658 六轴 IMU 驱动 |
| 地磁计 | `Code_boweny/Device/QMC6309/` | QMC6309 三轴地磁计驱动 |
| 无线 | `Code_boweny/Device/WIRELESS/` | LT8920 + KCT8206L 半双工无线链路 |
| 电机 | `Code_boweny/Device/Motor/` | PWMA CH3/CH4 双电机 PWM 驱动 |
| 日志 | `Code_boweny/Function/Log/` | UART1 分级日志输出 |
| 滤波 | `Code_boweny/Function/Filter/` | Q8 定点一阶低通滤波 |
| PID | `Code_boweny/Function/PID/` | Q10 定点位置式 PID 控制器 |

## 硬件资源

| 资源 | 用途 |
|------|------|
| UART1 | 日志输出 |
| UART2 | GPS |
| I2C `P1.4/P1.5` | QMI8658、QMC6309 |
| SPI4 | LT8920 无线模块 |
| PWMA CH3/CH4 | 左右电机 |

## 开发约束

- 尽量不修改 `Driver/` 官方驱动。
- 固件内避免浮点运算和 `%f` 日志格式。
- UART2 固定占用 Timer2。
- 电机和无线模块占用的引脚不要再被示例程序复用。

更多详细说明请查看各模块目录下的 `README.md`。
