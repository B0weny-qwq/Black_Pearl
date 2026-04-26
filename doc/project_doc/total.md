/**
 * @file    total.md
 * @brief   Black Pearl v1.1 工程总览文档
 *
 * @author  boweny
 * @date    2026-04-27
 * @version v1.6
 *
 * @details
 * 本文档基于 2026-04-26 当前工程实际代码重新整理，
 * 在 GPS、IMU、MAG 已接入基础上，补充 WIRELESS 模块接入后的真实运行链路、
 * 资源占用与约束边界，并补充 MOTOR PWM 驱动与 PID 功能模块。
 *
 * @note    详细变更记录请查阅 date.md
 *
 * @see     date.md
 */

# Black Pearl v1.1 - STC32G 工程总览

> 本文档已按当前工程真实状态更新，不再沿用旧版“计划中的运行流”。
> 若代码与旧描述冲突，以本文档和源码现状为准。

---

## 1. 工程基本信息

| 项目 | 说明 |
|------|------|
| 工程名称 | Black Pearl v1.1 (STC32G_Library) |
| MCU 芯片 | STC32G |
| 主时钟 | `MAIN_Fosc = 24000000L` |
| 编译器 | Keil MDK / C251 |
| 开发方式 | `User + Driver + App/Code_boweny` 三层结构 |
| 当前日志口 | UART1 (`P3.0/P3.1`) |
| 当前 GPS 口 | UART2 (`P1.0/P1.1`) |

---

## 2. 目录结构

```text
Black_Pearl_v1.1/
├── User/                     # 系统入口、初始化、主循环、任务框架
├── Driver/                   # STC 官方底层外设库（禁止项目内修改）
├── App/                      # 官方示例应用层
├── Code_boweny/             # 项目扩展模块
│   ├── Function/Log/        # UART1 日志系统
│   ├── Function/Filter/     # Q8 定点低通滤波
│   ├── Function/PID/        # Q10 定点 PID 控制器
│   └── Device/
│       ├── QMC6309/         # 地磁计驱动
│       ├── QMI8658/         # IMU 驱动
│       ├── GPS/             # GPS NMEA 解析模块
│       ├── WIRELESS/        # LT8920 + KCT8206L 无线驱动
│       └── MOTOR/           # PWMA3/PWMA4 双电机 PWM 驱动
├── RVMDK/                    # Keil 工程文件
└── doc/
    ├── project_doc/         # total.md / date.md
    ├── build_doc/           # 功能模块开发手册
    └── device_doc/          # 设备说明文档
```

---

## 3. 当前软件结构与真实运行流

### 3.1 分层职责

| 层级 | 目录 | 职责 |
|------|------|------|
| 芯片/系统层 | `User/` | 入口、初始化、主循环、任务框架 |
| 底层驱动层 | `Driver/` | UART/I2C/SPI/Timer/GPIO 等标准外设 API |
| 示例应用层 | `App/` | 官方示例模块 |
| 项目设备层 | `Code_boweny/Device/` | QMC6309 / QMI8658 / GPS / WIRELESS 等项目设备驱动 |
| 项目功能层 | `Code_boweny/Function/` | LOG / Filter / PID 等通用功能模块 |

### 3.2 当前真实启动顺序

当前 `SYS_Init()` 实际运行顺序如下：

```text
EAXSFR()
-> GPIO_config()
-> Switch_config()
-> Timer_config()
-> UART_config()
-> I2C_config()
-> EA = 1
-> APP_config()
-> log_init()
-> GPS_Init()
-> Wireless_Init()
-> Sensor_I2C_prepare()
-> QMC6309_Init()
-> QMI8658_PowerOnSelfTest()
```

### 3.3 当前真实主循环

当前 `User/Main.c` 已接入 GPS、无线协议轮询、任务处理和 IMU 高频读取：

```c
Wireless_MinimalTestUnit();
while (1)
{
    GPS_Poll();
    Wireless_Poll();
    ShipProtocol_Poll();
    Wireless_SearchSignalPoll();
    Task_Pro_Handler_Callback();
    IMU_HighRatePoll();
}
```

含义：

- `Wireless_MinimalTestUnit()` 在系统启动后执行一次，读取 LT8920 固定寄存器签名，确认无线 SPI 链路可读
- `GPS_Poll()` 高优先级处理 UART2 字节流，避免 GPS 串口堆积
- `Wireless_Poll()` 高频轮询 LT8920 状态，维持常驻 RX 和发包后自动回 RX
- `ShipProtocol_Poll()` 消费无线收包并维护船端协议状态
- `Wireless_SearchSignalPoll()` 执行无线搜索/信号扫描相关轮询
- `Task_Pro_Handler_Callback()` 消费 Timer0 标记任务，当前用于驱动 `P3.6` LED 闪烁
- `IMU_HighRatePoll()` 继续维持 QMI8658 高频轮询
- `DisplayScan()` 当前仍未接入主循环
- `Task.c` 当前已作为 LED 闪烁调度器参与运行

---

## 4. 当前启用模块状态

### 4.1 User 层

| 模块 | 状态 | 说明 |
|------|------|------|
| `GPIO_config()` | 启用 | 初始化端口模式，I2C 口先设为开漏 |
| `Switch_config()` | 启用 | 功能脚切换，UART2 已改为 `P1.0/P1.1` |
| `Timer_config()` | 启用 | 仅启用 Timer0 1ms 中断 |
| `UART_config()` | 启用 | 只初始化 UART1，用于 LOG |
| `I2C_config()` | 启用 | 初始化硬件 I2C |
| `Wireless_Init()` | 启用 | 初始化 LT8920、SPI4 和双天线扫描 |
| `APP_config()` | 启用 | 当前保留 `Lamp_init()`，配置 `P3.6` LED |
| `Task.c` | 启用 | 主循环已调用，当前执行 `Sample_Lamp()` |

### 4.2 App 层

当前 `APP_config()` 已调整为：

- `Lamp_init()`：保留，改为 `P3.6` LED 初始化
- `ADtoUART_init()`：停用
- 其他示例：保持注释禁用

停用 `ADtoUART_init()` 的原因：

- 该示例原先会重新占用 `Timer2`
- UART2 固定依赖 `Timer2` 作为波特率发生器
- 若继续启用，会与 GPS 模块产生资源冲突

### 4.3 Code_boweny 扩展模块

| 模块 | 状态 | 说明 |
|------|------|------|
| `Function/Log` | 启用 | UART1 日志输出 |
| `Function/Filter` | 启用 | QMI8658 / QMC6309 定点低通 |
| `Function/PID` | 已纳入工程 | Q10 定点 PID 控制器，当前提供 API，默认未在启动流中调用 |
| `Device/QMC6309` | 启用 | 硬件 I2C 地磁计 |
| `Device/QMI8658` | 启用 | 硬件 I2C IMU |
| `Device/GPS` | 启用 | UART2 NMEA0183 解析模块 |
| `Device/WIRELESS` | 启用 | LT8920 + KCT8206L 半双工无线驱动 |
| `Device/MOTOR` | 已纳入工程 | PWMA CH3/CH4 双电机驱动，当前提供 API，默认未在启动流中调用 |

---

## 5. 当前通信资源与引脚占用

| 资源 | 当前用途 | 引脚/路由 | 备注 |
|------|----------|-----------|------|
| UART1 | LOG 输出 | `P3.0=RX / P3.1=TX` | `UART_config()` 初始化，使用 Timer1 |
| UART2 | GPS 模块 | `P1.0=RX / P1.1=TX` | `GPS_Init()` 初始化，固定使用 Timer2 |
| I2C | QMC6309 + QMI8658 | `P1.4=SDA / P1.5=SCL` | 共享硬件 I2C 总线 |
| SPI4 | WIRELESS 模块 | `P3.2=SCLK / P3.3=MISO / P3.4=MOSI / P3.5=CS` | 由 `WirelessPort_Init()` 运行时切换并初始化为主机 |
| Timer0 | 系统 1ms 节拍 | - | 当前用于任务标记与 LED 闪烁调度基准 |
| Timer1 | UART1 波特率发生器 | - | 给 LOG 使用 |
| Timer2 | UART2 波特率发生器 | - | 已专门留给 GPS |
| P3.6 | 单灯闪烁 | `P3.6` | `Lamp_init()` 配置，`Sample_Lamp()` 250ms 翻转 |
| P5.0 | 无线复位 | `P5.0` | LT8920 `RST` |
| P5.1 | 天线选择 | `P5.1` | `ANT_SEL` |
| P5.4 | 发射使能 | `P5.4` | `TXEN` |
| P1.3 | 接收使能 | `P1.3` | `RXEN` |
| PWMA CH3 | 左电机 PWM | `PWM3P_2=P2.4 / PWM3N_2=P2.5` | `MLB=P / MLA=N` |
| PWMA CH4 | 右电机 PWM | `PWM4P_2=P2.6 / PWM4N_2=P2.7` | `MRB=P / MRA=N` |

### 5.1 当前功能脚切换结果

`Switch_config()` 当前配置：

- `UART1_SW(UART1_SW_P30_P31)`
- `UART2_SW(UART2_SW_P10_P11)`
- `UART3_SW(UART3_SW_P00_P01)`
- `UART4_SW(UART4_SW_P02_P03)`
- `I2C_SW(I2C_P14_P15)`
- `SPI_SW(SPI_P22_P23_P24_P25)`

MOTOR 模块在 `Motor_Init()` 内部独立执行：

- `PWM3_USE_P24P25()`：`PWM3P_2 -> P2.4(MLB)`，`PWM3N_2 -> P2.5(MLA)`
- `PWM4_USE_P26P27()`：`PWM4P_2 -> P2.6(MRB)`，`PWM4N_2 -> P2.7(MRA)`

说明：

- `Switch_config()` 仍保留默认 `SPI_P22_P23_P24_P25`
- 无线模块在 `WirelessPort_Init()` 中会运行时切换为 `SPI_P35_P34_P33_P32`
- 当前已启用模块中，没有其它运行链路依赖 SPI 第 2 组或第 4 组
- MOTOR 当前不在 `SYS_Init()` 中自动调用，避免上电后电机误动作；需要运动控制时由上层显式调用 `Motor_Init()`

---

## 6. 当前设备模块说明

### 6.1 LOG 模块

- 路径：`Code_boweny/Function/Log/`
- 输出口：UART1
- 调用前提：`UART_config()` 完成后再 `log_init()`
- 当前用途：QMC6309 / QMI8658 / GPS 初始化与错误日志

### 6.1.1 PID

- 路径：`Code_boweny/Function/PID/`
- 类型：通用位置式 PID 控制器
- 数据格式：`kp/ki/kd` 使用 Q10 定点数，`1024 = 1.0`
- 状态模型：每个 `PID_Controller_t` 独立保存目标值、积分、上一帧误差和限幅配置
- 当前用途：作为后续电机速度环、航向环、姿态角速度环的基础控制函数，当前未在启动流中自动调用
- 当前对外接口：

```c
void PID_Init(PID_Controller_t *pid,
              int16 kp, int16 ki, int16 kd,
              int16 output_min, int16 output_max,
              int32 integral_min, int32 integral_max);
void PID_Reset(PID_Controller_t *pid);
void PID_SetTarget(PID_Controller_t *pid, int16 target);
void PID_SetGains(PID_Controller_t *pid, int16 kp, int16 ki, int16 kd);
void PID_SetOutputLimit(PID_Controller_t *pid, int16 output_min, int16 output_max);
void PID_SetIntegralLimit(PID_Controller_t *pid, int32 integral_min, int32 integral_max);
int16 PID_Update(PID_Controller_t *pid, int16 measured);
int16 PID_UpdateTarget(PID_Controller_t *pid, int16 target, int16 measured);
```

### 6.2 QMC6309

- 路径：`Code_boweny/Device/QMC6309/`
- 总线：硬件 I2C `P1.4/P1.5`
- 当前状态：启动阶段初始化并读回寄存器
- 与 QMI8658 共线运行

### 6.3 QMI8658

- 路径：`Code_boweny/Device/QMI8658/`
- 总线：硬件 I2C `P1.4/P1.5`
- 当前状态：启动自检后，在主循环中由 `IMU_HighRatePoll()` 高频轮询加速度数据
- 自检成功后置 `g_qmi8658_ready = 1`

### 6.4 GPS

- 路径：`Code_boweny/Device/GPS/`
- 接口：UART2 `P1.0/P1.1`
- 默认波特率：`GPS_BAUDRATE = 115200`
- 当前调试状态：`GPS_RAW_ECHO_ENABLE = 0`，当前已关闭 UART2 原始透传，便于单独观察 I2C 初始化链路
- 当前支持语句：`GGA / RMC / GSA / GSV / VTG`
- 数据结构：`GPS_State_t`
- 当前对外接口：

```c
s8 GPS_Init(void);
void GPS_Reset(void);
void GPS_Poll(void);
const GPS_State_t *GPS_GetState(void);
```

### 6.5 GPS 当前实现要点

- 接收链路：`RX2_Buffer -> GPS FIFO -> NMEA 状态机`
- 不修改 Driver 层 UART2 ISR
- 使用 XOR 校验，不接受未校验通过的语句
- 支持语句只有在整句解析成功后才提交到 `GPS_State_t`
- `RMC` 作为主状态更新源，`update_sequence` 仅在新的 `RMC UTC/Date` 到达时递增
- 经纬度使用 `deg1e7`，速度使用 `x100`，海拔使用 `cm`，DOP 使用 `x100`
- 经纬度定点换算已按 32 位整型上限做安全收敛，避免溢出
- `GSV` 当前只汇总 `satellites_view` 与 `max_snr`，不维护完整卫星表

### 6.6 WIRELESS

- 路径：`Code_boweny/Device/WIRELESS/`
- 架构：`wireless.*` + `lt8920.*` + `wireless_port.*`
- 射频芯片：单颗 `LT8920`
- 前端芯片：`KCT8206L`
- 当前总线：硬件 `SPI4`
- 当前模式：单芯片半双工，默认常驻 RX，发送时切 TX，发完立即回 RX
- 双天线策略：启动扫描 `ANT1/ANT2` 后固定到较优天线
- 当前对外接口：

```c
s8 Wireless_Init(void);
s8 Wireless_Deinit(void);
s8 Wireless_Poll(void);
s8 Wireless_Send(const u8 *data, u8 len);
s8 Wireless_Receive(u8 *data, u8 buf_len, u8 *out_len);
s8 Wireless_SetAntenna(u8 ant_sel);
s8 Wireless_GetState(Wireless_State_t *state);
s8 Wireless_RescanAntenna(void);
```

### 6.7 WIRELESS 当前实现要点

- 不复用 `User/System_init.c` 中原示例 `SPI_config()`，避免从机模式和重复初始化冲突
- 板级抽象集中在 `wireless_port.*`，硬件脚位不散落到业务层
- 当前交付的是底层驱动和原始包收发框架，不包含旧 `wirelessProtocal.c` 业务协议兼容层
- 当前不使用 `PKT` 外部中断脚，全部依赖寄存器轮询
- 当前无线运行不会修改 UART/I2C/Timer 资源分配
- `main()` 中已增加一次性最小测试单元，用固定寄存器签名替代不存在的 `WHO_AM_I`

### 6.8 MOTOR

- 路径：`Code_boweny/Device/MOTOR/`
- 定时器资源：`PWMA`
- 左电机：`PWM3N_2 -> MLA(P2.5)`，`PWM3P_2 -> MLB(P2.4)`
- 右电机：`PWM4N_2 -> MRA(P2.7)`，`PWM4P_2 -> MRB(P2.6)`
- 输出定义：`MRA/MLA = N`，`MRB/MLB = P`
- 默认周期：`MOTOR_PWM_PERIOD = 1000`
- 速度范围：`-1000 ~ +1000`，超范围自动限幅
- 当前对外接口：

```c
void Motor_Init(void);
void Motor_SetSpeed(Motor_Id_t motor, int16 speed);
void Motor_SetBothSpeed(int16 left_speed, int16 right_speed);
void Motor_Stop(Motor_Id_t motor);
void Motor_StopAll(void);
int16 Motor_GetSpeed(Motor_Id_t motor);
```

### 6.9 MOTOR 当前实现要点

- 使用 `PWM3/PWM4` 的 P/N 互补输出，而不是把 P/N 当作两个独立占空比通道
- 正速度默认使用 P 侧作为主动方向，负速度通过极性翻转使用 N 侧作为主动方向
- 速度为 0 时关闭对应 PWM 通道输出，避免互补输出在 0 占空比下仍有一路处于有效态
- 切换方向前先关闭对应电机输出，再更新极性和占空比，降低方向切换瞬态风险
- 当前没有加入 `SYS_Init()` 自动初始化，防止系统启动时电机意外动作

---

## 7. 任务系统当前状态

`User/Task.c` 仍然是 Timer0 驱动的轮询调度框架，当前工程的实际状态如下：

1. 主循环已经调用 `Task_Pro_Handler_Callback()`
2. `Sample_ADtoUART` 已停用，仅剩 `Sample_Lamp` 保留在任务表中

因此当前运行时：

- 任务框架已编译
- Timer0 中断仍会置任务运行标志
- 主循环会执行任务处理函数
- 当前唯一实际运行的任务是 `Sample_Lamp()`，用于 `P3.6` LED 闪烁

这也是本次修订 `total.md` 的重点之一：文档必须反映“当前真实运行链路”，而不是旧版计划流。

---

## 8. 关键开发约束

### 8.1 扩展 SFR 访问

STC32G 大量外设寄存器位于扩展 SFR 区，访问前必须确保 `EAXFR=1`。
当前工程已在 `SYS_Init()` 开始执行 `EAXSFR()`。

### 8.2 禁止浮点运算

- STC32G 无 FPU
- GPS / IMU / MAG / Filter 全部使用定点整数
- LOG 中禁止 `%f`

### 8.3 Driver 层不可修改

以下目录禁止项目内业务修改：

- `Driver/inc/`
- `Driver/src/`
- `Driver/isr/`
- `User/STC32G.H`

项目扩展必须放在：

- `User/`
- `App/`
- `Code_boweny/`
- `doc/`
- `RVMDK/`（仅工程纳入与分组同步）

### 8.4 UART2 与 Timer2 绑定约束

- UART2 固定使用 Timer2 作为波特率发生器
- 因此 GPS 模块接入后，不能再启用 `APP_AD_UART` 这类会重新占用 Timer2 的示例
- 资源冲突应在上层规避，不允许通过修改 Driver 层实现绕过

### 8.5 GPS 接收边界

- 不重写 UART2 ISR
- 不直接清空 Driver 内部缓冲管理逻辑
- GPS 模块只做增量消费 `COM2.RX_Cnt / RX2_Buffer`
- 上层若需要 GPS 数据，只通过 `GPS_GetState()` 读取

### 8.6 WIRELESS 资源边界

- 不修改 `Driver` 层 SPI/GPIO/UART/I2C 实现
- 不启用 `User/System_init.c` 中原有 `SPI_config()`，无线模块自行初始化 SPI4 主机
- `P5.4` 不可再用于 `MCLKO/SS_3/PWM6_2`
- `P5.0/P5.1` 不可再用于比较器输入
- 无线接入后，不能再并行启用依赖旧 SPI 示例引脚的 `APP_SPI_PS` 类模块

### 8.7 MOTOR 资源边界

- `PWMA CH3/CH4` 已预留给 MOTOR 模块
- `P2.4/P2.5/P2.6/P2.7` 作为电机 PWM 输出后，不应再被 LCM 数据口、SPI 第二组或其它 GPIO 业务复用
- 不能并行启用 `APP_PWMA_Output` 中使用 `PWM3/PWM4` 的示例输出
- MOTOR 不占用 Timer0/Timer1/Timer2，不影响任务节拍、UART1 LOG 和 UART2 GPS

### 8.8 PID 使用边界

- PID 模块只做控制量计算，不直接操作 PWM、GPIO 或传感器
- 禁止传入浮点增益，所有增益必须换算为 Q10 定点值
- 对同一个控制对象应使用独立 `PID_Controller_t`，不要让左右电机或不同控制环共用同一个 PID 状态
- 切换控制模式或目标对象时建议调用 `PID_Reset()`，避免沿用旧积分导致输出突跳

---

## 9. 文档体系

| 文档 | 路径 | 用途 |
|------|------|------|
| `total.md` | `doc/project_doc/total.md` | 当前工程总览 |
| `date.md` | `doc/project_doc/date.md` | 变更日志 |
| `README_GPS.md` | `doc/build_doc/README_GPS.md` | GPS 模块开发参考手册 |
| `GPS/README.md` | `Code_boweny/Device/GPS/README.md` | GPS 模块实现说明 |
| `README_wireless.md` | `doc/build_doc/README_wireless.md` | 无线模块移植与接入说明 |
| `WIRELESS/README.md` | `Code_boweny/Device/WIRELESS/README.md` | 无线模块实现说明 |
| `MOTOR/README.md` | `Code_boweny/Device/MOTOR/README.md` | PWM 电机驱动实现说明 |
| `PID/README.md` | `Code_boweny/Function/PID/README.md` | PID 定点控制器实现说明 |
| `LOG.md` | `doc/device_doc/LOG.md` | LOG 模块说明 |
| `IMU_QMI8658.md` | `doc/device_doc/IMU_QMI8658.md` | QMI8658 说明 |
| `QMC6309.md` | `doc/device_doc/QMC6309.md` | QMC6309 说明 |

---

## 10. 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-22 | v1.0 | 初版工程总览建立 |
| 2026-04-23 | v1.1 | 补充 QMC6309 / QMI8658 / Filter 相关说明 |
| 2026-04-24 | v1.3 | 按当前工程真实状态重写总览，补充 GPS 模块、UART2 路由、Timer2 资源调整和当前主循环事实 |
| 2026-04-26 | v1.4 | 补充 WIRELESS 模块接入、SPI4 资源占用、双天线策略与真实启动/主循环链路 |
| 2026-04-27 | v1.5 | 新增 MOTOR PWM 驱动模块说明，补充 PWMA CH3/CH4 与 P2.4~P2.7 引脚占用 |
| 2026-04-27 | v1.6 | 新增 Function/PID 定点 PID 控制器说明，补充 Keil 工程纳入状态与使用边界 |

---

> 关联文档：详细变更记录、GPS 接入记录和 review 结论请查阅 `date.md`。
