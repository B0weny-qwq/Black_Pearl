# Black Pearl v1.1

Black Pearl v1.1 是一个基于 STC32G MCU 的嵌入式控制工程，使用 Keil MDK / C251 开发。当前固件集成了 GPS、IMU、地磁计、无线通信、电机 PWM 输出、日志、滤波和定点 PID 控制等模块。

工程采用分层结构组织：`User/` 负责系统入口、初始化、主循环和任务调度；`Driver/` 保存 STC 官方底层驱动；`App/` 保存官方示例应用；`Code_boweny/` 保存项目自定义模块。

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
| ADC 采样 | `P0.0 / ADC_CH8` |
| 电机输出 | PWMA CH3 / CH4 |

## 目录结构

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
│       └── Motor/            # 双电机 PWM 驱动
├── RVMDK/                    # Keil 工程文件
└── doc/                      # 项目文档
```

## 已启用模块

### GPS

GPS 模块通过 UART2 接收 NMEA0183 数据，支持 `GGA`、`RMC`、`GSA`、`GSV`、`VTG` 语句。解析器使用内部 FIFO 和逐字符状态机，只接收通过 XOR 校验的完整语句，上层通过 `GPS_GetState()` 读取定位状态。

### IMU 与地磁计

工程当前使用 `QMI8658` 作为 IMU，使用 `QMC6309` 作为地磁计。两颗器件共用硬件 I2C 总线 `P1.4 / P1.5`。

### 无线通信

无线模块基于 `LT8920 + KCT8206L`，使用 SPI4。当前实现为单芯片半双工通信；协议主路径按 `Wireless_other` 移植，不把遥控器协议改成新格式。

当前无线业务按 `Wireless_other` 移植兼容：10 次 `PAIR_REQ(0x10)` 后先停在配对信道 idle，只写老版使用的 `reg36/reg39` 并保留 `reg37/reg38` 默认值；默认 seed `65 65 A0 65` 派生为 `work_ch=13`、`key=32/30`。等待 30 个调度 tick 后再打开工作 RX，RX 空闲时按旧版 `Rx_TimeOUT > 10` 周期性重开接收窗口。

### 电机驱动

电机驱动使用 `PWMA CH3 / CH4` 控制左右双电机。

| 电机 | PWM 引脚 |
|------|----------|
| 左电机 | `P2.4 / P2.5` |
| 右电机 | `P2.6 / P2.7` |

速度范围为 `-1000` 到 `+1000`，模块内部自动限幅。为避免上电误动作，电机模块默认不在系统启动阶段自动初始化。

### PID 控制器

PID 模块提供通用位置式 PID 控制器，参数使用 Q10 定点格式：

```text
1024 = 1.0
```

该模块只计算控制输出，不直接操作 PWM、GPIO、传感器或通信外设，适合作为速度环、航向环和姿态控制环的基础组件。

## 启动流程

当前 `SYS_Init()` 主要流程：

```text
EAXSFR()
-> GPIO_config()
-> Switch_config()
-> Timer_config()
-> ADC_config()
-> UART_config()
-> I2C_config()
-> EA = 1
-> APP_config()
-> log_init()
-> Wireless_Init()
-> GPS_Init()              [skipped when WIRELESS_MINIMAL_TEST_ONLY=1]
-> Sensor_I2C_prepare()
-> QMC6309_Init()
-> QMI8658_PowerOnSelfTest()
```

## 主循环

```c
Wireless_MinimalTestUnit();

while (1)
{
    /* GPS/IMU/MAG are skipped when WIRELESS_MINIMAL_TEST_ONLY=1. */
    GPS_Poll();
    Wireless_Poll();
    ShipProtocol_RunScheduler();
    Wireless_SearchSignalPoll();
    Task_Pro_Handler_Callback();
    IMU_HighRatePoll();
}
```

主循环负责 GPS 数据解析、无线轮询、船端协议处理、无线信号搜索、Timer0 任务调度和 IMU 高频采样。

## 资源占用

| 资源 | 用途 |
|------|------|
| UART1 | 日志输出 |
| UART2 | GPS |
| I2C | QMI8658 / QMC6309 |
| SPI4 | LT8920 无线模块 |
| P0.0 / ADC_CH8 | 船体模拟量采样，经无线 `0x12` 原 power 字节回传，并打印 raw/电压估算值 |
| Timer0 | 1 ms 系统节拍 |
| Timer1 | UART1 波特率发生器 |
| Timer2 | UART2 波特率发生器 |
| PWMA CH3 | 左电机 PWM |
| PWMA CH4 | 右电机 PWM |

## 开发注意事项

- `Driver/` 保存 STC 官方底层库，原则上不修改。
- P0.0 电池电压估算由 `SHIP_ADC_REF_MV`、`SHIP_BAT_DIV_NUM`、`SHIP_BAT_DIV_DEN` 配置；无线帧仍只发送老版 1 字节 power 字段。
- 无线业务是 `Wireless_other` 移植兼容，不是新协议重构；`Wireless_Receive()` 返回 RF payload，旧业务截帧和固定 `0x12` 回包在 `ship_protocol.c` 内完成。
- 固件模块中应避免使用浮点运算。
- 日志输出中不要使用 `%f`。
- UART2 依赖 Timer2，因此会复用 Timer2 的示例模块需要保持关闭。
- 电机模块占用 `P2.4` 到 `P2.7` 后，这些引脚不应再复用为 SPI、LCM 或普通 GPIO。
- 无线模块自行初始化 SPI4，不使用原示例中的 SPI 初始化流程。

## 相关文档

- `doc/project_doc/total.md`：工程总览
- `doc/project_doc/date.md`：变更记录
- `doc/build_doc/README_GPS.md`：GPS 模块说明
- `doc/build_doc/README_wireless.md`：无线模块说明
- `Code_boweny/Device/Motor/README.md`：电机驱动说明
- `Code_boweny/Function/PID/README.md`：PID 控制器说明

## 当前版本

当前工程版本：`Black Pearl v1.1`

本文档基于 2026-05-06 的工程状态整理。
