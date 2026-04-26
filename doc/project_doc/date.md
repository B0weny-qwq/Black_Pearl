/**
 * @file    date.md
 * @brief   Black Pearl v1.1 开发日志
 *
 * @author  boweny
 * @date    2026-04-22
 * @version v1.0
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

# Black Pearl v1.1 - 开发日志 (date.md)

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

