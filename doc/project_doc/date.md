/**
 * @file    date.md
 * @brief   Black Pearl v1.1 开发日志
 *
 * @author  boweny
 * @date    2026-05-01
 * @version v1.7.18
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

## [2026-05-01] - v1.7.18 AHRS yaw漂移与打印延迟优化

### Bug 修复
- **[AHRS yaw漂移]** 当前 QMC6309 尚未完成硬铁/软铁校准和倾斜补偿，地磁慢修正会在静止时继续拉动 yaw，导致 `yr` 锁零后仍缓慢偏移。修复：`AHRS_MAG_ENABLE` 默认改为 `0`，保留地磁读取和有效位，但 yaw 暂时只采用陀螺相对积分，避免未校准磁力计污染 90 度转向测试。

### 优化改进
- **[UART1日志负载]** 新增 `LOG_UART_BAUDRATE` 宏，当前 UART1 调试口默认回退到 `57600`，降低高频日志对联调阶段时序的扰动；需要时仍可在 `config.h` 覆盖。
- **[AHRS日志节流]** 恢复 `AHRS_LOG_DECIMATION=32`，把 AHRS 正常日志间隔保持在约 `544ms`，避免串口打印过密时影响 IMU I2C 轮询稳定性。

### 开发者备注
- 后续若需要恢复绝对航向，可在 `config.h` 中显式定义 `AHRS_MAG_ENABLE=1`，但应先完成 QMC6309 校准、磁场模长门控和倾斜补偿验证。

---

## [2026-05-01] - v1.7.17 AHRS与控制闭环策略汇报文档

### 新增功能
- **[AHRS汇报文档]** 新增 `doc/project_doc/ahrs_report.md`，说明当前 IMU + 磁力计融合算法、flags 判断逻辑、UART 日志解读、限制和后续升级路线。
- **[控制策略文档]** 新增 `doc/project_doc/control_strategy_report.md`，按当前硬件条件说明可闭环对象、不可闭环对象、推荐控制层级和后续升级路线。

### 优化改进
- **[汇报口径]** 明确当前无电机编码器/转速反馈，因此电机推力仍为开环 PWM；第一阶段优先做 QMI8658 陀螺 Z 轴偏航角速度闭环，再叠加短时航向保持外环。
- **[质量门控]** 明确磁力计和 GPS 只作为低频航向参考，必须经过 `flags`、磁场模长、GPS 定位和速度门控后才能参与航向控制。

---

## [2026-05-01] - v1.7.16 AHRS相对航向锁零等待航向稳定

### 优化改进
- **[AHRS日志]** `yr` 不再只等 `flags=0x1F` 后立即锁零；现在必须在陀螺零偏 ready 后，yaw 连续 6 个日志采样变化不超过 `1.50 deg` 才锁定相对 yaw 零点。
- **[AHRS调试]** 相对 yaw 未锁定时输出 `ys=稳定计数` 并继续保留 `g=`，用于观察地磁航向是否仍在慢收敛。避免刚进入 `0x1F` 时地磁还在拖 yaw，导致 `yr` 被启动收敛误差污染。

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


## [2026-04-22] -  工程要宏定义传感器 实现可移植
