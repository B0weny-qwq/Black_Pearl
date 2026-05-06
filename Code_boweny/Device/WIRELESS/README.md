# WIRELESS 模块说明

## 概述

`Code_boweny/Device/WIRELESS/` 用于驱动单颗 `LT8920` 2.4G 收发芯片，并控制外部 `KCT8206L` 射频前端完成半双工无线收发。

当前实现遵循以下工程约束：

- 不修改 `Driver/` 层。
- 不使用动态内存。
- 对外接口统一返回错误码。
- 硬件相关逻辑集中在 `wireless_port.*`。
- 关键步骤通过 `LOGI/LOGE` 输出。

当前默认运行目标是无线最小业务流程：上电完成 LT8920 自检后，发送配对请求、等待遥控器配对响应，配对成功后在工作信道接收并打印每一帧遥控器 `lr/ud/key` 值。当前不启用电机控制、GPS/IMU/MAG 业务路径。

## 当前硬件接线

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
P0.0  (ADC input)   <-  船体模拟量采样输入
3.3V                 ->  VCC
GND                  ->  GND
```

说明：

- `CS` 当前使用 GPIO 手动控制，不依赖硬件自动片选。
- `RXEN/TXEN` 控制 `KCT8206L` 的收发路径。
- `ANT_SEL` 在 `ANT1/ANT2` 之间切换。
- 当前策略为启动扫描后固定天线，运行中不自动来回切换。

## 模块分层

| 文件 | 职责 |
|------|------|
| `wireless.h/.c` | 对外 API、状态机、接收队列、轮询入口 |
| `lt8920.h/.c` | LT8920 芯片层，负责寄存器、先进先出缓冲区、收发模式和状态读取 |
| `wireless_port.h/.c` | STC32G 板级层，负责 GPIO、SPI、延时、天线和前端控制 |
| `ship_protocol.h/.c` | 船端业务协议和配对调度 |

头文件注释约束：

- `wireless.h` 明确 `Wireless_Receive()` 返回的是 LT8920 射频载荷，不等同于旧业务协议帧。
- `ship_protocol.h` 明确本层是 `Wireless_other/wirelessProtocal.c` 的移植兼容层，不是重构协议。
- 旧遥控器不可修改，后续改动必须保持 `AA | len | cmd | payload | xor | BB` 包格式、seed 派生、工作信道和固定 `0x12` 回包节奏。

## 对外接口

```c
s8 Wireless_Init(void);
s8 Wireless_Deinit(void);
s8 Wireless_Poll(void);
s8 Wireless_Send(const u8 *buf, u8 len);
s8 Wireless_Receive(u8 *buf, u8 buf_len, u8 *out_len);
s8 Wireless_SetAntenna(u8 ant_sel);
s8 Wireless_GetState(Wireless_State_t *state);
s8 Wireless_RescanAntenna(void);
s8 Wireless_SearchSignalPoll(void);
s8 Wireless_RunMinimalTest(void);
```

## 初始化流程

`Wireless_Init()` 完成以下工作：

- 初始化 `SPI4 + GPIO`。
- 复位 LT8920。
- 装载默认寄存器 profile。
- 做寄存器回读自检。
- 扫描 `ANT1/ANT2`。
- 固定较优天线。
- 进入常驻接收模式。

## 主循环轮询

`Wireless_Poll()` 应在主循环高频调用：

- 轮询 LT8920 状态寄存器。
- 发现有效包时读取 FIFO 并放入静态接收队列。
- 发现 CRC 错包时清计数并恢复 RX。
- 发包完成后自动切回 RX。

## 发送与接收

`Wireless_Send()` 流程：

```text
参数检查 -> RX 切到 TX -> 写 FIFO -> 触发发送 -> 等待完成/超时 -> 切回 RX
```

`Wireless_Receive()` 只从模块内部静态队列取包，不直接访问 LT8920 硬件。

注意：`Wireless_Receive()` 返回的是一次 LT8920 射频载荷，协议层不能假设它刚好等于一帧 `AA..BB` 旧业务包。当前由 `ship_protocol.c` 按旧版 `WirelessProtocal_Receive_Handle()` 逐字节截帧。

## 最小自测试

`Wireless_RunMinimalTest()` 用于底层联调阶段确认 SPI 链路和 LT8920 响应正常：

- 读取 `Reg3 / Reg6 / Reg11 / Reg41`。
- 校验关键寄存器签名。
- 用于区分 SPI 链路问题和空口协议问题。

## 当前系统接入点

默认配置为 `AHRS_TEST_ONLY=0`、`WIRELESS_MINIMAL_TEST_ONLY=1`、`SHIP_PROTOCOL_POLL_ENABLE=1`：

```text
SYS_Init()
  -> APP_config()
  -> log_init()
  -> Wireless_Init()
  -> GPS_Init()              [skipped when WIRELESS_MINIMAL_TEST_ONLY=1]

main()
  -> Wireless_MinimalTestUnit()
  -> while(1)
       Wireless_Poll()
       ShipProtocol_RunScheduler()
       Task_Pro_Handler_Callback()
```

最小业务模式不会进入 `Wireless_RunPairTxOnlyTest()` 持续单向发包诊断，也不会调用 `ShipProtocol_Poll()` 兼容轮询分支。

## 配对流程说明

- 配对请求命令为 `cmd=0x10`。
- 配对响应命令为 `cmd=0x0F`。
- 配对请求帧固定为 9 字节：

```text
AA | 06 | 10 | seed0 | seed1 | seed2 | seed3 | xor | BB
```

- 默认配对发射信道为 `0x7F`。
- 默认固定 seed 为 `65 65 A0 65`，由 `SHIP_PAIR_SEED0..3` 宏定义控制。
- `seed[4]` 会派生工作信道和同步/密钥字节；默认 seed 按老版公式应得到 `work_ch=13`、`key=32/30`、`reg36=0x2020`、`reg39=0x1E1E`。
- 当前内部状态机为 `BOOT_WAIT -> PAIR_SEND -> WORK_RX`；第 10 次配对包发完后只执行老版 `RF_Encrypt_Config()` 等效动作，先停在配对信道空闲态并打开 `PAIR_RSP(0x0F)` 有效窗口，等待 30 个调度节拍后才由工作态打开工作接收。
- 工作同步寄存器严格对齐老版 `RF_Encrypt_Config()`：只写 `reg36=key0/key0`、`reg39=key1/key1`，保留 `reg37=0x0380`、`reg38=0x5A5A`，不清 FIFO，不自动打开接收。
- 工作接收打开顺序对齐老版 `LT8920_OpenRx()`：`reg7 idle -> reg52 clear -> reg8=0x6C90 -> reg7 RX`；无有效包时每 10 个调度节拍周期性重开接收，对应旧版 `Rx_TimeOUT > 10` 后重新 `LT8920_OpenRx()`。
- 配对和 `0x12` 回包发送走 `Wireless_SendOnChannel()`，对齐老版 `LT8920_TxData()`：先写 `reg7 idle(channel)`，再清 FIFO、写 FIFO、进入 TX，发送完成后回到空闲态，不自动打开接收。
- TX 前端保持老版方式：`RXEN` 常开，发送时只拉高 `TXEN`，发送完成拉低 `TXEN`。
- 收包解析按老版 `WirelessProtocal_Receive_Handle()` 对齐：在单个射频载荷内逐字节查找 `0xAA`，按长度字段收完整帧，校验 `xor` 和 `0xBB` 后再分发。
- 当前硬件为单颗 LT8920 半双工，配对阶段每约 `300ms` 发送一次配对包，共发送 10 次后进入约 `5s` 响应窗口。
- 当前按旧遥控器兼容逻辑处理：配对等待窗口内收到合法 `PAIR_RSP(0x0F)` 即认为配对成功；若响应载荷长度为 4，只打印该载荷供调试，不再强制要求它等于 seed。
- 配对请求发送失败不会消耗 10 次发送计数；只有成功发送满 10 次并成功写入工作同步寄存器后才进入响应窗口。
- 收到任意合法协议帧后，船体会按老版 `WirelessProtocal_Resolve_Handle()` 末尾行为立即回发一次 `GPS_REPORT(0x12)`；载荷固定为老版 15 字节，power 字段仍在第 14 个载荷字节，不新增任何无线字段。
- 当前 power 字段来自 `P0.0 / ADC_CH8` 的 12 位采样值右移 4 位后的 1 字节压缩结果；串口同步打印 `raw/adc_mv/bat_mv/power`，用于对照老版 `Power_ADC_Get_Level()`。
- 当前测试验收要求必须保留两类日志：配对成功后打印进入工作通道；收到 `THROTTLE(0x11)` 后打印油门、转向和按键数据。
- 配对成功日志固定包含 `pair ok, enter work channel rx_ch=... tx_ch=...`，用于确认船端已经进入老版派生工作信道。
- 遥控器数据日志固定包含 `rc lr=<0-255> ud=<0-255> key=0xXX paired=1` 和 `throttle=<ud> steering=<lr> key=0xXX`。
- 真实 PWM 油门输出由 `SHIP_THROTTLE_PWM_ENABLE` 控制，默认 `0`，当前只打印遥控器数据，不调用 `Motor_SetBothSpeed()`；每帧会打印 `pwm disabled by SHIP_THROTTLE_PWM_ENABLE=0` 作为安全确认。
- 只有把 `User/Config.h` 中 `SHIP_THROTTLE_PWM_ENABLE` 显式改为 `1` 后，才会初始化 `Motor` 并把 `cmd=0x11` 的 `ud` 油门轴映射为左右电机同速 PWM 输出。

关键日志示例：

```text
[SHIP] I: pair req start retry=0 pair_ch=0x7F seed=6565A065 work_rx=13 key=32/30
[SHIP] I: pair req frame=AA 06 10 65 65 A0 65 D3 BB
[SHIP] I: pair req burst done, wait rsp
[SHIP] I: rxdbg pair-sync-idle ch=127 reg7=0x007F reg36=0x2020 reg37=0x0380 reg38=0x5A5A reg39=0x1E1E ...
[SHIP] I: pair sync idle done, wait 30 ticks then work-rx
[SHIP] I: rxdbg work-rx ch=13 reg7=0x008D reg8=0x6C90 reg36=0x2020 reg37=0x0380 reg38=0x5A5A reg39=0x1E1E ...
[SHIP] I: enter work-state rx_ch=... tx_ch=...
[SHIP] I: pair ok, enter work channel rx_ch=13 tx_ch=13
[SHIP] I: pair success paired=1 work_rx=... work_tx=... key=.../... rsp_len=...
[SHIP] I: throttle=142 steering=100 key=0xA0
[SHIP] I: rc lr=100 ud=142 key=0xA0 paired=1
[SHIP] I: pwm disabled by SHIP_THROTTLE_PWM_ENABLE=0
[SHIP] I: adc p0.0 raw=2048 adc_mv=1650 bat_mv=1650 power=0x80
```

## 当前资源占用

| 资源 | 用途 | 备注 |
|------|------|------|
| SPI4 | LT8920 数据收发 | 使用 `P3.2/P3.3/P3.4` |
| P3.5 | LT8920 片选 | GPIO 手动片选 |
| P5.0 | LT8920 RST | 推挽输出 |
| P5.1 | ANT_SEL | 推挽输出 |
| P1.3 | RXEN | 推挽输出 |
| P5.4 | TXEN | 推挽输出 |

## 注意事项

1. 当前默认只完成遥控器配对、工作信道监听和遥控值打印，不启用电机、GPS、IMU、MAG 业务。
2. `P1.3` 会覆盖系统默认的 `P1.0~P1.3` 高阻配置，这是预期行为。
3. `P5.4` 不可再用于 `MCLKO/SS_3/PWM6_2`。
4. `P5.0/P5.1` 不可再用于比较器输入。
5. 无线运行时会切换到 SPI 第 4 组，因此不要并行启用旧 `APP_SPI_PS` 示例。
6. 当前未使用 `PKT` 外部中断脚，全部依赖寄存器轮询。
7. `seed` 会直接影响工作信道和同步字，不应当作无关占位值。
8. 业务协议层应通过 `wireless.h` 管理层访问无线链路，不直接 include `lt8920.h` 或读取 LT8920 寄存器。
9. 若后续拿到旧协议源码，建议在本目录上层新增协议层文件，不要把业务逻辑塞进 `lt8920.c`。

## 相关文件

- `Code_boweny/Device/WIRELESS/wireless.h`
- `Code_boweny/Device/WIRELESS/wireless.c`
- `Code_boweny/Device/WIRELESS/lt8920.h`
- `Code_boweny/Device/WIRELESS/lt8920.c`
- `Code_boweny/Device/WIRELESS/wireless_port.h`
- `Code_boweny/Device/WIRELESS/wireless_port.c`
- `Code_boweny/Device/WIRELESS/ship_protocol.h`
- `Code_boweny/Device/WIRELESS/ship_protocol.c`
- `doc/build_doc/README_wireless.md`

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-06 | v1.1.5 | 将模块说明统一收敛为中文表述，接线说明、流程说明、术语和注意事项全部改写为中文叙述，保持代码标识符与串口原始日志不变。 |
| 2026-05-06 | v1.1.4 | 明确当前遥控器接收测试验收项：配对成功必须打印进入工作通道，收到 `0x11` 必须打印油门/转向/按键；新增 `SHIP_THROTTLE_PWM_ENABLE` 宏门控，默认关闭真实 PWM 输出，只做日志调试。 |
| 2026-05-06 | v1.1.3 | 按 `Wireless_other/README.md` 复核并修正移植偏差：seed key 先 8 位截断再右移，默认 seed 得到 `key=32/30`；第 10 次配对包后只写 `reg36/reg39` 并停在配对信道空闲态，等待 30 个调度节拍后再打开工作接收；`LT8920_SetSyncRegs()` 不再清 FIFO。 |
| 2026-05-06 | v1.1.2 | 对齐旧版发送时序：协议发送使用 `Wireless_SendOnChannel()`，配对包发送前后不自动打开接收；发送前端保持 `RXEN=1`，只脉冲 `TXEN`；工作 key 配置先在空闲态写寄存器后再显式打开接收。 |
| 2026-05-06 | v1.1.1 | 对齐 `Wireless_other` 接收时序：工作同步只写 reg36/reg39 并保留 reg37/reg38 默认值；工作接收空闲时按旧版 `Rx_TimeOUT > 10` 周期性重开接收；`rxdbg` 增加同步寄存器打印。 |
| 2026-05-06 | v1.1 | 切回无线最小业务流程：固定 seed 配对、兼容 `PAIR_RSP(0x0F)`、按旧版流式收包解析并对任意合法帧固定回 `0x12`；工作态打印每帧 `0x11` 遥控值，并在 `0x12` power 字节回传 `P0.0 / ADC_CH8` 采样值。 |
| 2026-04-26 | v1.0 | 新建 WIRELESS 模块，完成 LT8920 + KCT8206L 板级抽象、SPI4 接入、半双工收发框架和双天线启动扫描 |
