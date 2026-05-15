# WIRELESS 模块说明

## 概述

`Code_boweny/Device/WIRELESS/` 模块用于在当前 `Black Pearl v1.1` 工程中驱动单颗 `LT8920` 2.4G 收发芯片，并控制外部 `KCT8206L` 射频前端完成半双工无线收发。

当前实现遵循工程约束：

- 不修改 `Driver/` 层
- 不使用动态内存
- 所有对外接口返回错误码
- 硬件相关逻辑集中在 `wireless_port.*`
- 关键步骤输出 `LOGI/LOGE`

当前版本交付的是**底层驱动与原始收发框架**，不包含旧工程缺失的 `wirelessProtocal.c` 业务协议兼容层。

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

- `CS` 当前使用 GPIO 手动控制，不依赖硬件自动片选
- `RXEN/TXEN` 控制 `KCT8206L` 收发路径
- `ANT_SEL` 在 `ANT1/ANT2` 之间切换
- 当前策略为“启动扫描后固定天线”，运行中不自动来回跳变

## 模块分层

| 文件 | 职责 |
|------|------|
| `wireless.h/.c` | 对外 API、状态机、静态收发队列、轮询入口 |
| `lt8920.h/.c` | LT8920 芯片层：寄存器、FIFO、收发模式、状态读取 |
| `wireless_port.h/.c` | STC32G 板级层：GPIO、SPI、delay、天线/前端控制 |

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
s8 Wireless_RunMinimalTest(void);
```

### `Wireless_Init()`

完成以下工作：

- 初始化 `SPI4 + GPIO`
- 复位 LT8920
- 装载默认寄存器 profile
- 做寄存器回读自检
- 扫描 `ANT1/ANT2`
- 固定较优天线
- 进入常驻接收模式

### `Wireless_Poll()`

主循环高频调用函数：

- 轮询 LT8920 状态寄存器
- 发现有效包时读 FIFO 并放入静态接收队列
- 发现 CRC 错包时清计数并恢复 RX
- 发包完成后自动切回 RX

### `Wireless_Send()`

- 参数检查
- 从 RX 切到 TX
- 写 FIFO 并触发发送
- 等待发送完成或超时
- 无论成功失败都切回 RX

### `Wireless_Receive()`

- 只从模块内部静态队列取包
- 不直接触碰 LT8920 硬件

### `Wireless_RunMinimalTest()`

- 执行一次性最小联通性测试
- 读取 `Reg3 / Reg6 / Reg11 / Reg41`
- 校验 `Reg11=0x0008`、`Reg41=0xB000`
- 用于确认 SPI 链路与 LT8920 应答正常

说明：

- `LT8920` 当前没有类似 `WHO_AM_I` 的专用芯片 ID 寄存器
- 因此最小测试单元不是“读 ID”，而是“读固定寄存器签名并验证”

## 当前系统接入点

无线模块已接入以下运行链路：

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
       Task_Pro_Handler_Callback()
       IMU_HighRatePoll()
```

说明：

- `Wireless_Poll()` 放在主循环前部，保证 RX 轮询频率
- `Wireless_MinimalTestUnit()` 在 `SYS_Init()` 后执行一次，用于 bring-up 阶段确认无线 SPI 链路可读
- 无线模块不接入现有 `Task.c` 1ms 任务框架
- 当前 SPI 初始化不复用 `User/System_init.c` 中的 `SPI_config()`

## 当前实现策略

### 1. 单 LT8920 半双工

- 默认常驻 RX
- 发送时短暂切到 TX
- 发完立即回 RX

### 2. 双天线启动扫描后固定

- 初始化时先扫描 `ANT1`
- 再扫描 `ANT2`
- 依据 RSSI、有效包数、CRC 错误数择优
- 若都无有效信号，则回退到 `ANT1`

### 3. 静态接收队列

- 队列深度：`WIRELESS_RX_QUEUE_DEPTH = 4`
- 单包最大长度：`LT8920_MAX_PAYLOAD_LEN = 60`
- 队列满时增加溢出计数，不覆盖旧包

## 当前资源占用

| 资源 | 用途 | 备注 |
|------|------|------|
| SPI4 | LT8920 数据收发 | 运行时切到 `SPI_P35_P34_P33_P32` |
| P3.5 | LT8920 CS | GPIO 手动片选 |
| P5.0 | LT8920 RST | 推挽输出 |
| P5.1 | ANT_SEL | 推挽输出 |
| P1.3 | RXEN | 推挽输出 |
| P5.4 | TXEN | 推挽输出 |

## 注意事项

1. 当前实现不保证兼容旧遥控端协议，只保证底层驱动和原始包收发可用
2. `P1.3` 会覆盖系统默认的 `P1.0~P1.3` 高阻配置，这是预期行为
3. `P5.4` 不可再用于 `MCLKO/SS_3/PWM6_2`
4. `P5.0/P5.1` 不可再用于比较器输入
5. 当前无线运行时会把 SPI 路由切到第 4 组，因此不能再并行启用旧 `APP_SPI_PS` 示例
6. 当前未使用 `PKT` 外部中断脚，全部依赖寄存器轮询
7. 若后续拿到旧协议源码，建议在本目录上层再新增协议层文件，而不要把业务逻辑塞进 `lt8920.c`

## 相关文件

- `Code_boweny/Device/WIRELESS/wireless.h`
- `Code_boweny/Device/WIRELESS/wireless.c`
- `Code_boweny/Device/WIRELESS/lt8920.h`
- `Code_boweny/Device/WIRELESS/lt8920.c`
- `Code_boweny/Device/WIRELESS/wireless_port.h`
- `Code_boweny/Device/WIRELESS/wireless_port.c`
- `doc/build_doc/README_wireless.md`
- `doc/project_doc/total.md`
- `doc/project_doc/date.md`

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-26 | v1.0 | 新建 WIRELESS 模块，完成 LT8920 + KCT8206L 板级抽象、SPI4 接入、半双工收发框架与双天线启动扫描逻辑 |
