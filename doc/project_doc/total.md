/**
 * @file    total.md
 * @brief   Black Pearl v1.1 当前工程总览
 *
 * @author  boweny
 * @date    2026-05-07
 * @version v1.7.52
 *
 * @details
 * 本文档按 2026-05-07 今晚联调版源码重新整理。
 * 当前目标不是姿态融合验证，而是让船在明早前具备可配对、可手动开环驱动、
 * 可回传 `0x12` 状态包的最小可用控船能力。
 *
 * @see     date.md
 */

# Black Pearl v1.1 工程总览

> 当前文档只描述现在这版代码的真实行为。若旧文档、旧日志或旧计划与源码冲突，以当前源码和本文档为准。

## 1. 当前联调目标

今晚目标只有一条主线：先让船开环跑起来。

当前验收重点：

- 遥控器能配对成功
- 手动前进、后退、左转、右转、回中停止真实有效
- 合法协议帧后能回老版 `0x12`
- `A/C/D` 按键入口恢复
- 磁力计可上电、可观察

当前明确不做：

- 陀螺仪 bring-up
- IMU 运行期轮询
- AHRS 姿态融合
- 自动驾驶、巡航、返航、航点业务恢复

## 2. 目录结构

```text
Black_Pearl_v1.1/
├── User/                     # 系统入口、初始化、主循环、功能开关
├── Driver/                   # STC 官方底层驱动
├── App/                      # 官方示例与当前保留的状态灯任务
├── Code_boweny/
│   ├── Device/
│   │   ├── GPS/             # GPS NMEA 解析
│   │   ├── MOTOR/           # PWMA 双电机驱动
│   │   ├── QMC6309/         # 地磁计
│   │   ├── QMI8658/         # IMU（今晚关闭）
│   │   └── WIRELESS/        # LT8920 + 旧遥控器协议移植层
│   └── Function/
│       ├── AHRS/            # 姿态融合（今晚关闭）
│       ├── Filter/          # 定点低通
│       ├── Log/             # UART1 日志
│       └── PID/             # 定点 PID
└── doc/
    └── project_doc/
        ├── total.md
        └── date.md
```

## 3. 当前功能开关

当前集中在 [User/FeatureSwitch.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/User/FeatureSwitch.h)：

```c
#define ENABLE_WIRELESS_MODULE         1
#define ENABLE_GPS_MODULE              1
#define ENABLE_MAG_MODULE              1
#define ENABLE_IMU_MODULE              0

#define ENABLE_SHIP_PROTOCOL_SCHED     1
#define ENABLE_MAG_STANDALONE_POLL     1
#define ENABLE_IMU_AHRS_POLL           0

#define AHRS_TEST_ONLY                 0
#define SHIP_PROTOCOL_POLL_ENABLE      1
#define SHIP_THROTTLE_PWM_ENABLE       1
#define WIRELESS_MINIMAL_TEST_ONLY     0
```

含义：

- 无线、GPS、磁力计启用
- IMU 整条链路关闭
- 无线协议调度启用
- 电机 PWM 真实输出启用

## 4. 当前真实启动顺序

当前 `SYS_Init()` 主路径如下：

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
-> GPS_Init()
-> Sensor_I2C_prepare()
-> QMC6309_Init()
```

当前不会执行：

- `AHRS_Reset()`
- `QMI8658_Init()`
- `QMI8658_RequestReinit()`

## 5. 当前真实主循环

当前主循环在 [User/MainLoop.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/User/MainLoop.c)：

```text
MainLoop_RunOnce()
  -> GPS_Poll()
  -> Wireless_Poll()
  -> ShipProtocol_RunScheduler()
  -> Wireless_SearchSignalPoll()
  -> MAG_StandalonePoll()
  -> Task_Pro_Handler_Callback()
```

当前保留模块职责：

- `GPS_Poll()`：持续消费 UART2 数据
- `Wireless_Poll()`：轮询 LT8920，维持收发状态
- `ShipProtocol_RunScheduler()`：配对、工作信道监听、旧协议截帧、`0x11/0x12` 业务
- `Wireless_SearchSignalPoll()`：低频无线搜索/扫描辅助
- `MAG_StandalonePoll()`：低频打印 QMC6309 原始值，仅用于可见性验证
- `Task_Pro_Handler_Callback()`：当前只驱动 `P3.6` 状态灯闪烁

## 6. 当前无线业务状态

当前无线业务文件：

- [ship_protocol.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.c)
- [ship_protocol.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.h)

当前保持的协议集合：

- `0x0F`：配对响应
- `0x10`：配对请求
- `0x11`：手动控制 + 按键
- `0x12`：状态/GPS 回传
- `0x13/0x14/0x15`：仅保留兼容解析入口，不恢复自动驾驶动作

### 6.1 配对

当前配对行为对齐老版核心节奏：

- 固定发送 10 次 `PAIR_REQ(0x10)`
- 固定 seed：`65 65 A0 65`
- 派生工作信道：`13`
- 派生 key：`32/30`
- 第 10 次后先只写同步寄存器，等待 30 个调度节拍，再切工作 RX
- 配对窗口内收到合法 `PAIR_RSP(0x0F)` 后置 `paired=1`

### 6.2 手动控制

当前 `0x11` 恢复老版开环判定：

- 比较 `abs(frontBack-100)+5` 与 `abs(leftRight-100)`
- 前后量主导时进入前进/后退
- 左右量主导时进入左转/右转
- 摇杆回中时停机

当前不使用：

- 姿态角
- 航向角
- 陀螺仪修正
- 数据融合结果

### 6.3 按键

当前按键行为：

- `A`：保留老版灯控入口，但板级灯引脚未确认，只打印提示日志
- `B`：保持 no-op
- `C`：150ms 前冲
- `D`：150ms 后退
- `E`：兼容入口保留，不启用巡航/自动驾驶副作用

补充说明：
- 当前 `C/D` 是按“今晚先让船开环动起来”的目标映射为主推进电机短脉冲。
- 老版 `C/D` 原始实现实际是 `P0.2/P0.3` 独立脉冲输出；而当前 v1.1 工程里 `P0.2/P0.3` 已被 `UART4_SW(P02_P03)` 复用占用，板级用途未确认前不能直接恢复老版 GPIO 版本。

### 6.4 状态回传

当前每收到一帧合法协议帧，都会立即回发一次 `0x12`。

`0x12` 保持老版 15 字节格式，不新增字段：

- 卫星数
- 航向角
- 经度方向 + 经度整数/小数部分
- 纬度方向 + 纬度整数/小数部分
- 电量字节
- 保留自动驾驶状态字节

补充说明：
- 当前 `0x12` 发完后会立即重开工作信道 RX，避免状态回包后出现额外接收空窗。

## 7. 传感器状态

### 7.1 GPS

- 启用
- 路由：UART2 `P1.0/P1.1`
- 用于无线 `0x12` 状态包回传
- 当前 `ShipProtocol_SendGpsOnce()` 除了保留老版 `0x12` 15 字节 payload 外，还会打印正式 `gps state` 日志：
  - `fix`
  - `sat`
  - `lon`
  - `lat`
  - `angle`
  - `power`
  - `seq`
- 该日志仅用于联调观测和上位机解析，不改变空口协议格式

### 7.2 QMC6309

- 启用
- 仅做上电初始化和低频轮询可见性验证
- 不参与控船闭环

### 7.3 QMI8658

- 关闭
- 不初始化
- 不轮询
- 不进入 AHRS
- 不参与控船

## 8. 电机与资源占用

电机资源：

- 左电机：`PWM3P_2=P2.4 / PWM3N_2=P2.5`
- 右电机：`PWM4P_2=P2.6 / PWM4N_2=P2.7`

无线资源：

- `SPI4`: `P3.2/P3.3/P3.4/P3.5`
- `P5.0`: `RST`
- `P5.1`: `ANT_SEL`
- `P1.3`: `RXEN`
- `P5.4`: `TXEN`
- `P0.0`: `ADC_CH8`

系统资源：

- `UART1`: 日志
- `UART2`: GPS
- `Timer0`: 1ms 节拍
- `Timer1`: UART1 波特率
- `Timer2`: UART2 波特率

## 9. 当前已知风险

以下两项当前仍需现场确认：

1. `A` 键船灯引脚
   - 老版是 `P31` 或 `P22`
   - 当前仓只有 `P3.6` 状态灯事实，不足以证明它就是船灯
2. 电机正反极性
   - 当前 `ship_protocol.c` 已恢复老版动作判定
   - 但“前进/左转”映射到新 `Motor` 正负号后是否与实船一致，必须以现场为准

## 10. 文档入口

- [date.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)
- [WIRELESS/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/README.md)
- [Motor/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/Motor/README.md)

## 11. 版本记录

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-07 | v1.7.52 | 二次复核老版无线业务后，补回 `0x12` 回传后的工作 RX 立即恢复、长静默后的开环安全恢复节奏，并修正左右转极性映射；同时明确 `C/D` 以当前电机 PWM 脉冲语义为准，不恢复老版独立脉冲链路。 |
| 2026-05-07 | v1.7.51 | 切到今晚开环控船联调版：关闭 IMU/AHRS，恢复无线 `0x11` 手动动作和 `0x12` 老版回传，磁力计只保留可见性轮询。 |
| 2026-05-07 | v1.7.50 | QMI8658 继续按 5.1 凌晨老版阻塞路径复测。 |
