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
STC32G                  Wireless Module
------                  ----------------
P3.2  (SPI4 SCLK)   ->  SPI_CLK
P3.3  (SPI4 MISO)   <-  MISO
P3.4  (SPI4 MOSI)   ->  MOSI
P3.5  (GPIO CS)     ->  SPI_SS
P5.0  (GPIO)        ->  RST
P5.1  (GPIO)        ->  ANT_SEL
P1.3  (GPIO)        ->  RXEN
P5.4  (GPIO)        ->  TXEN
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
| `lt8920.h/.c` | LT8920 芯片层，负责寄存器、FIFO、收发模式和状态读取 |
| `wireless_port.h/.c` | STC32G 板级层，负责 GPIO、SPI、delay、天线和前端控制 |
| `ship_protocol.h/.c` | 船端业务协议和配对调度 |

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

## 最小自测试

`Wireless_RunMinimalTest()` 用于 bring-up 阶段确认 SPI 链路和 LT8920 响应正常：

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
- `seed[4]` 会派生工作接收信道、工作发送信道和同步/密钥字节。
- 当前内部状态机为 `BOOT_WAIT -> PAIR_SEND -> PAIR_WAIT_RSP -> WORK_RX`。
- 当前硬件为单颗 LT8920 半双工，配对阶段每约 `300ms` 发送一次配对包，共发送 10 次后进入约 `5s` 响应窗口。
- 当前按旧遥控器兼容逻辑处理：配对等待窗口内收到合法 `PAIR_RSP(0x0F)` 即认为配对成功；若响应 payload 长度为 4，只打印 payload 供调试，不再强制要求它等于 seed。
- 配对请求发送失败不会消耗 10 次发送计数；只有成功发送满 10 次并成功切入工作 RX 后才进入响应窗口。
- 配对成功后保持工作 RX 监听；每收到一帧 `THROTTLE(0x11)` 打印 `rc lr=<0-255> ud=<0-255> key=0xXX paired=1`，当前不调用电机控制。

关键日志示例：

```text
[SHIP] I: pair req start retry=0 pair_ch=0x7F seed=6565A065 work_rx=... key=.../...
[SHIP] I: pair success paired=1 work_rx=... work_tx=... key=.../... rsp_len=...
[SHIP] I: enter work-state rx_ch=... tx_ch=... tx_div>80
[SHIP] I: rc lr=100 ud=142 key=0xA0 paired=1
```

## 当前资源占用

| 资源 | 用途 | 备注 |
|------|------|------|
| SPI4 | LT8920 数据收发 | 使用 `P3.2/P3.3/P3.4` |
| P3.5 | LT8920 CS | GPIO 手动片选 |
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
| 2026-05-06 | v1.1 | 切回无线最小业务流程：固定 seed 配对、兼容 `PAIR_RSP(0x0F)`、工作态打印每帧 `0x11` 遥控值，删除正常路径刷屏日志，并补齐中文 Doxygen 头文件注释 |
| 2026-04-26 | v1.0 | 新建 WIRELESS 模块，完成 LT8920 + KCT8206L 板级抽象、SPI4 接入、半双工收发框架和双天线启动扫描 |
