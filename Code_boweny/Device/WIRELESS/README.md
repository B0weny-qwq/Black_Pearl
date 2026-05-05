# WIRELESS 模块说明

## 概述

`Code_boweny/Device/WIRELESS/` 用于驱动单颗 `LT8920` 2.4G 收发芯片，并控制外部 `KCT8206L` 射频前端完成半双工无线收发。

当前实现遵循以下工程约束：

- 不修改 `Driver/` 层。
- 不使用动态内存。
- 对外接口统一返回错误码。
- 硬件相关逻辑集中在 `wireless_port.*`。
- 关键步骤通过 `LOGI/LOGE` 输出。

当前版本交付的是底层驱动和原始收发框架，并包含船端配对调度逻辑；它不是旧工程 `wirelessProtocal.c` 的完整兼容层。

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

```text
SYS_Init()
  -> APP_config()
  -> log_init()
  -> GPS_Init()
  -> Wireless_Init()

main()
  -> Wireless_MinimalTestUnit()
  -> while(1)
       GPS_Poll()
       Wireless_Poll()
       ShipProtocol_Poll()
       Wireless_SearchSignalPoll()
       Task_Pro_Handler_Callback()
       IMU_HighRatePoll()
```

## 配对流程说明

- 配对请求命令为 `cmd=0x10`。
- 配对响应命令为 `cmd=0x0F`。
- 配对请求帧固定为 9 字节：

```text
AA | 06 | 10 | seed0 | seed1 | seed2 | seed3 | xor | BB
```

- 默认配对发射信道为 `0x7F`。
- `seed[4]` 会派生工作接收信道、工作发送信道和同步/密钥字节。
- 当前硬件为单颗 LT8920 半双工，配对阶段采用“发一个包、切回接收、再发下一个包”的节奏。
- 只有在配对等待窗口内收到 payload 长度为 4 且 seed 完全一致的 `PAIR_RSP`，才认为配对成功。

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

1. 当前实现不保证兼容旧遥控端完整协议，只保证底层驱动和原始包收发可用。
2. `P1.3` 会覆盖系统默认的 `P1.0~P1.3` 高阻配置，这是预期行为。
3. `P5.4` 不可再用于 `MCLKO/SS_3/PWM6_2`。
4. `P5.0/P5.1` 不可再用于比较器输入。
5. 无线运行时会切换到 SPI 第 4 组，因此不要并行启用旧 `APP_SPI_PS` 示例。
6. 当前未使用 `PKT` 外部中断脚，全部依赖寄存器轮询。
7. `seed` 会直接影响工作信道和同步字，不应当作无关占位值。
8. 若后续拿到旧协议源码，建议在本目录上层新增协议层文件，不要把业务逻辑塞进 `lt8920.c`。

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
| 2026-04-26 | v1.0 | 新建 WIRELESS 模块，完成 LT8920 + KCT8206L 板级抽象、SPI4 接入、半双工收发框架和双天线启动扫描 |
