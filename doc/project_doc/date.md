/**
 * @file    date.md
 * @brief   Black Pearl v1.1 开发日志
 *
 * @author  boweny
 * @date    2026-05-13
 * @version v1.7.62
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
- **[船体 yaw 自稳参数收口]** `SHIP_YAW_HOLD_*` 参数统一收在 `User/FeatureSwitch.h`，并将 `SHIP_YAW_HOLD_KP_Q10` 从 `31` 下调到 `12`，保持 P-only，先把差速自稳从“偏猛”收回到可调区间。
- **[日志节流]** `SHIP_YAW_HOLD_LOG_PERIOD_MS` 继续保留，用于限制 `[MOT]` 诊断输出频率，避免上位机被高频 motor 日志刷屏。
- **[FeatureSwitch 中文化]** `User/FeatureSwitch.h` 重写为中文 Doxygen 风格，逐组补齐核心模块、轮询、IMU、无线、协议、配对、yaw 自稳和旧兼容参数说明，避免后续再出现“只写了一个注释”的不完整配置说明。

---

## [2026-05-13] - v1.7.62 device 文档一致性复核

### 新增功能
- **[AutoDrive README]** 新增 `Code_boweny/Device/AutoDrive/README.md`，把自动返航、点位巡航、链路保活、返航触发、航向来源和 flash 存储入口按当前根目录代码补齐，不再让 `AutoDrive` 成为“代码已接入但设备文档缺失”的盲区。

### Bug 修复
- **[Device 文档缺口]** `Code_boweny/Device/AutoDrive/` 已被 `ship_protocol.c` 实际初始化和轮询，但目录下没有 README，导致“每个 device 是否都能独立查行为”这件事不成立。修复：补齐独立 README，并明确它由 `ShipProtocol_RunScheduler()` 内部接管。
- **[头文件口径过期]** `QMI8658.h`、`QMC6309.h`、`Motor.h` 顶部说明仍残留“IMU 关闭”“磁力计不进主链”“正负速度交换 P/N 极性”这类旧阶段表述，已经与当前工程不一致。修复：统一改成当前真实运行口径。
- **[无线文档隐藏行为未写清]** `Code_boweny/Device/WIRELESS/README.md` 之前没有明确写出 `AutoDrive` 实际由协议调度器初始化/轮询，也没有把手动 yaw-hold 的完整门控条件写全。修复：补齐 `AutoDrive_Init/LinkAliveTick/Poll` 真实接入位置，以及 `|steering| <= gate + 前进油门` 的当前门控条件。
- **[总览文档遗漏 AutoDrive 主链位置]** `doc/project_doc/total.md` 之前虽然写了 `0x13/0x14/0x15` 和返航存储，但没有明确 `AutoDrive` 当前不在主循环裸入口、而是在 `ShipProtocol_RunScheduler()` 内运行。修复：补齐主循环说明和当前结论。

### 优化改进
- **[文档边界统一]** 当前根目录 `Device/*/README.md` 现在全部存在，并统一按“根目录真实代码”描述，不再混用历史联调档口径。
- **[维护入口更清晰]** `total.md` 与各 device README 现在都能直接指向 `User/FeatureSwitch.h`、`ship_protocol.c`、`autodrive.c` 这类真实配置或行为来源，后续维护时更容易顺着代码落点核对。

### 变更记录
- **[Device]** 新增 `Code_boweny/Device/AutoDrive/README.md`。
- **[Headers]** 更新 `Code_boweny/Device/QMI8658/QMI8658.h`、`Code_boweny/Device/QMC6309/QMC6309.h`、`Code_boweny/Device/Motor/Motor.h`、`Code_boweny/Device/WIRELESS/ship_protocol.h`、`Code_boweny/Device/WIRELESS/ship_protocol.c` 的顶部说明。
- **[README]** 更新 `Code_boweny/Device/QMC6309/README.md`、`Code_boweny/Device/Motor/README.md`、`Code_boweny/Device/WIRELESS/README.md` 的当前行为描述。
- **[Project Doc]** 将 `doc/project_doc/total.md` 和 `date.md` 顶部版本同步为 `v1.7.62`。

### 开发者备注
- 这次只修正文档和注释口径，不改算法、不改功能宏、不改控制参数。
- `date.md` 历史条目保留当时语境；当前一致性只要求文件头版本、最新条目和 `total.md` 与当前工作区真实状态一致。

---

## [2026-05-13] - v1.7.61 手动自稳开启与PWM输出口径同步

### Bug 修复
- **[总览文档过期]** `doc/project_doc/total.md` 仍记录 `ENABLE_GPS_MODULE=0`、旧 yaw-hold 参数和旧运行档，已经与当前 `User/FeatureSwitch.h` 不一致。修复：按当前工作区真实开关重写总览，明确 `Wireless/GPS/MAG/IMU/AHRS` 均启用。
- **[协议核对结论缺失]** 旧文档没有集中记录遥控器串口命令是否逐字节对齐，容易继续误判 `0x12` 半球字段和点位 payload。修复：补齐旧版帧格式、命令号、GPS 15 字节回传、点位 10 字节格式和 `0x15` 保存格式的核对结论。
- **[手动自稳门控未同步]** 之前文档仍把 `SHIP_YAW_HOLD_MANUAL_ENABLE=1` 描述成“泛泛的小转向叠加 yaw-hold”，但当前代码已经把门限收敛到 `SHIP_YAW_HOLD_STEER_GATE=10U`，并且直接下发左右 PWM。修复：把“直线自稳触发条件”和“PWM 输出口径”同步写入总览与变更日志。

### 优化改进
- **[0x12 兼容口径]** 明确 `0x12` 半球字段固定 `E/W` 是对齐老版 `nmea41_Get_EW()` / `nmea41_Get_NS()` 的兼容行为，不是改成了新协议。
- **[存储口径]** 明确当前无外置 EEPROM，自动返航配置写入 STC flash/EEPROM 区 `0x0001F800`，由 `AutoDriveCfg_Save()` 管理。
- **[风险边界]** 明确当前工作区 `0x11` wire payload 与旧版一致，但应用层已开启 `SHIP_YAW_HOLD_MANUAL_ENABLE=1`，手动控制会在 `|steering| <= 10` 且前进油门成立时叠加 yaw-hold，并直接输出左右 PWM，不等同旧版纯开环。

### 变更记录
- **[total.md]** 重写当前结论、功能开关、启动顺序、主循环、旧遥控器协议核对表、存储方式和已知差异，并同步当前手动自稳门控口径。
- **[date.md]** 新增本条日志，并将文件头版本更新为 `v1.7.61`。

### 开发者备注
- 旧遥控器能否接收命令，首先看 wire 格式；当前 `0x0F~0x15` 的帧格式和字段顺序已对齐。
- 如果目标是“应用层行为严格等同老版本”，当前 `SHIP_YAW_HOLD_MANUAL_ENABLE=1` 需要在发布前处理，否则 `0x11` 控船手感和旧版纯开环存在差异。
- 当前手动自稳门控是 `|steering| <= 10` 且油门为前进时进入；其余情况仍回到开环差速。
- `0x15` 正常 11 字节帧兼容旧遥控器；短包只保存开关、不更新返航点，是当前实现的防御性差异。

---

## [2026-05-11] - v1.7.58 四元数AHRS重构

### Bug 修复
- **[欧拉角姿态主链退役]** 旧版 AHRS 采用 `roll/pitch/yaw` 分别积分，再用加速度和 `atan2(my,mx)` 做慢修正。该结构在大姿态和有倾角的 yaw 修正下容易出现耦合误差与单向漂移。修复：将 `Code_boweny/Function/AHRS` 重构为 Mahony 风格四元数传播 + accel/mag 误差反馈。
- **[磁修正缺少倾斜补偿]** 旧版磁力计只按水平面 `atan2(my,mx)` 修 yaw，板子存在明显 `roll/pitch` 时，航向会被错误拉偏。修复：磁力计改为“低通后的三轴向量 + 四元数姿态下的倾斜补偿航向误差反馈”。
- **[Keil C51 链接溢出]** 四元数版引入浮点状态后，AHRS 上下文直接放在默认区会触发 `EDATA` 溢出。修复：将 AHRS 上下文迁到 `xdata`，保持工程可继续完整链接。

### 优化改进
- **[接口兼容]** `AHRS_Reset`、`AHRS_UpdateRaw6Axis`、`AHRS_UpdateRawMag`、`AHRS_GetState` 和 `AHRS_State_t` 均保持不变，主循环和上位机日志正则无需改动。
- **[日志兼容]** 继续保留 `g=`、`ys=`、`yr=`、`f=`、`mv=0`、`me=0` 语义，不污染现有 `[AHRS]` 主格式。
- **[Yaw策略维持不变]** 仍按“短期信陀螺、长期用磁力计慢修正”的策略运行，GPS 不进入本轮主链。

### 变更记录
- **[AHRS.h/.c]** 内核从欧拉角互补滤波切换为四元数 Mahony 融合；保留轴映射、gyro bias 学习、模长门控与 flags 语义。
- **[README / total / ahrs_report]** 根目录和镜像文档统一改写为当前四元数实现口径。
- **[Build]** `RVMDK/STC32G-LIB.uvproj` 已重新编译通过。

### 当前效果
- 当前根目录工程已切到“四元数传播 + accel/mag 修正”的 AHRS 主链。
- Keil 构建通过，当前体积为 `edata+hdata=3967`、`xdata=6724`、`code=47175`。
- `MainLoop.c`、`doc/tools/ship_log_viewer.html` 和现有 `[AHRS]` 日志显示链路无需同步改格式。

### 开发者备注
- 本轮没有加入磁力计硬铁/软铁校准，所以绝对 yaw 仍属于“工程可用、未最终标定”状态。
- 下一阶段若要继续提升绝对航向，应先做磁力计校准，再考虑把 GPS 作为低频长期参考接入。

## [2026-05-12] - v1.7.59 船体 yaw 自稳参数收口

### 优化改进
- **[Yaw 自稳参数收口]** `SHIP_YAW_HOLD_*` 统一保留在 `User/FeatureSwitch.h`，并将 `SHIP_YAW_HOLD_KP_Q10` 从 `31` 下调到 `12`，维持 P-only 控制，先把差速自稳从“偏猛”收回到可调区间。
- **[日志节流说明]** `SHIP_YAW_HOLD_LOG_PERIOD_MS` 保持 `1000UL`，用于限制 `[MOT]` 诊断输出频率，避免上位机被高频 motor 日志刷屏。

### 变更记录
- **[FeatureSwitch]** 为 yaw-hold 参数增加中文 Doxygen 注释，说明 `100ms` 控制周期、`±60` 输出限幅和 `Kp` 的当前建议值。
- **[total.md]** 同步重写当前项目总览，收口为当前真实运行档与 yaw 语义。

### 开发者备注
- 本轮只收口比例项，不引入积分和微分，避免在船体静态自稳阶段继续放大过冲。
- 如果实船仍偏激，再按实测继续下调 `Kp`，优先稳住响应而不是追求最大修正力度。

## [2026-05-11] - v1.7.57 IMU根因确认与AHRS融合链恢复

### Bug 修复
- **[IMU根因确认]** 本轮确认此前 IMU 异常的直接根因是 QMI8658 器件本体损坏，而不是 I2C 驱动路径或 AHRS 算法根因。修复：文档口径统一改为“IMU 已恢复，当前进入融合链恢复阶段”，不再继续把 AHRS 主链维持在 `basic raw` 排障档。
- **[主循环仍停在基础读数档]** 根目录当前代码此前只执行 `QMI8658_Service() + basic raw log`，AHRS 轮询入口未接回。修复：恢复 `QMI8658_Service() -> AHRS_UpdateRaw6Axis() -> AHRS_UpdateRawMag()` 主链，并保留 `Task_Pro_Handler_Callback()` 与 MAG 独立观测。
- **[AHRS参数与当前bring-up配置不一致]** 当前 QMI8658 仍使用 `CTRL2/CTRL3=0x07/0x07` 这一套可稳定出数的 bring-up 配置，但根目录 `AHRS.h` 仍保留 `9ms / 256 LSB/(deg/s)` 的旧参数。修复：对齐为 `AHRS_IMU_PERIOD_MS=17ms`、`AHRS_GYRO_LSB_PER_DPS=2048`。

### 优化改进
- **[Yaw策略收口]** 当前 yaw 策略明确为“短期信陀螺积分，长期用磁力计慢修正”，GPS 只保留为下一阶段低频长期参考，不在本轮主链启用。
- **[磁修正开关集中]** 新增/收口 `AHRS_MAG_ENABLE` 到 `User/FeatureSwitch.h`，默认 `1`；若现场磁环境差，可直接切回 gyro-only 诊断而不改 Driver 或业务层。
- **[AHRS日志辨识度]** AHRS 日志继续保留 `g=`、`ys=`、`yr=` 调试字段，并在磁力计未参与或无效时追加 `me=0` / `mv=0`，便于区分“未启用磁修正”和“磁数据当前无效”。

### 变更记录
- **[FeatureSwitch]** `ENABLE_IMU_AHRS_POLL=1`，`ENABLE_IMU_BASIC_POLL=0`，`AHRS_MAG_ENABLE=1`，保持 `ENABLE_MAG_MODULE=1`、`ENABLE_GPS_MODULE=0`。
- **[System_init]** 传感器初始化链路恢复 `AHRS_Reset()`，随后再初始化 `QMC6309/QMI8658`。
- **[MainLoop]** 当前主循环恢复 AHRS 轮询，使用 `Task_GetTickMs()` 固定 `dt_ms`，每 `17ms` 读取一次 IMU，每 `100ms` 读取一次滤波地磁。
- **[文档同步]** 同步更新根目录 `doc/project_doc/total.md` 与镜像目录 `black_-pearl-master/doc/project_doc/total.md` / `date.md`。

### 当前效果
- 固件已从“IMU basic raw 排障档”切回“IMU + MAG + AHRS”运行档。
- 启动后只要 `QMI8658_Init()` ready，主循环就会进入姿态融合并输出 AHRS 日志，不再停留在纯 `basic raw` 模式。
- GPS 长期航向修正本轮未接入，不会引入新的串口与业务链路耦合。

### 开发者备注
- 若现场磁场明显受电机、船体或布线干扰，可先把 `AHRS_MAG_ENABLE` 改为 `0` 做 gyro-only 复核，再决定是否保留磁修正。
- 下一阶段若要把 GPS 纳入长期航向参考，应在保持当前 AHRS 主链稳定的前提下单独增加门控与低频融合策略，不要与本轮恢复工作混做。

---

## [2026-05-11] - v1.7.56 传感器 I2C 路径复核与文档纠偏

### Bug 修复
- **[文档失真]** 根目录 `doc/project_doc/total.md` 与当前实际工程状态脱节，仍混入旧阶段描述，容易误导排障方向。修复：重写总览文档，明确当前测试档、真实启动路径、真实 I2C 调用链和本轮排障结论。
- **[硬 I2C 前置判定过严]** 当前传感器链路使用 STC 硬件 I2C 时，早先把 `P14/P15` GPIO 电平直接纳入“总线空闲”前置拦截，导致日志出现 `bus not idle -> recover failed -> bus idle check failed before probe`，在真正发起 I2C 事务前就可能被软件自己挡住。修复：硬 I2C 分支的总线状态判定收紧为优先看控制器 busy 状态，`P14/P15` 电平判断只保留给软 I2C 分支。

### 优化改进
- **[I2C 路径确认]** 复核当前根目录工程后确认：
  - `QMC6309_port.c` 不维护独立 I2C 实现，而是直接复用 `QMI8658Port_*`。
  - `QMI8658_port.c` 在 `QMI8658_I2C_USE_SOFT == 0` 时，实际调用 `Driver/src/STC32G_I2C.c` 的 `I2C_WriteNbyte()` / `I2C_ReadNbyte()`。
  - 当前传感器硬 I2C 确实在走 STC 底层官方 API，不是手搓事务，也不是旧文档描述的其他路径。
- **[测试档收口]** 当前特征开关已收敛到传感器基础排障档：
  - `ENABLE_MAG_MODULE=1`
  - `ENABLE_IMU_MODULE=1`
  - `ENABLE_MAG_STANDALONE_POLL=1`
  - `ENABLE_IMU_BASIC_POLL=1`
  - `ENABLE_IMU_AHRS_POLL=0`
  - `QMI8658_I2C_USE_SOFT=0`
  - `QMI8658_DIAG_ENABLE=1`

### 变更记录
- **total.md**: 重写根目录工程总览，改为与当前代码一致的启动顺序、主循环职责、I2C 路径和排障结论。
- **date.md**: 新增本条日志，记录 2026-05-11 传感器 I2C 路径复核与排障结论。

### 当前效果
- 日志现已不再停在最早的 `bus idle check failed before probe` 阶段。
- 代码路径已经推进到 `QMC6309` / `QMI8658` 的真实地址探测阶段，说明“软件自己先把总线挡死”这一层问题已经被压缩。
- 当前仍未拿到完整 probe 后续 ACK / ID 结果，因此还不能把原因继续细化到“器件无应答”还是“读寄存器异常”。

### 当前判断
- 现阶段最可信的根因不是“没走 STC 底层 API”，因为实际已经在走。
- 更合理的原因是：当前根目录工程相对之前可工作的版本发生了传感器 I2C 实现漂移，中间叠加了过严的硬 I2C 前置判定，导致一度在 probe 前自锁。
- 在放宽前置判定后，问题已收敛到器件地址探测与 ACK/ID 读取阶段；是否还有更深层的硬件或时序问题，需要继续看 probe 后续日志。
- [模块名] 改进描述

### 变更记录
- [模块名] 原有行为 → 新行为

### 开发者备注
- 任何需要特别注意的事项
```

---

## 变更日志

---

## [2026-05-07] - v1.7.55 半桥电机 PWM 中点模型修正

### Bug 修复
- **[半桥 PWM 控制模型错误]** 之前 `Motor` 层把速度 0 直接映射为 `0%` 占空比，并在停机/换向时撤销 `PWMA CH3/CH4` 输出使能，导致 `P2.4~P2.7` 掉回 GPIO 上拉态，现场表现为 `MLA/MLB` 或 `MRA/MRB` 一起顶到 `3.3V`。修复：`Motor_Stop()` 改为回 `50%` 中点，占空比始终围绕中点偏移，PWM 不再退回 GPIO 上拉态。
- **[CH3/CH4 极性与板级接法不匹配]** 复核原理图后确认左/右电机的 `PWMxP/PWMxN` 分别接到半桥驱动的 `HIN/LIN#`，两侧并非完全对称。修复：`Motor_LeftSetForwardPolarity()` 与 `Motor_RightSetForwardPolarity()` 改为按板级固定极性映射，不再按“速度正负切 P/N 极性”的旧思路处理。

### 变更记录
- **[Motor]** `Code_boweny/Device/MOTOR/Motor.c` 将速度映射改为 `-1000~+1000 -> 0~1000`，其中 `500` 为静止中点；初始化默认 duty 设为 `50%`，停机同样回到 `50%`。
- **[GPIO/PWM 接管]** `P2.4~P2.7` 维持在 PWM 接管路径下工作，并关闭内部上拉，避免停机时被 GPIO 上拉错误拉高。
- **[文档同步]** 更新根 `README.md`、`Code_boweny/Device/MOTOR/README.md`、`doc/project_doc/total.md`，统一说明当前真实电机模型。

### 当前效果
- 油门值变化时，PWM 已不再按“0% 关断、正负切极性”输出，而是按半桥驱动常见的“50% 中点 + 差分偏移”方式工作。
- 本轮未改 `Driver/src/STC32G_PWM.c`，修复全部集中在 `Motor` 层，便于继续现场联调和回退。

---

## [2026-05-07] - v1.7.54 GPS 0x12 老版格式复核修正

### Bug 修复
- **[GPS 坐标格式不兼容]** 复核老版 `ship_Gps_V2.1_20260406-115200/App/Wireless/wirelessProtocal.c::RF_Send_Gps_Data()` 与 `App/Gps/nmea41_protocal.c` 后确认，遥控器端期望的不是 `deg1e7`，而是 RMC 原始 NMEA 的 `dddmm.mmmm/ddmm.mmmm` 拆分字段。修复：`0x12` 优先使用 RMC 原始字符串拆出的 `lon1/lon2/lat1/lat2`。
- **[GPS 经纬度定点换算错误]** `GPS_ParseCoordinate1e7()` 中分钟转度的小数比例多乘了 10，导致 `gps state` 的 `deg1e7` 观测值偏大。修复：改为 `minutes_scaled1e4 * 100 / 6`。
- **[GPS 航向角单位不一致]** 老版 `GPS_DK2_RMC` 下 `nmea41_Get_Angel()` 返回整数度。修复：`0x12` 航向角回传整数度，不再发送 `deg * 100`。
- **[GPS 卫星数来源不一致]** 老版 `nmea41_Get_GPS_Num()` 优先使用完整 GSA PRN 计数，GSA 不完整或为 0 才回退 GGA 卫星数，并限制最大 24。修复：新增老版语义的 GSA PRN 完整计数并用于 `0x12` 的 `sat` 字段。

### 优化改进
- **[GPS 现场核对日志]** `ShipProtocol_SendGpsOnce()` 增加 `gps sat source` 与 `gps payload bytes` 日志，现场可直接核对遥控器收到的 15 字节旧格式 payload。
- **[上位机 GPS 解析]** 上位机新增 `gps payload bytes` 解析，按旧格式展示遥控器端应看到的坐标字段。

### 变更记录
- **[0x12 payload]** 当前 15 字节顺序固定为 `sat, angle_u16, E, lon1_u16, lon2_u16, W, lat1_u16, lat2_u16, power, auto`。
- **[老版常量方向字节]** `0x12` payload 中方向字节继续按老版 `nmea41_Get_EW()='E'`、`nmea41_Get_NS()='W'` 保持兼容；真实半球只用于 `gps state` 日志。

### 开发者备注
- 如果现场 `fix=0 legacy=0`，说明当前 RMC 无有效定位，`0x12` 坐标为 0 是预期表现；此时优先排查 GPS 天线、室外环境、UART2 数据与波特率。
- 本次再次核对老版 `GPS_DK2_RMC` 编译分支和 `RF_Send_Gps_Data()` 15 字节顺序，未新增协议字段。

---

## [2026-05-07] - v1.7.53 串口日志上位机与 GPS 正式回传收口

### 新增功能
- **[Wireless/GPS 日志]** `ShipProtocol_SendGpsOnce()` 新增正式 `gps state` 日志，输出 `fix / sat / lon / lat / angle / power / seq`，用于上位机稳定解析当前 GPS 回传状态。
- **[上位机网页]** 新增并收口 `doc/tools/ship_log_viewer.html`，支持串口选择、波特率配置、在线解析、离线日志粘贴解析，以及 GPS 摘要展示。
- **[根目录入口]** 新增根目录 `index.html`、`start_ship_log_viewer.bat`、`start_ship_log_viewer.ps1`，适配当前文件位置变化，Windows 下可直接启动本地服务并打开页面。

### Bug 修复
- **[GPS 在上位机不可见]** 之前 `0x12` 只有 `sat/angle/power` 简略日志，上位机无法正式看到经纬度与定位有效性。修复：补齐正式 GPS 状态日志并在网页中增加经纬度、定位有效、更新序号显示。
- **[工具路径失效]** 之前工具说明和启动方式仍依赖旧文件位置，根目录也没有稳定入口。修复：统一改为根目录跳转入口和 Python 启动脚本。
- **[遥控状态语义混淆]** 之前上位机容易把“已配对”误看成“遥控仍在线”。当前口径：页面“遥控在线”按完整 `AA ... BB` 头尾包活动刷新；有效 `0x11` 只作为控制输入和固件电机安全保活依据。

### 优化改进
- **[事件容量]** 时间线和原始日志缓存上限都收口到 `20000` 条，适合现场长时间联调。
- **[Windows 兼容]** 启动脚本不再依赖错误路径或乱码命令行，优先尝试 `py -3`，失败再回退 `python`。

### 变更记录
- **[doc/tools]** 工具入口从“只保留页面文件”调整为“根目录启动脚本 + 根目录跳转页 + doc/tools 实际页面”。
- **[GPS 观测链路]** 当前 GPS 是否有定位、经纬度和航向角可以通过 `gps state` 日志正式观察，无需再只靠 `0x12` payload 简略日志猜测。

### 开发者备注
- 当前网页能展示 GPS 解析结果，但前提仍是固件确实输出了 GPS 相关日志；如果现场 GPS 模块未锁定或未接入，页面会明确显示 `fix=0` 或仍停留在等待状态。

---

## [2026-05-07] - v1.7.52 无线业务复核修正

### Bug 修复
- **[`0x12` 回传后 RX 断续]** 当前 `ShipProtocol_SendGpsOnce()` 发完状态包后只把 `work_rx_configured` 置 0，依赖下一轮调度再恢复工作 RX。修复：发完 `0x12` 后立即重开工作信道 RX，避免合法帧后出现额外的接收空窗。
- **[遥控失联恢复节奏偏弱]** 当前代码只有 `SHIP_THROTTLE_TIMEOUT_MS` 超时停机，没有把老版“长时间收不到遥控帧后重新整理接收链路”的节奏带回来。修复：增加开环安全版恢复逻辑，`0x11` 长时间静默后重新打开工作 RX，但不恢复老版自动驾驶、巡航或软复位副作用。
- **[左右转向极性映射错误]** 复核老版 `pwm.h` 的 `MOTOR_POSITIVE=TRUE`、`MOTOR_LEFTRIGHT=FALSE` 与 `DcMotor_Direction_Set()` 后确认，当前新 `Motor` 映射中的左/右转正负号写反。修复：`SHIP_MOTION_LEFT/RIGHT` 的双电机符号已按老版方向语义纠正。

### 变更记录
- **[Wireless Scheduler]** 当前在保持 `0x10/0x0F/0x11/0x12` 老版协议不变的前提下，补回了更贴近老版的工作 RX 恢复节奏。

### 开发者备注
- `A` 键船灯引脚仍未确认，当前不能擅自把老版 `P31/P22` 或现仓 `P3.6` 当成同一功能脚。
- `C/D` 已按当前 v1.1 板级真实定义固定为“通过现有电机 PWM 引脚直接让船产生 150ms 前冲/后退动作”，不再追求老版 `P0.2/P0.3` 独立脉冲实现。

---

## [2026-05-07] - v1.7.51 今晚开环控船联调版

### 新增功能
- **[Wireless 手动链路]** `ship_protocol.c` 恢复老版 `0x11` 手动开环判定，重新按前进、后退、左转、右转、停止五种语义驱动 `Motor`。
- **[按键恢复]** 恢复 `C` 短促前冲、`D` 短促后退、`B` no-op、`E` 兼容入口但禁用自动驾驶副作用；`A` 保留灯控入口日志，等待板级引脚确认。
- **[状态回传]** 保持老版“任意合法协议帧后立刻回发一次 `0x12`”的节奏，继续使用老版 15 字节 payload 格式。

### Bug 修复
- **[主循环目标偏离]** 之前代码和文档仍残留 IMU/AHRS 测试档路径，与今晚“先把船开环跑起来”的目标不一致。修复：切换功能开关到无线/GPS/磁力计开启、IMU/AHRS 关闭的联调档，并从 `MainLoop.c` 和 `System_init.c` 移除 IMU/AHRS 主路径调用。
- **[文档失真]** `ship_protocol.h`、`WIRELESS/README.md`、`total.md` 仍描述“只打印油门、不输出 PWM”的旧测试态。修复：统一更新为当前真实开环联调行为，并补记 A 键引脚未确认的限制。
- **[头文件注释缺失/乱码]** `Code_boweny` 下多份公开头文件缺少中文 Doxygen 文件头或存在编码乱码。修复：补齐 `GPS/QMC6309/QMC6309_port/QMI8658/QMI8658_port` 等头文件的中文 Doxygen 风格说明，并纠正 `ship_protocol.h` 的旧描述。

### 优化改进
- **[无线业务边界]** 明确 `ship_protocol.h` 是旧版 `Wireless_other/wirelessProtocal.c` 的移植兼容层，不是新协议设计层；后续修改必须优先对齐老版空口和业务节奏。
- **[总览文档]** 重写 `total.md`，聚焦当前真实启动顺序、主循环、功能开关、无线主链路和今晚验收目标，移除与当前固件无关的陈旧测试档描述。
- **[模块说明]** 重写 `WIRELESS/README.md`，明确当前配对节奏、`0x11` 手动控制、`0x12` 回传、按键语义和已知未决项。

### 变更记录
- **[FeatureSwitch]** 当前联调档固定为 `ENABLE_WIRELESS_MODULE=1`、`ENABLE_GPS_MODULE=1`、`ENABLE_MAG_MODULE=1`、`ENABLE_IMU_MODULE=0`、`SHIP_THROTTLE_PWM_ENABLE=1`。
- **[System_init]** 传感器初始化只保留 `QMC6309_Init()`；`QMI8658_Init()` 与相关 AHRS 路径不再进入今晚主链路。
- **[MainLoop]** 当前主循环保持 `GPS_Poll()`、`Wireless_Poll()`、`ShipProtocol_RunScheduler()`、`Wireless_SearchSignalPoll()`、`MAG_StandalonePoll()`、`Task_Pro_Handler_Callback()`。

### 开发者备注
- `A` 键船灯引脚仍未确认，当前不能擅自把老版 `P31/P22` 或现仓 `P3.6` 当成同一功能脚。
- 电机正反极性仍需以明早实船效果最终确认，仓内没有足够板级证据证明“前进/左转”对应的新 `Motor` 正负号方向已经完全正确。

---

## [2026-05-07] - v1.7.50 QMI8658 5.1 凌晨老版路径复测

### 变更记录
- **[QMI8658]** 将本轮测试切到 `QMI8658_INIT_NONBLOCKING=0`，初始化重新走 5.1 凌晨老版阻塞 bring-up 流程。
- **[寄存器顺序]** 按老版顺序执行 `CTRL7=0x00 -> CTRL1 -> CTRL2 -> CTRL3 -> CTRL5 -> CTRL7=0x03 -> 30ms -> 读回 -> STATUS0`。
- **[寄存器数值]** 恢复 `CTRL2=0x07`、`CTRL3=0x07`、`CTRL5=0x11`，并保持 `soft_reset=0`、`CLEAR_DATAPATH=0`，绕开 `RESET` 与 `CTRL9` 干扰项。
- **[IIC 速率]** 共享传感器 IIC 回到 `SENSOR_I2C_SPEED_CFG=58`（约 100kHz），对齐 5.1 老版 QMC6309/QMI8658 传感器总线配置。
- **[日志窗口]** 失败时恢复连续 10 次 `STATUSINT/STATUS0/timestamp/temp/raw` 数据窗口打印，用于确认是否仍然是 `WHO_AM_I` 正常但数据域全零。

### 当前效果
- 当前固件用于验证“老版库函数整包 IIC + 老版寄存器顺序/数值”是否能恢复 QMI8658 出数。
- QMC6309 仍然启用并走同一条共享 IIC 后端，可同步观察磁力计是否受本轮回退影响。

---

## [2026-05-07] - v1.7.49 传感器硬件 IIC 后端统一切回库函数

### 优化改进
- **[Sensor IIC]** `QMI8658_port` 的硬件 IIC 路径改为统一调用 `I2C_WriteNbyte()` 和 `I2C_ReadNbyte()`，不再手动拼 `Start/SendData/RecvACK/RecvData/Stop` 事务。
- **[QMC6309]** 由于磁力计复用 `QMI8658_port` 后端，当前 `bus=hard` 时也会自动统一到同一套库函数整包读写路径。

---

## [2026-05-07] - v1.7.48 QMI8658 关闭 soft reset 试验

### 优化改进
- **[QMI8658]** 将 `QMI8658_SOFT_RESET_ENABLE` 关闭，当前初始化阶段不再写 `RESET=0xB0`。
- **[QMI8658]** 本轮验证目标是进一步贴近 `0f33bd2` 老版本的最小 bring-up 路径，观察在绕开 `CTRL9` 后、同时关闭 soft reset 时，IMU 是否恢复 `STATUS0 / timestamp / raw` 更新。

---

## [2026-05-07] - v1.7.47 QMI8658 绕开 CTRL9 命令链试验

### 优化改进
- **[QMI8658]** 将 `QMI8658_CLEAR_DATAPATH_ENABLE` 关闭，当前初始化阶段不再执行 `CTRL6/8/FIFO/CTRL9` 数据通路清理。
- **[QMI8658]** 本轮验证目标是直接绕开 `CTRL9` 命令寄存器和未握手的 FIFO reset 流程，观察 IMU 是否恢复 `STATUS0 / timestamp / raw` 更新。

---

## [2026-05-07] - v1.7.46 QMI8658 延长 READY 观察窗口并收敛重试日志

### 优化改进
- **[QMI8658]** 将 `QMI8658_READY_TIMEOUT_MS` 从 `200ms` 调整为 `1000ms`，在宣布 ready timeout 之前持续更久地观察 `STATUS0/timestamp/raw` 是否开始变化。
- **[QMI8658]** 调整状态机重试返回值：当驱动只是安排下一轮 retry 而非真正进入 `FAILED` 时，`QMI8658_Service()` 不再返回错误，避免主循环把正常重试误报成 `service failed`。

---

## [2026-05-07] - v1.7.45 QMI8658 最大量程与关闭低通试验

### 优化改进
- **[QMI8658]** 将 `CTRL2` 从 `0x07` 调整为 `0x37`，把加速度计切到 `±16g @ 58.75Hz`。
- **[QMI8658]** 将 `CTRL3` 从 `0x07` 调整为 `0x77`，把陀螺仪切到 `±2048dps @ 58.75Hz`。
- **[QMI8658]** 将 `CTRL5` 从 `0x11` 调整为 `0x00`，关闭 Accel/Gyro 低通滤波，用于排除滤波链路对出数的影响。

---

## [2026-05-07] - v1.7.44 QMI8658 按手册时序调整启动等待

### 优化改进
- **[QMI8658]** 依据 `QMI8658C datasheet rev 0.9` 的 System Turn On Time 说明，将 `QMI8658_PWR_UP_DELAY_MS` 从 `50ms` 调整为 `150ms`。
- **[QMI8658]** 依据同一手册中 Software Reset / Power-On Default 的启动时序要求，将 `QMI8658_RESET_DELAY_MS` 从 `10ms` 调整为 `150ms`，避免 reset 后过早继续配置寄存器。

---

## [2026-05-07] - v1.7.43 QMI8658 配置寄存器值回退试验

### 优化改进
- **[QMI8658]** 将 `CTRL2/CTRL3` 初始化值从当前 `0x16/0x36` 回退到 `0x07/0x07`，与 `0f33bd2` 老版本保持一致，用于配合老版时序继续验证 IMU 是否能恢复出数。

---

## [2026-05-07] - v1.7.42 QMI8658 寄存器配置时序回退试验

### 优化改进
- **[QMI8658]** 将状态机中的寄存器配置顺序调整得更贴近 `0f33bd2` 老版本：先写 `CTRL7=0x00`，再写 `CTRL1/CTRL2/CTRL3/CTRL5`，随后写 `CTRL7=0x03` 使能，等待 `ENABLE_DELAY` 后才做整组读回验证。
- **[QMI8658]** `cfg readback` 现在会额外检查 `CTRL7`，便于直接确认“老版时序下使能位是否保持正确”。

---

## [2026-05-07] - v1.7.41 传感器共享 IIC 速率上调到 400k 验证

### 优化改进
- **[Sensor IIC]** 将 `User/FeatureSwitch.h` 中 `SENSOR_I2C_SPEED_CFG` 从 `28` 进一步调整为 `13`，把 `P1.4/P1.5` 共享总线从约 `200kHz` 提升到约 `400kHz`，用于继续验证 QMI8658 在高速 IIC 下的读数表现。

---

## [2026-05-07] - v1.7.40 传感器共享 IIC 速率上调验证

### 优化改进
- **[Sensor IIC]** 将 `User/FeatureSwitch.h` 中 `SENSOR_I2C_SPEED_CFG` 从 `58` 调整为 `28`，把 `P1.4/P1.5` 共享总线从约 `100kHz` 提升到约 `200kHz`，用于验证 QMI8658 在更高速率下的读数表现。

---

## [2026-05-07] - v1.7.39 QMC6309 IIC 后端与 QMI8658 对齐

### 优化改进
- **[QMC6309 端口层]** 新增 `QMC6309_port.c/.h`，磁力计不再直接依赖 `Start/SendData/RecvData` 等硬件 IIC 原语，改为通过 port 层访问总线。
- **[共享后端]** `QMC6309` 默认复用 `QMI8658_port` 的软/硬 IIC 后端与总线恢复逻辑，`P1.4/P1.5` 共享总线切换行为保持一致。
- **[编码清理]** `QMC6309.h/.c` 重写为干净 UTF-8 版本，并统一包含 `Config.h`，便于后续继续维护和补丁。

---

## [2026-05-06] - v1.7.38 QMI8658 软/硬IIC与非阻塞状态机

### 新增功能
- **[QMI8658 端口层]** 新增 `QMI8658_port.c/.h`，把 IMU 总线访问从驱动主体中拆出，统一收口 `P1.4/P1.5` 的板级口线、延时、总线恢复，以及软/硬 IIC 后端切换。
- **[双后端切换]** 在 `User/FeatureSwitch.h` 新增 `QMI8658_I2C_USE_SOFT`、`QMI8658_SOFT_I2C_DELAY_US`、`QMI8658_READY_MODE_STATUS0/STATUSINT`、`QMI8658_INIT_NONBLOCKING` 等宏，当前默认仍走硬件 IIC，软件 IIC 与其共用 `P1.4/P1.5`。
- **[状态机接口]** `QMI8658` 新增 `QMI8658_Service()`、`QMI8658_RequestReinit()`、`QMI8658_IsReady()`、`QMI8658_HasDataReady()`、`QMI8658_ClearDataReady()`，初始化与读数调度改为服务式推进。

### 优化改进
- **[初始化非阻塞]** 旧的 `System_init.c` 阻塞式 IMU 上电自检被移除，QMI8658 现改为 `BUS_PREPARE -> ID_PROBE -> QUIESCE -> SOFT_RESET -> CONFIG -> ENABLE -> READY_WAIT` 状态机，主循环持续推进，不再在启动阶段长时间卡住。
- **[ready标志轮询]** 当前板上没有 IMU 外部 INT 脚，因此“中断标志位轮询”按寄存器 ready 位实现；默认使用 `STATUS0.aDA/gDA` 作为主判据，`STATUSINT` 仅保留为可选实验分支。
- **[运行期恢复]** `MainLoop.c` 中 AHRS 采样链改为先跑 `QMI8658_Service()`，再按 `data_ready` 取数；若 `ReadAll()` 连续失败达到阈值，会主动请求 IMU 重初始化并重置 AHRS。

### 变更记录
- **[主循环读数模型]** `IMU_HighRatePoll()` 不再按固定周期盲读 IMU，而是消费 `QMI8658` 状态机置起的数据就绪标志，再把 6 轴数据送入 AHRS。
- **[兼容接口保留]** `QMI8658_ReadID/ReadAcc/ReadGyro/ReadAll/GetLastI2cError` 等旧接口继续保留；`QMI8658_Wait_AccReady/Wait_GyroReady` 退化为兼容性的阻塞包装，不再是主链路核心。

---

## [2026-05-06] - v1.7.32 IMU+MAG 融合测试配置

### 新增功能
- **[测试档切换]** 将 `User/FeatureSwitch.h` 切换为 `IMU + 磁力计` 融合测试专用配置：打开 `ENABLE_IMU_MODULE`、`ENABLE_MAG_MODULE`、`ENABLE_IMU_AHRS_POLL`，并开启 `AHRS_TEST_ONLY`。

### 变更记录
- **[关闭其余链路]** 关闭 `ENABLE_WIRELESS_MODULE`、`ENABLE_GPS_MODULE`、`ENABLE_SHIP_PROTOCOL_SCHED` 和 `SHIP_PROTOCOL_POLL_ENABLE`，确保当前主循环只保留传感器初始化、AHRS 融合和日志输出链路。
- **[测试目标]** 当前配置用于单独观察 QMI8658 + QMC6309 融合输出，不再同时带无线配对、GPS 解析或其它联调任务。

---

## [2026-05-06] - v1.7.31 Device 冗余清理首轮收口

### 优化改进
- **[GPS 归口]** 删除 `GPS.c` 中与 `User/System_init.c` 重复的 `UART2_SW` 与 P1.0/P1.1 管脚前置配置，保留 `GPS_Init()` 的 UART2 运行态初始化、NVIC 使能、状态清零和 FIFO 清空逻辑。
- **[MAG 归口]** 收缩 `QMC6309.c` 的 I2C 总线恢复流程，不再在模块内部重复整套 `I2C_SetSpeed()` / `I2C_WDTA_DIS()` 主控恢复性重配置，恢复后继续沿用 `System_init` 已经建立的 I2C 配置。
- **[IMU 清理]** 整理 `QMI8658.h` 中先定义再 `#undef` 覆盖的配置写法，收敛为单一宏定义；同时移除未被当前主链路使用的 `QMI8658_ProbeAddr()` 和未启用诊断分支对应的静态实现残留。

### 变更记录
- **[硬件配置边界]** 当前工程继续按 `System_init` 负责最终硬件配置、device 负责运行期读写与防御性恢复的边界推进，首轮已覆盖 GPS / QMC6309 / QMI8658 三条链路。
- **[业务保持]** 无线配对调度、GPS 状态更新、QMC6309 地址探测、QMI8658 `WHO_AM_I` 校验和稳定读数判定逻辑保持不变。

---

## [2026-05-06] - v1.7.30 无线联调配置继续收口

### 新增功能
- **[配置收口]** 继续把无线联调相关的编译期开关统一进 `User/FeatureSwitch.h`，包括 `PAIR_CHANNEL`、`SHIP_PAIR_SEED0~3`、`SHIP_PAIR_SYNC_WORD`、`SHIP_PAIR_SEND_TIMES`、`SHIP_WAIT_TICKS_DEFAULT`、`SHIP_THROTTLE_PWM_ENABLE`、`WIRELESS_FRONTEND_BYPASS_TEST`、`WIRELESS_RX_TRACE_ENABLE`、`WIRELESS_TX_TRACE_ENABLE` 等。
- **[SPI 切换]** 无线端口的软/硬件 SPI 切换统一改为 `WIRELESS_SPI_USE_SOFT`，同时把硬件 SPI 速率也收口到 `WIRELESS_HW_SPI_SPEED_CFG`。
- **[中文注释]** 将无线相关核心文件的文件头和关键联调注释统一改为中文，便于后续排查和调试。

### 变更记录
- **[无线链路]** `wireless.c`、`wireless_port.c`、`ship_protocol.c` 不再各自维护一套本地兜底宏，统一从 `User/FeatureSwitch.h` 读取配置。
- **[调试开关]** 保留旧宏兼容层，但新联调优先只改 `FeatureSwitch.h`，避免多处同步遗漏。

---

## [2026-05-06] - v1.7.29 Main入口解耦与功能开关集中

### 新增功能
- **[功能开关集中]** 新增 `User/FeatureSwitch.h`，统一承载 `AHRS_TEST_ONLY`、`WIRELESS_MINIMAL_TEST_ONLY`、`SHIP_THROTTLE_PWM_ENABLE`、`SHIP_PROTOCOL_POLL_ENABLE` 等编译期调试/模式开关，后续联调只需要改这一处。
- **[主循环模块化]** 新增 `User/MainLoop.h/.c`，把原先 `User/Main.c` 中的 `Wireless_MinimalTestUnit()`、`MAG_StandalonePoll()`、`IMU_HighRatePoll()` 以及主循环轮询流程收拢到独立运行时模块。

### 优化改进
- **[入口职责收敛]** `User/Main.c` 现在只保留 `SYS_Init()`、`MainLoop_Bootstrap()` 和 `MainLoop_RunOnce()` 调用，避免主入口继续膨胀到数百行。
- **[配置分层]** `User/Config.h` 现在只保留主时钟和公共包含，并通过 `#include "FeatureSwitch.h"` 引入功能开关，减少“基础配置”和“调试模式”混在一起的问题。
- **[工程同步]** `RVMDK/STC32G-LIB.uvproj` 已加入 `User/MainLoop.c`，保证 Keil 工程和源码目录结构一致。

### 变更记录
- **[文档同步]** 更新 `doc/project_doc/total.md`，把“开关位于 `User/Config.h`”修正为“开关集中在 `User/FeatureSwitch.h`”，并把主循环职责从 `User/Main.c` 调整为 `User/MainLoop.c`。
- **[历史说明保留]** 早期日志中关于 `Main.c` / `Config.h` 的记录反映的是当时状态，本次不回改旧条目内容，只在新版本和总览文档中标明当前结构。

### 开发者备注
- 本次主要是结构整理，不改变现有默认运行模式和无线业务逻辑。
- 当前默认调试入口请优先查看 `User/FeatureSwitch.h`，不要再把功能开关继续散落回 `Main.c`。

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

## [2026-05-06] - v1.7.37 QMI8658 复位前先静默传感器

### 变更记录
- **复位前先停传感器**: `QMI8658_Init()` 在软复位前新增 `CTRL7=0x00`，先强制关闭 Accel/Gyro，避免运行态下忽略 `RESET=0xB0`。
- **复位前先清命令/数据通路**: 软复位前先执行一次 `QMI8658_ClearDataPath()`，清理 `CTRL6/CTRL8/FIFO/CTRL9` 相关残留状态。
- **诊断日志补点**: 新增 `pre_reset` 寄存器快照，便于观察复位前芯片是否仍处于上一次运行态。

### 当前效果
- `QMI8658` 初始化更贴近“先静默、再复位、后配置、最后使能”的稳妥顺序。
- 若后续 `reset_state/status/timestamp/raw` 仍保持全零，则更能确认问题不只是 reset 时机，而是芯片状态域或硬件层异常。

---

## [2026-05-06] - v1.7.36 QMI8658 上电阻塞与复位时序调整

### 变更记录
- **上电等待缩短**: `QMI8658_PWR_UP_DELAY_MS` 从 `500ms` 收到 `50ms`，减少刚上电时的阻塞等待。
- **软复位改为主动执行**: 当前只要 `QMI8658_SOFT_RESET_ENABLE=1`，初始化阶段就固定写入 `RESET=0xB0`，不再依赖 `RESET_STATE` 是否为 `0x80` 再决定是否复位。
- **复位后等待收敛**: `QMI8658_RESET_DELAY_MS` 调整为 `200ms`，更贴近当前 bring-up 经验。
- **初始化顺序保持严格**: 复位等待完成后，先写入并读回 `CTRL1/2/3/5`，最后再写 `CTRL7` 使能 Accel/Gyro。

### 当前效果
- 启动阻塞时间明显缩短，同时保持 `reset -> wait 200ms -> config -> enable` 这条更稳妥的时序。
- 若后续仍出现 `STATUS0=0x00` 与原始数据全零，则更能聚焦到芯片状态域或硬件问题，而不是初始化时序过乱。

---

## [2026-05-06] - v1.7.35 QMI8658 配置重新对齐

### 变更记录
- **软复位恢复默认开启**: `QMI8658_SOFT_RESET_ENABLE` 改为由 `User/FeatureSwitch.h` 统一配置，当前测试档默认开启，避免 MCU 复位但 IMU 未掉电时残留旧状态机。
- **数据通路清理恢复**: `QMI8658_CLEAR_DATAPATH_ENABLE` 同样收口到 `User/FeatureSwitch.h`，当前默认执行 `CTRL6/CTRL8/FIFO/CTRL9` 清理。
- **量程与 ODR 对齐**: `QMI8658_CTRL2_INIT` 改回 `0x16`（ACC ±4G / 117Hz），`QMI8658_CTRL3_INIT` 改回 `0x36`（GYRO ±125/128dps / 117Hz）。
- **AHRS 同步修正**: `AHRS_IMU_PERIOD_MS` 恢复为 `9U`，`AHRS_GYRO_LSB_PER_DPS` 恢复为 `256L`，与新的 QMI8658 默认配置保持一致。
- **文档纠偏**: `Code_boweny/Device/QMI8658/README.md` 更新为当前真实默认配置，不再保留 `CTRL2/3=0x07/0x07`、`soft reset=0` 这套旧 bring-up 描述。

### 当前效果
- 底层 QMI8658 初始化策略、README 说明和 AHRS 解算参数重新对齐，避免“驱动配置是 2g/2048dps，但文档或算法按 4g/256dps 理解”的隐性问题。
- 如果后续仍然出现 `WHO_AM_I` 正常但 `STATUS0/temp/acc/gyro` 全零，则更接近芯片状态域或硬件本体问题，而不是当前这层配置矛盾。

---

## [2026-05-06] - v1.7.34 QMI8658 使能前读回确认

### 变更记录
- **初始化顺序收紧**: `QMI8658_Init()` 现改为先写入 `CTRL1/2/3/5`，并在开启 `CTRL7` 之前先做一次读回确认。
- **使能条件明确**: 只有当前置配置读回值与期望值一致时，才继续写入 `CTRL7=0x03` 使能 Accel/Gyro。
- **日志更聚焦**: 启动阶段新增 `pre-enable readback` 与 `post-enable readback CTRL7` 日志，便于区分“配置没写进去”和“配置已生效但数据域仍无输出”。

### 当前效果
- 更符合 QMI8658 的推荐初始化顺序：先配置基础寄存器，再开启全局使能。
- 若后续仍然出现 `STATUS0=0x00`、`temp/acc/gyro` 全零，则更能确认问题集中在数据域或芯片状态，而不是 CTRL 配置写入次序。

---

## [2026-05-06] - v1.7.33 恢复 IMU IIC 调试日志

### 变更记录
- **IMU 调试开关收口**: 在 `User/FeatureSwitch.h` 新增 `QMI8658_DIAG_ENABLE`，当前 IMU + MAG 融合测试档默认打开，便于集中控制 IIC 排障日志。
- **QMI8658 日志恢复**: `Code_boweny/Device/QMI8658/QMI8658.c` 不再无条件 `#undef LOGD`，当 `QMI8658_DIAG_ENABLE=1` 时恢复详细 DEBUG 日志。
- **头文件兼容处理**: `Code_boweny/Device/QMI8658/QMI8658.h` 改为优先使用外部配置宏；若外部未定义，则默认关闭诊断日志。

### 当前效果
- 重新打印 QMI8658 上电等待、地址探测、`WHO_AM_I` 校验、`wait acc ready` 过程和失败后的寄存器窗口诊断。
- 仍保持当前测试档只启用 `IMU + MAG + AHRS + LOG`，无线与 GPS 继续关闭。

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

