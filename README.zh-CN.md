# Black Pearl v1.1

Black Pearl v1.1 是一个基于 STC32G MCU 的嵌入式控制工程，使用 Keil MDK / C251 开发。当前工程已经接入 GPS、IMU、地磁计、无线通信、电机 PWM 输出、日志、滤波和定点 PID 控制模块。

工程整体采用分层结构组织：`User/` 负责系统入口和调度，`Driver/` 保存 STC 官方底层驱动，`App/` 保存官方示例应用层，`Code_boweny/` 保存项目自定义模块。

## 项目概览

| 项目 | 说明 |
|------|------|
| MCU | STC32G |
| 主频 | 24 MHz |
| 编译环境 | Keil MDK / C251 |
| 日志串口 | UART1，`P3.0 / P3.1` |
| GPS 串口 | UART2，`P1.0 / P1.1` |
| I2C 总线 | `P1.4 / P1.5` |
| 无线芯片 | LT8920 + KCT8206L |
| 电机输出 | PWMA CH3 / CH4 |

## 工程结构

```text
Black_Pearl_v1.1/
├── User/                     # 系统入口、初始化、主循环、任务调度
├── Driver/                   # STC 官方底层外设库
├── App/                      # 官方示例应用层
├── Code_boweny/              # 项目自定义模块
│   ├── Function/
│   │   ├── Log/              # UART1 日志系统
│   │   ├── Filter/           # Q8 定点低通滤波
│   │   └── PID/              # Q10 定点 PID 控制器
│   └── Device/
│       ├── GPS/              # GPS NMEA0183 解析
│       ├── QMI8658/          # IMU 驱动
│       ├── QMC6309/          # 地磁计驱动
│       ├── WIRELESS/         # LT8920 无线驱动
│       └── MOTOR/            # 双电机 PWM 驱动
├── RVMDK/                    # Keil 工程文件
└── doc/                      # 项目文档
```

## 当前已启用模块

### GPS

GPS 模块通过 UART2 接收 NMEA0183 数据，当前支持 `GGA`、`RMC`、`GSA`、`GSV` 和 `VTG` 语句。

模块内部使用 FIFO 和增量状态机解析串口数据，只接受通过 XOR 校验的完整语句。上层通过 `GPS_GetState()` 读取解析后的定位状态。

### IMU 与地磁计

当前工程使用：

- `QMI8658` 作为 IMU，在主循环中高频轮询
- `QMC6309` 作为地磁计

两个设备共用硬件 I2C 总线 `P1.4 / P1.5`。

### 无线通信

无线模块基于 `LT8920 + KCT8206L`，使用 SPI4 通信。

当前特性：

- 单芯片半双工通信
- 默认常驻 RX
- 发送时临时切换到 TX，发送完成后自动回到 RX
- 启动时进行双天线扫描
- 使用寄存器轮询，不依赖外部中断

### 电机驱动

电机驱动模块使用 `PWMA CH3 / CH4` 控制左右双电机。

| 电机 | PWM 引脚 |
|------|----------|
| 左电机 | `P2.4 / P2.5` |
| 右电机 | `P2.6 / P2.7` |

速度范围为 `-1000` 到 `+1000`，模块内部会自动限幅。电机模块默认不会在系统启动时自动初始化，以避免上电后电机误动作。

### PID 控制器

PID 模块提供通用位置式 PID 控制器，参数使用 Q10 定点格式：

```text
1024 = 1.0
```

该模块只负责计算控制输出，不直接操作 PWM、GPIO、传感器或通信外设，适合作为后续速度环、航向环和姿态控制环的基础组件。

## 启动流程

当前 `SYS_Init()` 的主要流程如下：

```text
EAXSFR()
-> GPIO_config()
-> Switch_config()
-> Timer_config()
-> UART_config()
-> I2C_config()
-> EA = 1
-> APP_config()
-> log_init()
-> GPS_Init()
-> Wireless_Init()
-> Sensor_I2C_prepare()
-> QMC6309_Init()
-> QMI8658_PowerOnSelfTest()
```

## 主循环

```c
Wireless_MinimalTestUnit();

while (1)
{
    GPS_Poll();
    Wireless_Poll();
    ShipProtocol_Poll();
    Wireless_SearchSignalPoll();
    Task_Pro_Handler_Callback();
    IMU_HighRatePoll();
}
```

当前主循环负责 GPS 数据解析、无线模块轮询、船端协议处理、无线信号搜索、Timer0 任务调度和 IMU 高频采样。

## 资源占用

| 资源 | 用途 |
|------|------|
| UART1 | LOG 输出 |
| UART2 | GPS |
| I2C | QMI8658 / QMC6309 |
| SPI4 | LT8920 无线模块 |
| Timer0 | 1 ms 系统节拍 |
| Timer1 | UART1 波特率发生器 |
| Timer2 | UART2 波特率发生器 |
| PWMA CH3 | 左电机 PWM |
| PWMA CH4 | 右电机 PWM |

## 开发说明

- `Driver/` 目录保存 STC 官方底层库，原则上不修改。
- 固件模块中应避免使用浮点运算。
- 日志输出中不要使用 `%f`。
- UART2 依赖 Timer2，因此会复用 Timer2 的示例模块需要保持关闭。
- MOTOR 模块占用 `P2.4` 到 `P2.7` 后，这些引脚不应再复用为 SPI、LCM 或普通 GPIO。
- 无线模块自行初始化 SPI4，不使用原示例中的 SPI 初始化流程。

## 文档

更多详细说明见：

- `doc/project_doc/total.md`：工程总览
- `doc/project_doc/date.md`：变更记录
- `doc/build_doc/README_GPS.md`：GPS 模块说明
- `doc/build_doc/README_wireless.md`：无线模块说明
- `Code_boweny/Device/MOTOR/README.md`：电机驱动说明
- `Code_boweny/Function/PID/README.md`：PID 控制器说明

## 当前版本

当前工程版本：`Black Pearl v1.1`

本文档基于 2026-04-27 的工程状态整理。
