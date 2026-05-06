/**
 * @file    date.md
 * @brief   Black Pearl v1.1 开发日志
 *
 * @author  boweny
 * @date    2026-05-06
 * @version v1.7.28
 *
 * @details
 * 本文件是 Black Pearl v1.1 项目的变更记录和 Bug 追踪文档。
 * 每一次代码修改、Bug 发现与修复、功能增删都必须记录在此。
 * 这是项目持续维护的核心依据，请务必在每次提交前更新。
 *
 * @note    日志格式：新增功能 / Bug修复 / 优化改进 / 变更记录 / 开发者备注
 *
 * @see     total.md
 */

# Black Pearl v1.1 开发日志

> 本文件是项目的**变更记录和 Bug 追踪文档**。
> 每一次代码修改、Bug 发现与修复、功能增删都必须记录在此。
> 这是项目持续维护的核心依据，请务必在每次提交前更新。

---

## 日志格式规范

```
## [YYYY-MM-DD] - vX.X.X.X

### 新增功能
- [模块名] 功能描述

### Bug 修复
- **[类型缺失]** `Type_def.h` 缺少 `s8` 定义，导致 `QMC6309.h` 中所有 `s8` 返回值的函数声明被 Keil C51 解析崩掉，进而引发连锁报错 (error C42/not in formal parameter list)。修复：补 `typedef signed char s8;`
- **[冗余变量]** `QMC6309_I2C_Addr` 全局变量初始化赋的值实际未被使用，删除；所有 I2C 操作直接用 `QMC6309_I2C_ADDR_PRIMARY` 宏替代
- [模块名] Bug描述 → 修复方案

### 优化改进
- [模块名] 改进描述

### 变更记录
- [模块名] 原有行为 → 新行为

### 开发者备注
- 任何需要特别注意的事项
```

---

## 变更日志

---

## [2026-05-06] - v1.7.28 文档中文化收敛

### 优化改进
- **[总览文档]** 更新 `doc/project_doc/total.md`，将正文中的“载荷、空闲态、调度节拍、射频、底层联调”等说明统一改写为中文表述，保留代码标识符、宏名和串口原始日志不变。
- **[开发日志]** 更新 `doc/project_doc/date.md`，补记本次文档中文化收敛记录，确保版本追溯时可以区分“代码行为变更”和“文档措辞整理”。
- **[无线说明]** 更新 `Code_boweny/Device/WIRELESS/README.md`，把接线说明、流程说明和注意事项中的英文叙述统一替换为中文术语。

### 开发者备注
- 本次仅整理文档表述，不修改无线业务逻辑、不修改寄存器时序，也不改变当前测试日志内容。

---

## [2026-05-06] - v1.7.27 无线遥控器接收测试日志与PWM门控

### 新增功能
- **[PWM安全门控]** 新增 `SHIP_THROTTLE_PWM_ENABLE` 宏，默认 `0`。默认测试模式下 `cmd=0x11` 只打印 `lr/ud/key`，不调用 `Motor_SetBothSpeed()`，避免遥控器联调时误输出真实 PWM。
- **[测试确认日志]** 收到 `cmd=0x11` 时，在原有 `rc lr=... ud=... key=... paired=...` 和 `throttle=... steering=... key=...` 后追加 `pwm disabled by SHIP_THROTTLE_PWM_ENABLE=0`，用于现场确认当前没有驱动电机。

### 优化改进
- **[中文 Doxygen]** 检查 `Code_boweny/Device/WIRELESS/` 下 `.c/.h` 文件，补齐中文 Doxygen 文件头；`ship_protocol.c`、`lt8920.c`、`wireless.c`、`wireless_port.c` 已明确各自职责和移植边界。
- **[测试验收文档]** 更新 `Code_boweny/Device/WIRELESS/README.md` 和 `total.md`，明确当前必须打印两类信息：配对成功后进入工作通道；收到遥控器油门/转向/按键数据。

### 开发者备注
- 真实 PWM 输出只能在手动把 `SHIP_THROTTLE_PWM_ENABLE` 改为 `1` 后启用；默认固件仍是无线配对和遥控器收包调试，不是运动控制固件。

---

## [2026-05-06] - v1.7.26 wireless-other配对寄存器与时序再对齐

### Bug 修复
- **[seed key 派生错误]** 当前 `ShipProtocol_ApplyDefaultRf()` 把 `(seed[0] << 4) >> 4` 的结果最后才转 `u8`，导致 seed `65 65 A0 65` 派生出 `key=128/126`。老版是先把左移结果截断为 8 位再右移，现已修正为 `key=32/30`，对应 `reg36=0x2020`、`reg39=0x1E1E`。
- **[配对后 RX 时序偏差]** 当前第 10 次 `PAIR_REQ` 后会立即打开工作 RX；老版只执行 `RF_Encrypt_Config(SEND/REC)` 并保留 `lt8920_waitTimes=30`，后续才由 `RF_Receive(work_ch)` 打开 RX。现已改为第 10 包后只写 `reg36/reg39` 并停在配对信道 idle，等待 30 个调度 tick 后再打开工作 RX。
- **[同步寄存器写入多余清 FIFO]** 当前 `LT8920_SetSyncRegs()` 末尾清 RX FIFO；老版 `RF_Encrypt_Config()` 不写 `reg52`。现已改为只写 `reg7 idle -> reg36 -> reg39`，不清 FIFO、不自动打开 RX。

### 优化改进
- **[RX入口寄存器顺序]** 新增 `LT8920_OpenRxOnChannel()`，工作 RX 打开时直接执行 `reg7 idle(work_ch) -> reg52 clear -> reg8=0x6C90 -> reg7 RX`，减少切信道过程中的额外 RX 动作。
- **[诊断日志]** 首次配对发送打印完整 9 字节请求帧；第 10 包后打印 `pair-sync-idle` 寄存器快照，现场应看到 `reg7=0x007F`、`reg36=0x2020`、`reg39=0x1E1E`。

### 开发者备注
- 这是移植修正，不是协议重构；遥控器程序不可变，后续所有改动继续以 `Wireless_other/README.md` 的包格式、寄存器写入顺序和调度节拍为准。

---

## [2026-05-06] - v1.7.25 配对TX时序按LT8920_TxData对齐

### Bug 修复
- **[配对TX时序偏差]** `ShipProtocol_SendFrame()` 原先通过 `Wireless_SetChannel()` 设信道，该接口会先打开 RX；`Wireless_Send()` 发完又自动打开 RX。老版 `Wireless_Send()->LT8920_TxData()` 是 `reg7 idle(channel) -> reg52 clear -> FIFO -> reg7 TX -> reg7 idle(channel)`，配对包发送前后都不会自动开 RX。已新增 `Wireless_SendOnChannel()` 并切换协议发送路径，保证配对 `0x10` 真正按旧版 TX 时序发出。
- **[前端使能偏差]** 老版初始化后 `RX_EN_H` 常开，TX 时只拉高 `TX_EN_H`，发送结束只拉低 `TX_EN_L`。当前 TX 前端已改为保持 `RXEN=1` 并拉高 `TXEN=1`，不再 TX 时强制关闭 RXEN。
- **[加密配置时序偏差]** 老版 `RF_Encrypt_Config()` 只写 `reg36/reg39` 并停在 idle，真正打开工作 RX 是后续 `RF_Receive(work_ch)`。当前新增 `Wireless_SetSyncRegsIdle()`，`ShipProtocol_ApplyWorkRx()` 先 idle 写 key，再显式切工作信道 RX。

### 优化改进
- **[诊断日志]** 临时保留 `tx ok len=...`、`rx event ...`、`rx pkt len=...` 和 `work-rx reopen cnt=...`，用于下一轮确认配对包是否发出、是否看到遥控器回包边沿。

### 开发者备注
- 本次 review 结论：上一版工作 RX 寄存器值已经正确，但配对 TX 路径不严格等同老版；本轮修正的是“配对包发出去”的 TX 时序。
- Keil Build 已通过：`0 Error(s), 0 Warning(s)`。

---

## [2026-05-06] - v1.7.24 RX时序与同步寄存器按Wireless_other对齐

### Bug 修复
- **[同步寄存器偏差]** 当前 `LT8920_SetSyncRegs()` 原先会把 `reg37/reg38` 清为 `0x0000`，但老版 `RF_Encrypt_Config()` 只写 `reg36=key0/key0` 和 `reg39=key1/key1`。已改为只写 `reg36/reg39`，保留 `reg37=0x0380`、`reg38=0x5A5A`，避免工作信道 RX 同步条件与不可修改的遥控器不一致。
- **[RX空闲时序]** 工作态无有效包时增加 10 tick 周期性重开 RX，对齐老版 `RF_Receive()` 中 `Rx_TimeOUT > 10` 后重新 `LT8920_OpenRx()` 的行为。
- **[空闲计时下溢]** 进入工作 RX 时初始化 `last_proto_rx_ms`，避免启动初期打印 `work-rx idle 4294967269ms` 这类 tick 下溢值。

### 优化改进
- **[RX诊断]** `rxdbg` 增加 `reg36/reg37/reg38/reg39` 打印，现场可直接确认同步寄存器是否为 `key0/key0, 0380, 5A5A, key1/key1`。
- **[文档同步]** 更新 `README.md`、`Code_boweny/Device/WIRELESS/README.md`、`doc/project_doc/total.md`，明确当前 RX 时序是移植对齐，不是协议重构。

### 开发者备注
- Keil Build 已通过：`0 Error(s), 1 Warning(s)`；唯一告警为既有 `System_init.c(168): warning C174: 'Sensor_I2C_prepare': unreferenced 'static' function`。

---

## [2026-05-06] - v1.7.23 头文件Doxygen与移植边界文档同步

### 优化改进
- **[Doxygen注释]** `ship_protocol.h` 补充 `Wireless_other/wirelessProtocal.c` 移植背景、旧版帧格式、流式截帧、10 次配对后进入工作 RX、任意合法帧固定回 `0x12` 等约束。
- **[接口边界]** `wireless.h` 补充 `Wireless_Receive()` 返回 LT8920 RF payload，不等同于旧业务协议帧；旧协议截帧由 `ShipProtocol_RunScheduler()` 内部完成。
- **[文档同步]** 更新 `README.md`、`Code_boweny/Device/WIRELESS/README.md`、`doc/project_doc/total.md`，明确当前是移植兼容而不是协议重构。

### 开发者备注
- 后续修改无线业务时必须先对照 `Wireless_other`，不要把 RF payload 直接当完整 `AA..BB` 协议帧，也不要向 `0x12` payload 新增字段。
- Keil Build 已通过：`0 Error(s), 1 Warning(s)`；唯一告警为既有 `System_init.c` 中 `Sensor_I2C_prepare` 未引用静态函数。

---

## [2026-05-06] - v1.7.22 wireless-other收包与回包业务对齐

### Bug 修复
- **[收包解析]** `ship_protocol.c` 原先要求 `Wireless_Receive()` 返回的数据必须从 `0xAA` 开始且长度刚好等于完整协议帧；现改为对齐旧 `WirelessProtocal_Receive_Handle()`，在单个 RF payload 内逐字节寻找 `0xAA`、按长度字段收帧、校验 `xor` 和 `0xBB` 后分发。
- **[回包节奏]** 原先只对已识别命令回发 `0x12`；现对齐旧 `WirelessProtocal_Resolve_Handle()`，任意通过校验的协议帧分发结束后都回发一次 `0x12`。

### 优化改进
- **[旧业务对齐]** `cmd=0x12` 入站帧按旧版不做业务处理，仅保持合法帧后的 `0x12` 回包，不再额外解析或打印对端 `0x12` 内容。
- **[兼容边界]** RF payload 长度超过旧版 `WIRELESS_PROTOCAL_MAX_LEN(30)` 时按旧逻辑截为 10 字节处理，避免新解析器行为比旧版更宽。

### 开发者备注
- `0x13/0x14/0x15` 老版依赖 `autoDrive_Set_ReturnPosition()`、`autoDrive_Set_FishPosition()`、`autoDrive_Set_Switch()`；当前工程没有这些模块实现，本轮未伪造业务动作，只保留接收日志和固定 `0x12` 回包。
- Keil Build 已通过：`0 Error(s), 0 Warning(s)`。

---

## [2026-05-06] - v1.7.21 ADC打印与0x12格式收敛

### 新增功能
- **[ADC打印]** `ship_protocol.c` 在每次发送 `0x12` 状态包前打印 `P0.0 / ADC_CH8` 的 `raw/adc_mv/bat_mv/power`，便于现场直接观察采样值和实际进入无线帧的 1 字节 power 值。
- **[电压估算]** `User/Config.h` 新增 `SHIP_ADC_REF_MV`、`SHIP_BAT_DIV_NUM`、`SHIP_BAT_DIV_DEN`，用于按实际分压比例把 ADC 输入端电压估算回电池端电压；默认 `1/1`，不假设硬件分压。

### 优化改进
- **[数据格式]** `0x12` payload 固定检查为 15 字节，power 仍占用老版原位置的 1 字节，不向遥控器新增字段。
- **[配对流程]** 第 10 次 `PAIR_REQ(0x10)` 发送完成后按老版业务直接进入工作 RX，同时保留 `PAIR_RSP(0x0F)` 有效窗口用于打印配对成功，不再超时重启配对。
- **[初始化]** 修正 `SYS_Init()` 中 `ADC_config()` 缩进，保留 P0.0 高阻输入和关闭数字输入配置。

### 开发者备注
- `Wireless_other` 只包含 `Power_ADC_Get_Level()` 调用点，不包含该函数实现和电量阈值；当前串口已打印 raw 和 power，后续若补齐老版阈值，可只替换 power 字节生成函数，不改无线帧格式。

---

## [2026-05-06] - v1.7.20 P0.0 ADC采样无线回传与文档同步

### 新增功能
- **[ADC回传]** 恢复 `ADC_config()` 初始化链路，当前 `P0.0` 已作为模拟输入启用，并通过无线 `0x12` 状态包的 power 字段回传。

### 优化改进
- **[引脚配置]** `GPIO_config()` 中将 `P0.0` 设为高阻输入并关闭数字输入，减少数字输入路径对 ADC 采样的干扰。
- **[协议复用]** `ship_protocol.c` 新增本地 ADC 读取封装，直接读取 `ADC_CH8`，将 12 位采样值右移 4 位压缩为 1 字节后装入旧版 `0x12` 包结构，不改协议长度。
- **[注释同步]** `ship_protocol.c` 中新增 P0.0 ADC 回传函数的中文 Doxygen 注释，说明通道映射、压缩方式和异常返回值。

### 变更记录
- **[无线状态包]** `0x12` 包中的 power 字段由固定 `0` 改为 `P0.0 / ADC_CH8` 实时采样值。
- **[文档同步]** 更新 `README.md`、`Code_boweny/Device/WIRELESS/README.md`、`doc/project_doc/total.md`，补充 P0.0 ADC 占用、回传路径和串口观察点。

### 开发者备注
- 当前 power 字段仍为 1 字节，传输的是压缩后的 ADC 原始量；电压值只走串口日志，不进入无线 payload。
- 若后续启用 `UART3_SW_P00_P01` 的实际业务，需要先处理它与 `P0.0` ADC 采样的引脚复用冲突。

---

## [2026-05-06] - v1.7.19 WIRELESS最小业务配对接收

### 变更记录
- **[运行模式切换]** `WIRELESS_MINIMAL_TEST_ONLY=1` 现在运行无线最小业务链路：`Wireless_Poll()` + `ShipProtocol_RunScheduler()`，不再进入持续 `Wireless_RunPairTxOnlyTest()` 单向发包诊断。
- **[诊断宏关闭]** `WIRELESS_TX_ONLY_TEST`、`WIRELESS_PAIR_TX_ONLY_TEST`、`WIRELESS_CONTINUOUS_TX_TEST` 默认改为 `0`；固定配对 seed 保持 `65 65 A0 65`。
- **[配对兼容]** 船端按旧参考逻辑发送 10 次 `PAIR_REQ(0x10)` 后打开响应窗口；窗口内收到合法 `PAIR_RSP(0x0F)` 即认为配对成功，不再强制要求响应 payload 等于 seed。
- **[状态机收敛]** `ship_protocol.c` 内部切为 `BOOT_WAIT -> PAIR_SEND -> PAIR_WAIT_RSP -> WORK_RX` 状态流；解析 `PAIR_RSP` 时只更新协议状态，RF 工作 RX 配置由调度器统一维护。
- **[遥控值输出]** 收到 `THROTTLE(0x11)` 时每帧打印 `rc lr=... ud=... key=0x.. paired=1`，本轮只打印遥控器值，不调用电机控制。

### 优化改进
- **[日志降噪]** 删除正常路径中的 `mode->rx`、`mode->tx`、`tx done R3/R7/R48/R52`、`rx pkt len`、`rx frame len`、周期性 `status paired` 等刷屏日志；保留初始化、配对成功、进入工作态和错误日志。
- **[接收窗口保护]** 配对响应窗口和无线最小工作态只在进入时配置 RX 信道/同步字；配对发送失败不会消耗 10 次配对包计数，避免没发够就误入响应窗口。
- **[边界收敛]** 最后一包配对请求后，只有成功切入工作 RX 才开启 `PAIR_WAIT_RSP`；若 RF 配置失败，会回到下一轮配对发送，避免打开无效响应窗口。
- **[协议层边界]** `PAIR_RSP` 解析函数不再直接重配 RF，只负责置 `paired/state`；工作 RX 由调度器统一应用，降低队列解析过程中清 FIFO 的风险。
- **[API边界审核]** `ship_protocol.c` 只依赖 `wireless.h` 管理层 API，不直接 include `lt8920.h` 或读取芯片寄存器；`wireless.h`、`lt8920.h` 对外函数签名保持不变。
- **[头文件注释]** `Code_boweny/Device/WIRELESS/` 下 `wireless.h`、`lt8920.h`、`wireless_port.h`、`ship_protocol.h` 已统一为中文 Doxygen 风格注释。

### 开发者备注
- 本次未修改 `Driver/`、`wireless.h` 或 `lt8920.h` 对外 API。
- 当前目标是验证遥控器配对与接收值打印；GPS/IMU/MAG 和电机控制仍保持跳过或不调用。
- Keil Rebuild 已通过：`0 Error(s), 10 Warning(s)`；当前 10 个 warning 来自既有 System_init/QMI8658/GPS/wireless_port 非本次无线业务主路径。

---

## [2026-05-05] - v1.7.18 WIRELESS根目录时序差异补齐

### 优化改进
- **[LT8920寄存器表补齐]** `g_lt8920_default_regs` 补入根目录 `LDT89xxconfig` 中的 `Reg7=0x0030` 和 `Reg50=0x0000`；`Reg50` 属于 FIFO 口，只写入不纳入读回校验。
- **[软件SPI快速分支对齐]** 默认 `WIRELESS_SOFT_SPI_DELAY_US` 改为 `0`，并在软件 SPI 位传输中编译掉额外延时调用，使默认 GPIO SPI 更贴近根目录 `SlowSPI_io=0` 的快速流程。
- **[FIFO额外延时关闭]** 默认 `LT8920_FIFO_DELAY_TEST` 改为 `0`，FIFO 写入不再插入额外 `1us` 间隔。
- **[TX轮询间隔对齐]** `Wireless_Send()` 发射后仍先延时 `100us`，随后轮询 `Reg48 PKT` 的间隔改为 `1000us`，与根目录 `LT8920_TxData()` 当前寄存器 PKT 轮询路径一致。

### 变更记录
- **[引脚保持不变]** 本次只补齐寄存器与时序差异，不改变 `SCLK=P3.2`、`MISO=P3.3`、`MOSI=P3.4`、`CS=P3.5`、`RST=P5.0`、`ANT_SEL=P5.1`、`TXEN=P5.4`、`RXEN=P1.3`。

### 开发者备注
- 根目录 `Wireless` 的板级引脚与 `Code_boweny` 当前硬件不同；本次按 `Code_boweny` 固定引脚不变，只对齐 LT8920 寄存器配置和可工作收发时序。

---

## [2026-05-05] - v1.7.17 WIRELESS初始化层按根目录Wireless对齐

### 优化改进
- **[LT8920软件SPI默认]** `wireless_port.c` 默认改为软件 SPI，硬件 SPI4 仍可通过 `WIRELESS_SOFT_SPI_TEST=0` 切换；两种后端均固定使用 `P3.2/P3.3/P3.4/P3.5`，未新增测试片选脚。
- **[软件SPI时序]** 软件 SPI 的 SCLK/MOSI/MISO 顺序按根目录 `Wireless/LT8920/LT8920_SPI.c` 当前有效流程收敛，CS 只由 `P3.5` 控制。
- **[LT8920收发入口]** 新增芯片层 `LT8920_OpenRx()` 与 `LT8920_StartTxPacket()`，把 `Reg7 idle -> Reg52 clear -> Reg8 -> RX/TX` 等寄存器顺序集中到 `lt8920.c`。
- **[FIFO语义对齐]** FIFO 写入保持 `Reg50 + len + payload`；FIFO 读取保持 `Reg50|0x80 -> len -> payload`，长度最大按 64 字节钳制后清 RX path。

### 变更记录
- **[引脚保持不变]** 无线引脚仍为 `SCLK=P3.2`、`MISO=P3.3`、`MOSI=P3.4`、`CS=P3.5`、`RST=P5.0`、`ANT_SEL=P5.1`、`TXEN=P5.4`、`RXEN=P1.3`。
- **[发送流程对齐]** `Wireless_Send()` 发送前先打开 `TXEN` 并关闭 `RXEN`，再装载 FIFO、进入 TX、延时 `100us`、轮询 `Reg48 PKT`；完成后先让 LT8920 回 idle，再关闭 `TXEN`，必要时回 RX。
- **[边界保持]** 本次未修改 `Driver/`、`User/STC32G.H` 和无线 public API，也未恢复正常业务运行模式宏。

### 开发者备注
- 当前默认仍是无线最小 TX 诊断运行配置，只是底层初始化和收发时序已按根目录可工作 `Wireless/` 对齐。
- 若需要复测硬件 SPI4，只改 `User/Config.h` 中 `WIRELESS_SOFT_SPI_TEST` 为 `0`。

---

## [2026-05-05] - v1.7.16 WIRELESS配对阶段收敛与文档同步

### Bug 修复
- **[配对误判]** `SHIP_CMD_PAIR_RSP(0x0F)` 成功判定从“窗口内收到合法帧即成功”收紧为“payload 长度必须为 4 且必须与本轮 `seed[4]` 完全一致”。这样可以避免旧包、误包、串扰包把 `paired` 误置位。
- **[配对重试闭环]** 船端配对调度补齐自动重试：单轮 `pair_left` 发完并等待超时后，只要总超时 `pair_total_timeout_ticks` 尚未耗尽，就会重新装载 `pair_left` 并继续下一轮配对。
- **[配对信道日志]** 配对日志明确打印真实发射配对信道 `pair_ch=0x7F`，不再把派生工作信道误当作配对发射信道。

### 优化改进
- **[单芯片半双工配对]** 当前 `ship_protocol.c` 的配对阶段改为更贴近单颗 `LT8920` 的收发节奏：发送一个 `cmd=0x10` 后立刻切回 RX，先给一个短响应窗口，最后一包结束后再保留长窗口，避免长时间停留在单一方向。
- **[超时诊断]** 配对窗口超时时增加一次性汇总诊断 `diag sync/pkt/crc/fifo`，用于区分“完全没看到空口活动”和“看到了活动但没形成可解析帧”。
- **[日志降噪]** 删除已经确认无价值的发送寄存器刷屏日志，只保留必要的配对状态与异常日志，避免串口被底层重复寄存器值污染。
- **[LT8920寄存器写入试验]** 默认寄存器 profile 从 `{reg, u16 value}` 改为 `{reg, high, low}`，初始化时直接按高字节、低字节分别发送，用于排除 16 位拆分或编译器整数处理对 LT8920 配置写入的影响。
- **[LT8920成功时序对齐]** 按已验证工程的 LT8920 寄存器时序收敛当前单芯片驱动：复位等待改为高 10ms、低 100ms、高 100ms；默认 profile 不提前写 `Reg7`，初始化末尾补 `Reg8/Reg52/Reg7` 和 `Reg0/11/41` 校验；工作同步字 `SetSyncRegs()` 先 idle，再写 `Reg36`、清 `Reg37/38`、写 `Reg39`、清 FIFO；TX/RX 入口均先 idle/清 FIFO/写 `Reg8` 后进入对应模式，TX 轮询保留超时并在失败时强制 idle + 清 FIFO。

### 变更记录
- **[seed语义收敛]** 当前文档明确：`cmd=0x10` 的 4 字节 `seed` 不是单纯的持久化标签，而是当前配对输入；船端会基于它派生工作 RX/TX 信道与同步/密钥字节。若遥控器期待的 `seed` 不同，即使底层发射已确认成功，也可能完全无法配对。
- **[当前实测结论]** 已通过底层日志确认船端真实发出了 9 字节配对包 `AA 06 10 seed0 seed1 seed2 seed3 xor BB`。因此当前剩余主要疑点已收敛到“对端是否接受当前 `seed`”以及“对端是否按当前时序/同步回包”，而不是“船端到底有没有发出去”。
- **[文档同步]** 更新 `Code_boweny/Device/WIRELESS/README.md`、`doc/project_doc/total.md` 和 `ship_protocol.c` 注释，统一当前配对阶段真实行为。

### 开发者备注
- 当前默认固定 `seed` 仍为 `65 65 A0 65`，除非显式启用 `SHIP_PAIR_SEED_USE_CHIPID`，否则不会从芯片 ID 自动取种子。
- 若后续仍无法配对，优先看最新超时日志中的 `diag sync/pkt/crc/fifo`，再判断是完全没看到对端、看到同步但没出包，还是 payload 不匹配。

---

## [2026-05-01] - v1.7.15 QMI8658旧I2C路径复测

### 变更记录
- **[QMI8658复测]** 新增 `QMI8658_LEGACY_I2C_PATH=1` 测试开关，当前版本默认回到 STC 官方 `I2C_ReadNbyte()` / `I2C_WriteNbyte()` 旧路径，用于排除 v1.7.14 分段 ACK 诊断代码对 QMI8658 地址响应的影响。
- **[启动日志]** QMI8658 初始化日志增加 `i2c=legacy/ackdiag`，便于确认当前烧录固件实际使用哪条 I2C 读写路径。

### 开发者备注
- 最新实测现象为 `0x6B/0x6A` 均 `DEVW_NACK`，同时 `mag_id=0x90` 正常。若旧路径仍读不到 `WHO_AM_I=0x05`，优先继续排查 QMI8658 的 CSB、VDDIO、SDA/SCL 支路、焊点和芯片方向。

---

## [2026-05-01] - v1.7.14 QMI8658 I2C ACK分段诊断

### 优化改进
- **[QMI8658诊断]** QMI8658 寄存器读写改为逐段检查 I2C ACK，不再依赖 STC 官方 `I2C_ReadNbyte()` 的 `Get_MSBusy_Status()` 间接判断。
- **[启动定位]** `WHO_AM_I` 地址探测日志新增 `DEVW_NACK / REG_NACK / DEVR_NACK / BUSY` 错误名，用于区分 QMI8658 地址阶段无响应、寄存器地址无响应、读地址无响应和总线忙。

---

## [2026-05-01] - v1.7.13 AHRS人类可读调试输出

### 优化改进
- **[AHRS日志]** 测试输出从 centidegree 原始整数改为短格式定点度数。陀螺零偏未 ready 时输出 `r=-7.76 p=+1.05 y=-58.26 g=-5.10 -0.13 +0.60 f=17`；零偏 ready 后才锁定当前 yaw 为相对零点，并输出 `yr`：`r=-7.76 p=+1.05 y=-58.26 yr=-90.00 f=1F`。
- **[陀螺零偏]** 将静止零偏学习阈值从 `2 deg/s` 放宽到 `8 deg/s`，避免当前 QMI8658 静止 X 轴约 `-5 deg/s` 偏置导致 `flags` 长期卡在 `0x17`。

---

## [2026-05-01] - v1.7.12 QMI8658偶发全0xFF启动重试

### 优化改进
- **[上电时序]** `AHRS_TEST_ONLY=1` 下在传感器初始化前增加 `1500ms` 稳定等待，补回跳过 GPS / Wireless 后减少的启动延时。
- **[IMU自检重试]** `QMI8658_PowerOnSelfTest()` 初始化失败后会重新 `Sensor_I2C_prepare()`、执行 `QMI8658_BusRecover()` 并延时重试，最多 4 次，避免偶发 `WHO_AM_I=0xFF` 直接导致 AHRS 不启动。
- **[失败诊断]** 每次重试输出 `mag_id=...`，用于判断 QMC6309 是否仍能在同一 I2C 总线上正常响应。

---

## [2026-05-01] - v1.7.11 AHRS零偏未就绪时输出陀螺诊断

### 优化改进
- **[AHRS测试]** 当 `flags` 缺少 `AHRS_FLAG_GYRO_BIAS_READY` 时，AHRS 日志临时追加 `gyro_dps100=x y z`。零偏 ready 后自动恢复只输出 `rpy_cd/flags`，用于定位静止 yaw 漂移是否来自陀螺 Z 零偏或量程系数。

---

## [2026-05-01] - v1.7.10 QMI8658 WHO_AM_I选址

### Bug 修复
- **[QMI8658地址误判]** 启动测试中出现 `WHO_AM_I=0xFF`，说明原先基于写地址 ACK 的探测可能选中了无效地址或遇到总线浮空读全 1。修复：初始化阶段改为分别读取 `0x6B/0x6A` 的 `WHO_AM_I`，只有读到 `0x05` 才确认地址。

### 优化改进
- **[失败诊断]** 若两个地址都读不到 `0x05`，输出 `id probe fail p=... ok=... a=... ok=...`，便于区分地址脚变化、读事务 NACK 和总线读全 `0xFF`。

---

## [2026-05-01] - v1.7.9 AHRS测试模式保留错误诊断

### 优化改进
- **[日志过滤]** `AHRS_TEST_ONLY=1` 下改为放行 `AHRS` 正常日志和所有 `ERROR` 日志。成功时仍只刷 `rpy_cd/flags`，失败时能看到 `[IMU] E: ...` 等真实初始化失败原因。
- **[噪声控制]** 删除主循环里的 `[AHRS] E: imu not ready` 提示，避免 IMU 自检失败后被重复的泛化错误刷屏。

---

## [2026-05-01] - v1.7.8 AHRS角度-only串口测试模式

### 优化改进
- **[AHRS测试输出]** 新增 `AHRS_TEST_ONLY=1` 测试开关：启动后跳过 GPS / Wireless 初始化和主循环轮询，关闭无线扫描、协议轮询和 `MAG_StandalonePoll()` 独立地磁日志。
- **[日志过滤]** `Log.c` 在 AHRS 测试模式下只放行 `AHRS` tag，屏蔽 SYS 横幅、MAG/IMU/WL/GPS 等初始化和运行日志，串口只保留融合链路输出。
- **[角度日志]** AHRS 日志简化为 `rpy_cd=roll pitch yaw flags=0x..`，去掉 `gyro_dps100`，便于直接观察融合姿态角。

### 开发者备注
- 本模式下 AHRS 内部仍会按 `AHRS_MAG_PERIOD_MS` 读取 `QMC6309_ReadXYZFiltered()` 参与 yaw 慢修正，只是不再单独打印磁力计 raw。
- 若要恢复完整外设联调，将 `User/config.h` 中 `AHRS_TEST_ONLY` 改回 `0`。

---

## [2026-05-01] - v1.7.7 QMI8658换芯确认与AHRS测试模式

### Bug 修复
- **[QMI8658数据域失效]** 实测旧芯片表现为 `WHO_AM_I=0x05`、`REV=0x7C`、`CTRL1/2/3/5/7` 可读写，但 `RESET_STATE=0x00` 且 `STATUS0/temp/acc/gyro` 单字节 dump 全 `00`。更换 QMI8658 后恢复 `RESET_STATE=0x80` 与 `ready ... acc=...`，确认问题在旧芯片数据/采样域而非 I2C、AHRS 或地磁链路。

### 变更记录
- **QMI8658**: 固定为稳定 bring-up 测试路径：`CTRL2=0x07`、`CTRL3=0x07`、`CTRL5=0x11`、`CTRL7=0x03`，默认 `QMI8658_SOFT_RESET_ENABLE=0`，失败时才调用 `QMI8658_DumpRawRegs()`。
- **AHRS测试**: 保持 `AHRS_IMU_PERIOD_MS=17ms`、`AHRS_GYRO_LSB_PER_DPS=2048`，将 AHRS 日志抽样调整为约 0.5 秒一条，便于观察 `rpy_cd/gyro_dps100/flags`。
- **MAG日志**: 独立磁力计测试日志周期从 250ms 调整为 1000ms，减少串口干扰，同时保留磁力计活性观察。

### 开发者备注
- 下一轮上板重点看 `[AHRS] I: rpy_cd=... gyro_dps100=... flags=0x..`。静止放平后 `gyro_dps100` 应接近 0，`roll/pitch` 应稳定；倾斜板子后 `roll/pitch` 应跟随变化。
- 若 `flags` 长期缺少 acc/mag valid，再回查 `AHRS_UpdateRaw6Axis()` 的加速度模长窗口和地磁干扰。

---

## [2026-04-30] - v1.7.6 QMI8658单字节寄存器dump

### 新增功能
- **[IMU诊断]** 新增 `QMI8658_DumpRawRegs()`，在启动自检 `ReadAcc()` 连续失败后，用单字节读取方式输出 `WHO/REV/RESET/CTRL/STATUS/TEMP/ACC/GYRO` 关键寄存器。

### 开发者备注
- 若 dump 中 `acc/gyr/temp` 单字节也全为 `00`，说明不是多字节自动递增读取问题，而是 QMI8658 数据寄存器本身未更新。
- 若单字节 dump 有数据而 `ReadAcc()` 全 0，则回查 `CTRL1` 自动递增配置或 STC `I2C_ReadNbyte()` 多字节读流程。

---

## [2026-04-30] - v1.7.5 QMI8658旧版精确读数路径复测

### 变更记录
- **[IMU复测]** 新增 `QMI8658_LEGACY_EXACT_TEST=1`，恢复旧版关键路径：`CTRL2/CTRL3=0x07/0x07`、按 `RESET_STATE` 条件软复位、`500ms/30ms/200ms` 等待参数。
- **[诊断隔离]** `QMI8658_DIAG_ENABLE=0`，关闭初始化期间的 timestamp/temp/raw 数据窗口诊断，避免诊断读取改变第一帧行为；是否成功只看 `QMI8658_PowerOnSelfTest()` 后续直接读 `0x35~0x3A` 的 `ready ... acc=...`。

### 开发者备注
- 这是与 2026-04-23 实测成功路径最接近的一轮对照。若仍然全 0，说明问题不在 AHRS、地磁轮询、量程改动或诊断读取，而要回到 QMI8658 硬件数据域。

---

## [2026-04-30] - v1.7.4 QMI8658无软复位长等待复测

### 变更记录
- **[IMU复测]** 新增 `QMI8658_SOFT_RESET_ENABLE=0`，本轮完全跳过 QMI8658 软复位，只做旧版 `CTRL2/CTRL3=0x07/0x07` 配置写入和数据窗口观察。
- **[等待拉长]** 将 QMI8658 上电等待拉长到 `1000ms`，使能后等待拉长到 `200ms`，ready 轮询超时拉长到 `2000ms`，用于排除数据域启动慢或 reset 后恢复慢的问题。

### 开发者备注
- 若无软复位后恢复出数，说明 `RESET=0xB0` 或 reset 完成判据会触发当前板子的异常，后续默认禁用软复位。
- 若无软复位仍全 0，且 QMC6309 同总线正常，则优先查 QMI8658 电源/焊接/芯片本体。

---

## [2026-04-30] - v1.7.3 QMI8658旧版bring-up复测

### 变更记录
- **[IMU复测]** 将 QMI8658 默认配置临时回退到旧版实测可出数的 `CTRL2=0x07`、`CTRL3=0x07`，并同步 AHRS 周期为 `17ms`、陀螺仪灵敏度为 `2048 LSB/(deg/s)`。
- **[初始化隔离]** `QMI8658_CLEAR_DATAPATH_ENABLE=0`，本轮跳过新增的 `CTRL6/CTRL8/FIFO/CTRL9` 清理流程，并恢复为 `RESET_STATE != 0x80` 时才软复位，只保留数据通路诊断日志，用于区分“新增初始化流程导致不出数”和“芯片/供电数据域本身不出数”。

### 开发者备注
- 若本轮旧版 bring-up 路径恢复 `ready ... acc=...`，下一步逐项打开 `0x16/0x36` 和 CTRL9/FIFO 清理，定位具体触发点。
- 若本轮仍然 `STATUS0=0x00`、timestamp/temp/raw 全 0，则优先查 QMI8658 电源、焊接、芯片状态或同板硬件变化。

---

## [2026-04-30] - v1.7.2 QMC6309独立读数测试入口

### 新增功能
- **[MAG独立测试]** 在 `User/Main.c` 新增 `MAG_StandalonePoll()`，主循环每 250ms 直接读取一次 `QMC6309_ReadXYZ()`，输出 `test raw=x y z norm1=n`，不再依赖 QMI8658 ready 状态。

### 变更记录
- **Main.c**: 主循环新增 `MAG_StandalonePoll()`，即使 `g_qmi8658_ready=0` 导致 AHRS/IMU 轮询提前返回，也可以持续验证 QMC6309 原始三轴地磁数据是否变化。

### 开发者备注
- 当前日志用于判断磁力计是否真实出数：旋转板子时 `raw` 三轴应明显变化；若持续 all zero/all 0xFF 或 `test read fail`，再查 QMC6309 数据通路/I2C ACK/周边磁场。

---

## [2026-04-30] - v1.7.1 QMI8658数据通路诊断与AHRS参数同步

### Bug 修复
- **[IMU不出数]** 实测启动日志显示 `WHO_AM_I=0x05` 但 `STATUS0=0x00`、`acc=0 0 0`，导致 `g_qmi8658_ready=0`，AHRS 主循环直接返回。修复：QMI8658 初始化阶段固定执行一次软复位，避免旧状态下数字通信正常但数据通路未启动。
- **[量程参数不同步]** 将 QMI8658 默认配置从 bring-up 的 `CTRL2/CTRL3=0x07/0x07` 调整为常规 6 轴配置 `CTRL2=0x16`、`CTRL3=0x36`，并同步 AHRS 陀螺仪灵敏度为 `256 LSB/(deg/s)`、IMU 融合周期为 `9ms`。

### 优化改进
- **[启动诊断]** QMI8658 初始化现在会输出 `CTRL1/CTRL2/CTRL3/CTRL5/CTRL6/CTRL7/CTRL8/CTRL9`、FIFO 寄存器、`STATUSINT/STATUS0`、timestamp、temp 和首帧 raw acc/gyro 数据，便于判断失败点是在寄存器写入、ready 标志、FIFO 模式还是数据寄存器。
- **[延迟出数诊断]** 若 `STATUS0.aDA` 在 200ms 内仍未置位，启动阶段会追加约 1 秒短轮询窗口，连续观察 status、timestamp、temp 和 raw acc/gyro 是否延迟变为非零。

### 变更记录
- **QMI8658.h**: 默认 ACC 改为 ±4G/117Hz，GYRO 改为 ±125/128dps/117Hz。
- **QMI8658.c**: 初始化强制软复位，显式清理 FIFO/CTRL8/CTRL9/CTRL6 数据路径，并在 clear、enable 后与 ready/timeout 后输出数据通路快照；timeout 后追加 `data window` 轮询。
- **AHRS.h**: `AHRS_IMU_PERIOD_MS` 改为 `9U`，`AHRS_GYRO_LSB_PER_DPS` 改为 `256L`。

### 开发者备注
- 下一轮上板重点观察 `after_clear/after_enable/not_ready/ready` 四组 IMU 日志和 `data window result`；若 `CTRL*` 与 FIFO 读回正确但 `STATUS0`、timestamp、temp 和 raw 持续为 0，优先排查 QMI8658 传感器电源/焊接/芯片数据通路，而不是 AHRS 算法。

---

## [2026-04-27] - v1.7.0 AHRS姿态融合模块接入

### 新增功能
- **AHRS模块**: 新增 `Code_boweny/Function/AHRS/`
  - `AHRS.h`: 新增定点互补滤波参数、船体系轴向映射宏、`AHRS_State_t` 状态结构和对外 API
  - `AHRS.c`: 新增 Q8 定点姿态融合实现，支持陀螺仪积分、加速度 roll/pitch 慢修正、地磁 yaw 慢修正
  - `README.md`: 新增调参说明，记录 `+X=船尾`、`+Y=船右/右舷`、`+Z=上` 的机体系定义
- **主循环融合接入**: `User/Main.c` 中 `IMU_HighRatePoll()` 改为按 Timer0 1ms tick 固定节拍读取 QMI8658 6 轴数据，并调用 AHRS 更新姿态
- **地磁航向修正**: 主循环每 `AHRS_MAG_PERIOD_MS=100ms` 读取一次 `QMC6309_ReadXYZFiltered()`，通过 AHRS 低频修正 yaw
- **系统tick接口**: `Task.c/.h` 新增 `Task_GetTickMs()`，为 AHRS 提供真实 `dt_ms`

### Bug 修复
- **[IMU重复积分风险]** 原主循环按 while 速度直接读取传感器，可能在传感器 ODR 未更新时重复读同一帧。修复：使用 `Task_GetTickMs()` 将 AHRS IMU 更新节拍固定为 `AHRS_IMU_PERIOD_MS=17ms`
- **[低速积分量化]** AHRS 内部角度使用 Q8 小数保存，角度回绕时不丢弃小数，避免低速角速度积分被整数截断
- **[轴向散落风险]** 将 IMU/MAG 原始轴到船体系的换轴/取反集中到 `AHRS_IMU_BODY_*` 与 `AHRS_MAG_BODY_*` 宏，避免业务层分散处理导致方向不一致

### 优化改进
- **调试简单化**: 采用互补滤波而非 EKF，调参集中在 `AHRS.h`，优先保证上电可观测、日志可读、现场调试成本低
- **抗抖动处理**: 增加陀螺仪死区、三轴低通、加速度模长有效窗口、1g 参考自学习、静止零偏学习和地磁慢修正
- **中文Doxygen**: 为 `AHRS.h/.c` 补齐中文 Doxygen 风格注释，覆盖文件头、宏、结构体字段、内部关键函数和对外 API

### 变更记录
- **System_init.c**: 引入 `AHRS.h`，在传感器初始化链路中调用 `AHRS_Reset()`
- **Main.c**: `IMU_HighRatePoll()` 从单纯加速度日志改为 IMU/MAG 融合入口，周期输出 `rpy_cd` 与 `gyro_dps100`
- **Task.c/.h**: 新增 1ms tick 计数与临界区读取接口
- **RVMDK/STC32G-LIB.uvproj**: 新增 `AHRS` 分组，并补充 `..\Code_boweny\Function\AHRS` 头文件搜索路径
- **total.md**: 同步更新目录结构、启动顺序、主循环说明、模块表、设备说明、任务系统、AHRS 使用边界、文档索引和版本历史

### 开发者备注
- 当前默认坐标系为 `+X=船尾`、`+Y=船右/右舷`、`+Z=上`
- 若实测芯片 raw +X 指向船头，优先修改 `AHRS_IMU_BODY_X_SIGN=-1`
- 若地磁受电机或船体磁性材料干扰明显，联调阶段可先将 `AHRS_MAG_ENABLE=0`
- 若后续修改 QMI8658 陀螺仪量程，必须同步更新 `AHRS_GYRO_LSB_PER_DPS`

---

## [2026-04-26] - v1.3.0 WIRELESS模块接入

### 新增功能
- **WIRELESS模块**: 新增 `Code_boweny/Device/WIRELESS/`
  - `wireless.h/.c`: 对外 API、静态接收队列、轮询入口、双天线扫描逻辑
  - `lt8920.h/.c`: LT8920 芯片层，负责寄存器 profile、FIFO、模式切换、状态读取
  - `wireless_port.h/.c`: STC32G 板级抽象，负责 SPI4、`RST/ANT_SEL/RXEN/TXEN/CS` 控制
  - `README.md`: 新增模块说明，记录接线、接口、运行策略、资源占用与限制
- **最小测试单元**: 在 `main.c` 新增 `Wireless_MinimalTestUnit()`，上电后一次性读取 LT8920 固定寄存器签名，确认无线 SPI 链路可读
- **Keil工程**: 在 `RVMDK/STC32G-LIB.uvproj` 中新增 `WIRELESS` 分组，并补充 `..\Code_boweny\Device\WIRELESS` 头文件搜索路径
- **系统接入**: 在 `SYS_Init()` 中新增 `Wireless_Init()`，在主循环中新增 `Wireless_Poll()`

### Bug 修复
- **[SPI模式冲突]** 原 `User/System_init.c` 中 `SPI_config()` 为示例从机模式，不适合作为 LT8920 主控。修复：无线模块在板级层单独初始化 SPI4 主机，不复用原示例 SPI 初始化
- **[引脚记录错误]** 先前文档曾误记 `RXEN=P1.7`。修复：统一修正为用户确认的 `RXEN=P1.3`
- **[文档缺失]** 初次接入后未同步模块 README、`total.md`、`date.md`。修复：补齐 `WIRELESS/README.md`，并同步更新工程总览与开发日志

### 优化改进
- **板级隔离**: 将硬件相关逻辑全部集中到 `wireless_port.*`，避免 `LT8920` 芯片层直接依赖具体 `Pxx`
- **安全收发**: 发送流程固定为 `RXEN=0 -> TXEN=1 -> TX -> TXEN=0 -> RXEN=1 -> RX`，保证半双工切换明确
- **双天线启动策略**: 初始化阶段扫描 `ANT1/ANT2`，依据 RSSI、有效包数、CRC 错误数择优后固定，降低运行时链路抖动
- **静态队列化**: 增加固定深度接收队列，不使用动态内存，方便上层异步取包

### 变更记录
- **System_init.c**: 新增 `#include "..\\Code_boweny\\Device\\WIRELESS\\wireless.h"`，并在 `log_init()` / `GPS_Init()` 后调用 `Wireless_Init()`
- **Main.c**: 新增 `Wireless_MinimalTestUnit()`，主循环新增 `Wireless_Poll()`
- **RVMDK/STC32G-LIB.uvproj**: 新增 `WIRELESS` 分组和头文件搜索路径
- **README_wireless.md**: 同步修正 `RXEN` 为 `P1.3`
- **total.md**: 同步更新无线模块状态、SPI4 占用、主循环与启动顺序

### 开发者备注
- 当前交付的是**底层驱动与原始收发框架**，不保证兼容旧遥控端业务协议
- 当前无线模块使用 SPI4：`P3.2/P3.3/P3.4/P3.5`
- 当前控制脚占用：`P5.0=RST`、`P5.1=ANT_SEL`、`P5.4=TXEN`、`P1.3=RXEN`
- 当前未使用 `PKT` 外部中断脚，全部依赖寄存器轮询
- `LT8920` 当前没有 `WHO_AM_I` 风格 ID 寄存器，最小测试使用 `Reg3/Reg6/Reg11/Reg41` 固定签名替代

---

## [2026-04-24] - v1.2.2 关闭GPS原始透传

### 新增功能
- **联调隔离**: 关闭 `GPS_RAW_ECHO_ENABLE`，让日志口不再回显 UART2 原始字节，便于单独观察 I2C 初始化阶段是否仍然卡顿

### Bug 修复
- **[调试干扰]** 原始透传会在 UART1 日志口持续插入 GPS 字节流，影响对 `QMC6309/QMI8658` 初始化日志的连续观察。修复：关闭 `UART2 -> UART1` 原始透传

### 优化改进
- **调试边界收敛**: 保持 GPS 接收和解析链路不变，只收敛调试输出，避免把串口透传带来的额外干扰误判为 I2C 卡死

### 变更记录
- **GPS.h**: `GPS_RAW_ECHO_ENABLE` 从 `1` 改为 `0`
- **GPS/README.md**: 同步更新当前透传状态说明
- **total.md**: 同步更新当前 GPS 调试状态

### 开发者备注
- 本次改动未修改 `Driver/`
- 若后续还要复测 GPS 实际串口输出，可再将 `GPS_RAW_ECHO_ENABLE` 临时切回 `1`

---

## [2026-04-24] - v1.2.1 Lamp改为P3.6闪烁

### 新增功能
- **Lamp模块**: `Lamp_init()` 改为初始化 `P3.6` 单灯输出，`Sample_Lamp()` 改为直接翻转 `P3.6`

### Bug 修复
- **[灯任务不生效]** 原工程虽然保留 `Sample_Lamp()` 任务表项，但主循环未调用 `Task_Pro_Handler_Callback()`，导致灯任务不会实际执行。修复：在主循环接入任务处理回调

### 优化改进
- **运行链路收敛**: 保持 `Timer0 ISR -> Task_Marks_Handler_Callback() -> Task_Pro_Handler_Callback() -> Sample_Lamp()` 的现有上层框架，不引入新的底层依赖

### 变更记录
- **APP_Lamp.c**: 原 `P4.0 + P6` 跑马灯改为 `P3.6` 单灯闪烁
- **Main.c**: 主循环新增 `Task_Pro_Handler_Callback()`，让 `Sample_Lamp()` 按 250ms 周期执行
- **total.md**: 同步修正任务框架已运行、`P3.6` LED 占用和主循环真实状态

### 开发者备注
- 本次改动未修改 `Driver/`
- 当前任务表中只有 `Sample_Lamp()` 处于启用状态，因此接入 `Task_Pro_Handler_Callback()` 不会重新启用其它旧示例

---

## [2026-04-24] - v1.2.0 GPS模块接入与总览修订

### 新增功能
- **GPS模块**: 新增 `Code_boweny/Device/GPS/`
  - `GPS.h`: 新增 `GPS_State_t`、`GPS_Init()`、`GPS_Reset()`、`GPS_Poll()`、`GPS_GetState()`
  - `GPS.c`: 新增 UART2 二级 FIFO、逐字符 NMEA 状态机、XOR 校验和定点字段解析
  - `README.md`: 新增 GPS 模块说明文档，记录引脚、波特率、接口、数据单位和调用顺序
- **Keil工程**: 在 `RVMDK/STC32G-LIB.uvproj` 中新增 `GPS` 分组，并补充 `..\\Code_boweny\\Device\\GPS` 头文件搜索路径
- **串口透传测试**: 新增 `GPS_RAW_ECHO_ENABLE`，当前默认将 UART2 原始接收字节直接输出到 UART1，便于现场确认 GPS 实际波特率

### Bug 修复
- **[资源冲突]** 原 `APP_AD_UART` 示例会重新占用 `Timer2`，与 GPS 所需 `UART2` 波特率发生器冲突。修复：停用 `ADtoUART_init()` 与 `Sample_ADtoUART`
- **[UART2路由不符]** 系统默认将 `UART2` 切到 `P4.6/P4.7`，与 GPS 模块要求的 `P1.0/P1.1` 不一致。修复：更新 `Switch_config()` 并在 `GPS_Init()` 中再次恢复 `UART2_SW_P10_P11`
- **[总览文档过期]** 旧版 `total.md` 与当前真实主循环、启用模块和资源占用不一致。修复：按当前源码重写总览文档
- **[状态污染]** `RMC/GGA` 等语句在字段未完全通过前就可能提前写入状态。修复：各解析函数先在局部变量中完成校验与定点转换，整句成功后再统一提交到 `GPS_State_t`
- **[定点溢出]** 经纬度转换原实现存在 32 位乘法溢出风险。修复：改为 `minutes_scaled1e4 * 1000 / 6` 的等价安全公式，并补充输入长度与范围校验

### 优化改进
- **GPS结构优化**: 将 UART2 引脚恢复、缓冲区搬运、语句采集、字段拆分和状态更新拆成清晰私有函数，保持 `GPS_Init / GPS_Poll / GPS_GetState` 职责单一
- **定点实现**: 经纬度、速度、海拔和 DOP 全部改为整数定点，彻底避免 GPS 模块内浮点运算
- **计数器补强**: 增加 `checksum_error_count / parse_error_count / uart_overflow_count / fifo_overflow_count / sentence_overflow_count`，便于后续串口诊断
- **接入顺序优化**: 将 `GPS_Init()` 放到 `APP_config()` 之后执行，避免上层再次改写 `P1` 口状态
- **Review收敛**: 复查后收紧 talker 更新、字段提交时机与数值边界，保证坏句子不会覆盖上一帧有效数据

### 变更记录
- **System_init.c**: 引入 `GPS.h`，在 `log_init()` 之后调用 `GPS_Init()`；`Switch_config()` 中将 UART2 默认路由改为 `P1.0/P1.1`
- **Main.c**: 主循环新增 `GPS_Poll()`，与现有 `IMU_HighRatePoll()` 并行运行
- **APP.c / Task.c**: 停用 `APP_AD_UART` 初始化与任务入口，释放 `Timer2` 给 GPS
- **total.md**: 先基于当前工程真实状态重写，再补充 GPS 模块与 UART2/Timer2 资源变更
- **构建验证**: 已通过 Keil C251 `Rebuild target 'Target 1'`，确认 `GPS.c` 编译并链接进 `STC32G-LIB`

### 开发者备注
- 本次改动未修改 `Driver/` 与 `User/STC32G.H`
- GPS 接收链路遵循 `Driver RX2_Buffer -> GPS FIFO -> NMEA Parser`，未重写 UART2 ISR
- 当前 `Task.c` 仍保留框架，但主循环继续采用 `GPS_Poll() + IMU_HighRatePoll()` 的运行方式
---
## [2026-04-23] - v1.1.2 Filter低通滤波模块接入

### 新增功能
- **Filter模块**: 新增 `Code_boweny/Function/Filter/` 低通滤波模块
  - `Filter.h`: 新增编译期宏、Gyro/Mag 低通 API、DOxygen 注释
  - `Filter.c`: 新增一阶 IIR + Q8 定点实现，分别维护陀螺仪与地磁两套三轴状态
  - `README.md`: 新增模块使用说明，记录公式、默认宏、原始/滤波接口关系与无 FPU 限制
- **QMI8658驱动**: 新增 `QMI8658_ReadGyroFiltered()`，在保留 `QMI8658_ReadGyro()` 原始读取接口的同时提供软件低通后的陀螺仪输出
- **QMC6309驱动**: 新增 `QMC6309_ReadXYZFiltered()`，在保留 `QMC6309_ReadXYZ()` 原始读取接口的同时提供软件低通后的地磁输出

### 优化改进
- **Filter状态管理**: 首帧有效样本直接灌入状态，避免滤波器启动瞬态导致输出被拉偏
- **初始化策略**: `QMI8658_Init()` 与 `QMC6309_Init()` 成功完成后自动复位各自滤波状态，避免重初始化后沿用旧状态
- **参数管理**: 低通平滑系数改为 `Filter.h` 编译期宏统一管理，便于在无 FPU 场景下快速调参

### 变更记录
- **Keil工程**: 在 `RVMDK/STC32G-LIB.uvproj` 中新增 `FILTER` 分组，并补充 `..\Code_boweny\Function\Filter` 头文件搜索路径
- **QMI8658.c/.h**: 保持原始陀螺仪接口不变，新增滤波接口声明与实现，并补充 DOxygen 注释
- **QMC6309.c/.h**: 保持原始地磁接口不变，新增滤波接口声明与实现，并补充 DOxygen 注释
- **README文档**: 更新 `QMI8658/README.md`、`QMC6309/README.md`，并新增 `Filter/README.md`
- **total.md**: 补充 `Code_boweny/Function/Filter` 模块说明、固定点低通说明与版本历史

### 开发者备注
- 原始接口 `QMI8658_ReadGyro()` / `QMC6309_ReadXYZ()` 保持现有行为不变，兼容现有调用方
- 当前软件低通仅接入陀螺仪与地磁数据，不修改 QMI8658 内部硬件 DLPF 配置
- 默认滤波参数为 `FILTER_GYRO_LPF_SHIFT=2`、`FILTER_MAG_LPF_SHIFT=2`；后续若需调参，仅修改 `Filter.h` 即可

---

## [2026-04-23] - v1.1.1 QMI8658 工程接入与联调完成

### 新增功能
- **Keil工程**: 将 `Code_boweny/Device/QMI8658/QMI8658.c` 纳入 `STC32G-LIB.uvproj` 编译，并补齐 `IncludePath`
- **启动自检**: 在 `User/System_init.c` 中新增 QMI8658 上电自检流程，启动阶段读取 `WHO_AM_I` 与首帧加速度数据
- **高频读取**: 在 `User/Main.c` 中新增高频 `QMI8658_ReadAcc()` 轮询，并按抽样比例输出 UART1 日志

### Bug 修复
- **[共享I2C启动顺序]** `APP_config()` 会重配 `P1.4/P1.5`，导致传感器总线状态被覆盖。修复：在 `APP_config()` 后通过 `Sensor_I2C_prepare()` 恢复共享硬件 I2C 总线，再依次初始化 QMC6309 和 QMI8658
- **[Keil C51兼容]** 修正 `QMI8658.c/.h` 中影响 C251 编译的关键字/宏使用问题，保证 `QMI8658.c`、`System_init.c`、`Main.c` 可独立通过编译
- **[Ready判据]** 将 QMI8658 加速度 ready 轮询收敛到 `STATUS0.aDA`，避免混用 `STATUSINT` 带来的排查歧义
- **[日志歧义]** 将 `readacc addr=0x6B` 调整为 `i2c_addr=0x6B data_reg=0x35`，明确区分设备地址和数据寄存器地址

### 优化改进
- **日志输出**: 压缩 QMI8658 冗余 DEBUG/无效数据刷屏日志，仅保留 `WHO_AM_I`、ready、首帧数据和高频抽样加速度输出
- **启动策略**: 自检失败只记日志，不阻断系统继续启动，便于串口排查

### 变更记录
- **System_init.c**: 新增 `g_qmi8658_ready`、`Sensor_I2C_prepare()`、`QMI8658_PowerOnSelfTest()`
- **Main.c**: 保持 `main()` 空循环结构，在循环中接入高频 `IMU_HighRatePoll()`
- **QMI8658 README**: 补充共享 I2C 联调成功、当前 bring-up 配置和串口验证示例
- **total.md**: 补充共享 I2C 共线验证结果与工程当前状态

### 开发者备注
- 当前实机验证日志已确认：`[IMU] I: ready addr=0x6B id=0x05 acc=...`
- 后续若继续调陀螺仪量程/ODR，优先在当前可工作 bring-up 配置基础上逐项调整，不要一次性改动多组 CTRL 配置

---
## [2026-04-22] - v1.0 初始化

### 新增功能
- **整体工程**: 建立 Black Pearl v1.1 工程框架，基于 STC32G
- **Driver层**: 新增20个标准外设驱动模块（GPIO、UART、I2C、SPI、PWM、Timer、ADC、DMA、RTC、WDT、CAN、LIN、Clock、NVIC、Exti、Compare、EEPROM、Soft_UART、Soft_I2C、Delay、LCM）
- **App层**: 新增20个应用层示例模块
- **Device层**: 新增 LOG 日志系统、QMI8658 IMU 驱动、QMC6309 地磁计驱动
- **文档**: 新增 `total.md` 工程总览文档

### 配置信息
- 主时钟频率: 24MHz（`MAIN_Fosc = 24000000L`）
- 当日启用的外设初始化: `Lamp_init()`、`ADtoUART_init()`
- 其他外设初始化已注释备用

### 开发者备注
- 工程采用三层架构：User(芯片层) → Driver(驱动层) → Device/App(应用层)
- 硬件I2C固定引脚: SCL=P1.5, SDA=P1.4
- 扩展SFR访问需设置 EAXFR=1
- LOG系统依赖UART1初始化完成

---

## [2026-04-22] - v1.0.1 LOG日志模块实现

### 新增功能
- **Log模块**: 新增 `Code_boweny/Function/Log/` 轻量化串口日志模块
  - `Log.h`: API声明、DOxygen注释、4级日志宏 (LOGI/LOGW/LOGE/LOGD)
  - `Log.c`: 内部实现，支持带标签分级日志、原始日志、缓冲区溢出保护
- **Log README**: 新增 `Code_boweny/Function/Log/README.md` 模块使用说明
- **System_init.c**: 启用 `UART_config()` 并新增 `log_init()` 调用

### 变更记录
- **System_init.c**: `UART_config()` 由注释状态改为启用 (SYS_Init 第229行)
- **System_init.c**: `SYS_Init()` 中新增 `log_init()` 调用 (需在 `APP_config()` 之后)

### 技术细节
- 日志级别: INFO(I) / WARN(W) / ERROR(E) / DEBUG(D)
- 输出格式: `[tag] X: message\r\n`
- 原始输出: `message\r\n`
- 缓冲区: 128字节，最大127字符消息
- 波特率: 115200, 8N1, Timer1 作为波特率发生器
- 依赖: `STC32G_UART.h` (PrintString1), `stdarg.h`, `config.h`

### 开发者备注
- LOG系统必须严格按顺序调用: `UART_config()` → `log_init()` → `LOG*()`
- 禁止使用 `%f` 格式化，浮点值需先转为整数
- 日志缓冲区溢出时自动截断，末尾追加 `\r\n`
- UART1 引脚: P3.1=TXD, P3.0=RXD (已通过 `Switch_config()` 配置)

---

## [2026-04-22] - v1.0.3 QMI8658 IMU驱动实现

### 新增功能
- **QMI8658驱动**: 新增 `Code_boweny/Device/QMI8658/` 6轴IMU驱动库
  - `QMI8658.h`: 寄存器定义、I2C地址宏、量程/ODR配置宏、API声明、DOxygen注释
  - `QMI8658.c`: 完整驱动实现，调用 Driver 层 `I2C_WriteNbyte` / `I2C_ReadNbyte`，关键步骤嵌入LOG调试
  - `README.md`: 驱动使用说明文档，含最小测试单元、灵敏度换算表、API参考
- **功能特性**:
  - I2C主/备地址自动探测 (0x6B / 0x6A)
  - WHO_AM_I检测 (0x05)
  - RESET_STATE就绪检测 (0x80)
  - 软复位流程 (写入0xB0到寄存器0x60)
  - 分阶段寄存器配置写入 (CTRL1→CTRL2→CTRL3→CTRL5→CTRL7)
  - 读回验证
  - 数据有效性检查 (拒绝全零/全-1帧)
  - 加速度/陀螺仪/温度/全量9轴分别读取
  - ODR可配置 (3~940Hz加速度, 29~7520Hz陀螺仪)
  - 量程可配置 (加速度±2/4/8/16G, 陀螺仪±16~2048°/s)
  - I2C总线恢复 (9时钟脉冲释放SDA)
  - 默认配置: ACC ±4G / ODR=117Hz, GYRO ±128°/s / ODR=117Hz

### 变更记录
- **QMI8658.h**: 新建文件，v1.0
- **QMI8658.c**: 新建文件，v1.0 (原文件为空)
- **QMI8658/README.md**: 新建文档

### 技术细节
- 总线: STC32G 硬件I2C (P1.4=SDA, P1.5=SCL)
- I2C操作: 直接调用 Driver 层 `I2C_WriteNbyte()` / `I2C_ReadNbyte()`
- 依赖: `STC32G_I2C.h` / `STC32G_Delay.h` / `STC32G_GPIO.h` / `Log.h`
- LOG标签: `[IMU]` (INFO/WARN/ERROR/DEBUG四级)
- 数据格式: int16有符号整数，小端序
- 初始化流程: 地址探测(3次) → WHO_AM_I验证(0x05) → RESET_STATE检测 → 软复位(必要时) → 分阶段配置写入 → 读回验证 → 等待数据就绪
- 与QMC6309共用I2C总线，互不影响

### 开发者备注
- 调用顺序: `I2C_config()` → `QMI8658_Init()` → `QMI8658_ReadAcc/ReadGyro/ReadAll/...`
- 禁止使用浮点运算，数据全程int16整数传递；如需转换为物理量，在应用层用定点整数方式
- 灵敏度: 加速度±4G→8192 LSB/g, 陀螺仪±128°/s→256 LSB/°/s
- 默认量程和ODR可在 `QMI8658.h` 的 `QMI8658_CTRL2_INIT` / `QMI8658_CTRL3_INIT` 宏中修改
- I2C总线异常时调用 `QMI8658_BusRecover()` 恢复
- QMI8658和QMC6309可同时使用，共用同一I2C总线

---

## [2026-04-22] - v1.0.2 QMC6309 地磁计驱动实现

### 新增功能
- **QMC6309驱动**: 新增 `Code_boweny/Device/QMC6309/` 地磁计驱动库
  - `QMC6309.h`: 寄存器定义、I2C地址宏、ODR配置、API声明、DOxygen注释
  - `QMC6309.c`: 完整驱动实现，调用 Driver 层 `I2C_WriteNbyte` / `I2C_ReadNbyte`，关键步骤嵌入LOG调试
  - `README.md`: 驱动使用说明文档，含最小测试单元
- **功能特性**:
  - I2C主/备地址自动探测 (0x7C / 0x0C)
  - 软复位流程 (CONTROL_2=0x80)
  - 上电就绪轮询检测 (替代固定延时)
  - 数据有效性检查 (拒绝全零/全-1帧)
  - ODR可配置 (1/10/50/100/200Hz)，默认50Hz
  - I2C总线恢复 (9时钟脉冲释放SDA)

### 变更记录
- **date.md**: 新增本章节 (v1.0.2) 记录QMC6309驱动实现
- **System_init.c**: 取消 `I2C_config()` 注释；调整 `log_init()` 顺序至 `APP_config()` 之后；新增 `QMC6309_Init()` 调用
- **Main.c**: 在 `SYS_Init()` 后添加 CHIP_ID 读取验证和 XYZ 单次读取测试代码
- **QMC6309.h**: 新增 `#include STC32G_GPIO.h`（BusRecover 引脚操作依赖）

### 技术细节
- 总线: STC32G 硬件I2C (P1.4=SDA, P1.5=SCL)
- I2C操作: 直接调用 Driver 层 `I2C_WriteNbyte()` / `I2C_ReadNbyte()`，不重复手搓
- 依赖: `STC32G_I2C.h` / `STC32G_Delay.h` / `STC32G_GPIO.h` / `Log.h`
- LOG标签: `[MAG]` (INFO/WARN/ERROR/DEBUG四级)
- 数据格式: int16有符号整数，小端序
- 初始化流程: 地址探测(3次) -> 上电等待(1000ms) -> CHIP_ID验证(0x90) -> 软复位 -> 配置写入 -> 读回验证

### 开发者备注
- 调用顺序: `I2C_config()` -> `QMC6309_Init()` -> `QMC6309_ReadXYZ()` / 其他API
- 禁止使用浮点运算，数据全程int16整数传递
- 原始数据需应用层校准（硬铁/软铁）才能得到准确航向角
- 当前默认ODR为50Hz，如需更高输出速率在 `QMC6309_Init()` 前修改 `QMC6309_CTRL2_INIT`
- I2C总线异常时调用 `QMC6309_BusRecover()` 恢复

---

