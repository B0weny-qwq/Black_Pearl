# App / Driver 层职责说明

## 概述

当前工程的 `App/` 与 `Driver/` 并不是主业务实现层，而是底层支撑与官方示例层：

- `Driver/`：提供 STC32G 外设寄存器驱动、中断和基础缓冲能力
- `App/`：提供 STC 官方示例初始化与板级演示模块
- `User/`、`Code_boweny/`：承载当前船控、无线、AHRS、GPS 等真实业务逻辑

## 当前真实运行关系

```text
main()
  -> SYS_Init()
       -> APP_config()
       -> log_init()
       -> GPS_Init()
  -> MainLoop_Bootstrap()
  -> while(1)
       MainLoop_RunOnce()
```

其中：

- `APP_config()` 当前仅保留 `Lamp_init()`
- `Driver/UART` 提供 `COM1/COM2`、`TXn_Buffer/RXn_Buffer`
- `Log.c` 复用 UART1
- `GPS.c` 复用 UART2 的 `RX2_Buffer`

## 当前职责边界

### Driver 层

- 只做寄存器配置、中断收发、基础缓冲和硬件抽象
- 不处理 GPS NMEA、无线协议、AHRS、控制策略
- `STC32G_UART_Isr.c` 只搬运字节，不做业务解析

### App 层

- 主要保留官方示例与板级演示能力
- 当前项目默认不依赖大多数 `App/src/APP_*.c` 作为主链路
- 重新启用某个示例前，必须先检查是否抢占 UART1/UART2/Timer2/I2C 等当前业务资源

## 与 graphify 对齐的关键关系

- `SYS_Init() -> APP_config()`
- `Driver/isr/STC32G_UART_Isr.c` 写入 `RX2_Buffer`
- `GPS.c` 从 `RX2_Buffer` 增量取字节并进入 `GPS FIFO -> NMEA Parser`
- `Log.c` 通过 UART1 输出诊断文本

## 当前工程的资源约束

- UART1：日志输出
- UART2：GPS 接收
- Timer2：为 UART2/GPS 保留，不能再被 `APP_AD_UART` 等示例随意占用
- I2C：由传感器链路复用，示例模块若启用需重新核对

## 后续整理原则

- `Driver/` 注释以“硬件能力/中断语义/缓冲约束”为主
- `App/` 注释以“示例用途/当前是否启用/资源冲突风险”为主
- 不把业务策略错误地下沉到 `App/Driver` 层文档里
