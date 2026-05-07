# WIRELESS 模块说明

## 1. 模块定位

`Code_boweny/Device/WIRELESS/` 负责 `LT8920 + KCT8206L` 无线链路驱动，以及船端旧遥控器协议的移植兼容层。

当前版本的定位很明确：

- 今晚目标是开环控船，让船先能被遥控器真实驱动起来。
- 无线业务严格对齐老版 `ship_Gps_V2.1_20260406-115200\App\Wireless` 的核心手动链路。
- 不做新协议，不改空口命令字，不新增回包字段。
- IMU、陀螺仪、AHRS、数据融合、自动驾驶都不参与今晚主链路。

## 2. 当前启用行为

当前联调档对应的关键开关在 [User/FeatureSwitch.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/User/FeatureSwitch.h)：

- `ENABLE_WIRELESS_MODULE=1`
- `ENABLE_GPS_MODULE=1`
- `ENABLE_MAG_MODULE=1`
- `ENABLE_IMU_MODULE=0`
- `ENABLE_SHIP_PROTOCOL_SCHED=1`
- `ENABLE_MAG_STANDALONE_POLL=1`
- `ENABLE_IMU_AHRS_POLL=0`
- `SHIP_PROTOCOL_POLL_ENABLE=1`
- `SHIP_THROTTLE_PWM_ENABLE=1`
- `WIRELESS_MINIMAL_TEST_ONLY=0`
- `AHRS_TEST_ONLY=0`

含义：

- 无线、GPS、磁力计开启。
- IMU 全关闭，不初始化、不轮询、不重试。
- 遥控器 `0x11` 会进入真实开环电机控制。
- 磁力计仅上电和低频可见，不进入控制闭环。

## 3. 分层结构

| 文件 | 职责 |
|------|------|
| `wireless.h/.c` | 无线管理层：初始化、轮询、收发队列、天线扫描、链路参数切换 |
| `lt8920.h/.c` | LT8920 芯片层：寄存器、FIFO、模式切换、状态读取 |
| `wireless_port.h/.c` | 板级适配层：GPIO、SPI、延时、前端使能、天线选择 |
| `ship_protocol.h/.c` | 旧遥控器协议移植层：配对、收帧、`0x11` 手动控制、`0x12` 状态回传 |

约束：

- 业务层只通过 `wireless.h` 和 `ship_protocol.h` 进入无线主链路。
- `Wireless_Receive()` 返回的是一次 LT8920 射频载荷，不保证等于一帧完整旧协议包。
- 旧协议截帧逻辑由 `ship_protocol.c` 按老版 `WirelessProtocal_Receive_Handle()` 逐字节完成。

## 4. 当前硬件映射

```text
STC32G                  无线模块
------                  --------
P3.2  (SPI4 SCLK)   ->  SPI_CLK
P3.3  (SPI4 MISO)   <-  MISO
P3.4  (SPI4 MOSI)   ->  MOSI
P3.5  (GPIO CS)     ->  SPI_SS
P5.0  (GPIO)        ->  RST
P5.1  (GPIO)        ->  ANT_SEL
P1.3  (GPIO)        ->  RXEN
P5.4  (GPIO)        ->  TXEN
P0.0  (ADC input)   <-  电池采样输入
3.3V                 ->  VCC
GND                  ->  GND
```

说明：

- `CS` 由 GPIO 手动片选。
- `RXEN/TXEN` 控制 `KCT8206L` 收发前端。
- `ANT_SEL` 在 `ANT1/ANT2` 间切换。
- `P0.0 / ADC_CH8` 用于 `0x12` 电量字段采样。

## 5. 启动与主循环

当前真实启动顺序：

```text
SYS_Init()
  -> APP_config()
  -> log_init()
  -> Wireless_Init()
  -> GPS_Init()
  -> Sensor_I2C_prepare()
  -> QMC6309_Init()
```

当前真实主循环：

```text
MainLoop_RunOnce()
  -> GPS_Poll()
  -> Wireless_Poll()
  -> ShipProtocol_RunScheduler()
  -> Wireless_SearchSignalPoll()
  -> MAG_StandalonePoll()
  -> Task_Pro_Handler_Callback()
```

当前主循环中没有：

- `QMI8658_Init()`
- `QMI8658_Service()`
- `AHRS_Reset()`
- `AHRS_Update*()`
- 任何 IMU 重试或融合控制逻辑

## 6. 配对链路

当前配对流程保持老版核心节奏：

1. 船端发送 `PAIR_REQ(0x10)`，固定 10 次。
2. seed 固定为 `65 65 A0 65`。
3. 由 seed 派生：
   - `work_ch=13`
   - `key=32/30`
   - `reg36=0x2020`
   - `reg39=0x1E1E`
4. 第 10 次发完后只执行老版 `RF_Encrypt_Config()` 等效动作：
   - 只写工作同步寄存器
   - 停在配对信道 idle
   - 不自动开 RX
5. 再等待 30 个调度节拍后，切到工作信道 RX。
6. 在配对响应窗口内收到合法 `PAIR_RSP(0x0F)` 后置 `paired=1`。

当前保留的配对关键日志：

```text
[SHIP] I: pair req start retry=0 pair_ch=0x7F seed=6565A065 work_rx=13 key=32/30
[SHIP] I: pair req burst done, wait rsp
[SHIP] I: pair ok, enter work channel rx_ch=13 tx_ch=13
```

## 7. `0x11` 手动控制

当前 `0x11` 恢复的是老版手动开环语义，不做闭环修正：

- 比较 `abs(frontBack-100)+5` 与 `abs(leftRight-100)`。
- 前后量更大时进入前进/后退判定。
- 左右量更大时进入左转/右转判定。
- 摇杆回中时停机。
- 不使用姿态角、航向角、陀螺仪、地磁闭环修正。

当前按键行为：

- `A (0xA3)`：保留老版船灯入口，但当前 v1.1 板级引脚未确认，只记录日志，不硬绑未知引脚。
- `B (0xA5)`：保持老版未启用状态，不新增动作。
- `C (0xA7)`：150ms 短促前冲。
- `D (0xA9)`：150ms 短促后退。
- `E (0xA1)`：保留兼容入口，只打日志，不启用巡航/自动驾驶副作用。

补充说明：
- 当前 `C/D` 是按“今晚先让船开环动起来”的目标映射为主推进电机短脉冲。
- 老版 `C/D` 原始实现实际是 `P0.2/P0.3` 独立脉冲输出；而当前 v1.1 工程里 `P0.2/P0.3` 已被 `UART4_SW(P02_P03)` 复用占用，板级用途未确认前不能直接恢复老版 GPIO 版本。

遥控保护：

- 收到合法 `0x11` 后刷新在线时间戳。
- 超过 `SHIP_THROTTLE_TIMEOUT_MS` 未收到新 `0x11` 时，强制停机。
- 长时间收不到新的 `0x11`` 时，会按开环安全版恢复节奏重新打开工作 RX，但不会恢复老版自动驾驶、巡航或软复位副作用。

## 8. `0x12` 状态回传

当前保持老版 15 字节 payload，不新增字段。

字段来源：

- 卫星数：来自 `GPS_State_t.satellites_used`
- 航向角：来自 `GPS_State_t.course_deg_x100`
- 经纬度：来自 `GPS_State_t.lat_deg1e7/lon_deg1e7`
- 电量字节：来自 `P0.0 / ADC_CH8` 原始值右移 4 位
- 自动驾驶字节：当前固定保留兼容位，不恢复老版自动驾驶副作用

当前行为保持与老版一致：

- 只要收到任意合法协议帧，末尾就立即回发一次 `0x12`
- `0x12` 发完后会立即重开工作信道 RX，避免状态回包后出现额外接收空窗。

## 9. 已知未决项

以下两点当前不能伪装成“已完全确认”：

1. `A` 键船灯引脚未确认。
   - 老版在 `HW_V16` 下用 `P22`
   - 其他老版板型用 `P31`
   - 当前 v1.1 仓只明确存在 `P3.6` 状态灯，不足以证明它就是船灯
2. 电机正反极性需要现场实船验证。
   - 当前 `ship_protocol.c` 已恢复老版动作判定逻辑
   - 已按老版 `MOTOR_POSITIVE=TRUE`、`MOTOR_LEFTRIGHT=FALSE` 修正左右转极性映射
   - 但最终仍需以实船结果确认
3. `C/D` 当前按键语义已固定为主推进电机短脉冲。
   - 不再恢复老版 `P0.2/P0.3` 独立脉冲链路
   - 当前 v1.1 以现有电机 PWM 引脚直接驱动短促前冲/后退为准

## 10. 相关文件

- [wireless.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/wireless.h)
- [wireless.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/wireless.c)
- [lt8920.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/lt8920.h)
- [lt8920.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/lt8920.c)
- [wireless_port.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/wireless_port.h)
- [wireless_port.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/wireless_port.c)
- [ship_protocol.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.h)
- [ship_protocol.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.c)

## 11. 版本记录

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-07 | v1.3 | 二次复核老版无线业务后，补回 `0x12` 回传后的工作 RX 立即恢复、长静默后的开环安全恢复节奏，并修正左右转极性映射；同时明确 `C/D` 以当前电机 PWM 脉冲语义为准，不恢复老版独立脉冲链路。 |
| 2026-05-07 | v1.2 | 切到今晚开环控船联调版：IMU/AHRS 全关闭，恢复 `0x11` 手动开环动作、`A/C/D` 按键入口、合法帧固定 `0x12` 回传，并明确 A 键灯控引脚仍待板级确认。 |
| 2026-05-06 | v1.1 | 完成 LT8920 管理层、旧协议配对链路和 `0x12` 基础回传接入。 |
| 2026-04-26 | v1.0 | 新建 WIRELESS 模块，完成 `LT8920 + KCT8206L` 板级驱动框架。 |
