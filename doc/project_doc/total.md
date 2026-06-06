/**
 * @file    total.md
 * @brief   Black Pearl v1.1 当前工程总览
 * @author  boweny
 * @date    2026-06-01
 * @version v1.7.70
 */

# Black Pearl v1.1 当前工程总览

> 本文件描述根目录工程的真实运行状态。
> `black_-pearl-master/doc/project_doc` 仅作镜像说明，不反向定义当前代码现状。

## 1. 当前结论

- 当前运行档为 `Wireless + GPS + MAG + IMU + AHRS`，无线旧遥控器协议、GPS 回传和 AHRS 航向链同时启用。
- 旧遥控器空口数据格式已按 `ship_Gps_V2.1_20260406-115200` 复核：命令号、帧格式、`0x12` 15 字节 GPS 回传、`0x13/0x14/0x15` 点位格式保持旧版 wire 兼容。
- `0x12` 半球字段继续固定为 `E/W`，这是旧版 `nmea41_Get_EW()='E'`、`nmea41_Get_NS()='W'` 的兼容行为，不是新协议格式。
- 当前工作区 `0x11` wire payload 仍是旧版 `lr/ud/key`；遥控输入值只由 `0x11` 油门帧更新，手动控制目标则由 `ShipControl_Tick()` 每 `SHIP_MANUAL_CONTROL_PERIOD_MS=10ms` 使用最新输入连续刷新，底层 PWM 仍按硬件定时器频率输出；`SHIP_RC_INPUT_LOG_PERIOD_MS`、`SHIP_MOT_LOG_PERIOD_MS` 和 `SHIP_YAW_HOLD_LOG_PERIOD_MS` 只限制日志打印，不能影响控制更新。手动控制应用层已开启 `SHIP_YAW_HOLD_MANUAL_ENABLE=1`，直线自稳门槛为左右电机预期差速小于最大单侧输入的 `20%`，且油门为前进；当前无遥控油门时不会启动空闲 yaw-hold 驱动电机。yaw 误差先按 `SHIP_YAW_HOLD_FULL_ERROR_CD=1000` 归一化到 `±1000` 控制量，再按当前基础油门和电机满量程余量换算成左右差速；yaw 自稳基础速度只在偏航误差超过 `10.00°` 后开始降额，`20.00°` 后达到满降额。
- 工程硬约束：所有“手动自稳/定速巡航/返航循迹/钓点巡航”的最终电机输出都必须走 `ShipControl` / yaw-hold PID 路线，复用当前已验证的左右极性、差速限幅、陀螺阻尼和输出斜坡；禁止在 `AutoDrive` 或 `wireless` 里另写定时左/右转、另写左右电机极性、另写第二套 yaw PID。
- `AutoDrive` 并非独立主循环入口，而是隐藏在 `ShipProtocol_RunScheduler()` 内初始化与轮询；`0x13/0x14/0x15`、低电返航和链路超时都会走到这条链。当前 `AutoDrive` 只负责 GPS 点位规划、目标航向和到点判断，电机自稳输出必须交给 yaw-hold PID 公共链路。
- 返航/钓点巡航的 `target_heading_cd` 不是启动时固定一次的角度；`AUTO_DRIVE_RUNING` 状态下每当 `gps->update_sequence` 变化，都会用最新当前 GPS 点和目标点重新计算目标航向，随后继续提交给 `ShipControl_RequestGpsNav(target_heading_cd, base_speed)`。GPS 未更新的 10ms 控制周期内只沿用上一帧目标航向。
- 当前 GPS 定点巡航的当前船头角必须来自 `MainLoop_GetHeadingDeg100()` 的融合绝对航向；磁力计已按实测 `MAG_COMPASS_DIRECTION_SIGN=-1`、`MAG_COMPASS_INSTALL_OFFSET_CD=21930` 修正正北零点与旋转方向，上位机“船头朝向”应显示 `HDG fused` 才能证明航向链已 ready。
- D 键 GPS 北向校准当前已接入正式链路：`ship_protocol.c` 负责 1s 内双击 D 触发和 busy 期间 E 键取消，`NorthCalib.c` 负责 `CHECK_READY -> ALIGN_NORTH -> RUN_STRAIGHT -> CALC -> SAVE` 状态机，`MainLoop_GetHeadingDeg100()` 统一叠加 `north_offset_cd`，EEPROM 使用 `0x000200/0x000400` A/B 双槽保存。
- `Code_boweny/` 与 `User/` 当前按中文 Doxygen 口径维护：文件头使用 `@file/@brief/@details`，公开结构体和内部运行态结构体字段采用字段后置中文注释；改结构体、状态机或日志字段时必须同步模块 README、`date.md` 和相关日志工具说明。

## 2. 当前开关

当前控制开关集中在 `User/FeatureSwitch.h`。

```c
#define ENABLE_WIRELESS_MODULE         1
#define ENABLE_GPS_MODULE              1
#define ENABLE_MAG_MODULE              1
#define ENABLE_IMU_MODULE              1

#define ENABLE_SHIP_PROTOCOL_SCHED     1
#define SHIP_PROTOCOL_POLL_ENABLE      1
#define SHIP_PROTOCOL_COMPAT_ENABLE    0

#define ENABLE_MAG_STANDALONE_POLL     0
#define ENABLE_IMU_AHRS_POLL           1
#define ENABLE_IMU_BASIC_POLL          0

#define SHIP_THROTTLE_PWM_ENABLE       1
#define SHIP_MANUAL_CONTROL_PERIOD_MS  10UL
#define SHIP_YAW_HOLD_ENABLE           1
#define SHIP_YAW_HOLD_MANUAL_ENABLE    1
#define SHIP_YAW_HOLD_PERIOD_MS        50UL
#define SHIP_YAW_HOLD_LOG_ENABLE       1
#define SHIP_YAW_HOLD_LOG_PERIOD_MS    1000UL
#define SHIP_YAW_HOLD_OUTPUT_LIMIT     1000
#define SHIP_YAW_HOLD_FULL_ERROR_CD    1000
#define SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE 320
#define SHIP_YAW_HOLD_OUTPUT_SIGN      1
#define SHIP_MANUAL_YAW_HOLD_DIFF_PERCENT 20U
#define SHIP_YAW_HOLD_DERATE_START_CD  1000
#define SHIP_YAW_HOLD_DERATE_FULL_CD   2000
#define SHIP_YAW_HOLD_DERATE_MIN_BASE  500
#define SHIP_YAW_HOLD_KP_Q10           384
#define SHIP_YAW_HOLD_DEADBAND_CD      50
#define SHIP_YAW_HOLD_KI_Q10           0
#define SHIP_YAW_HOLD_KD_Q10           96
#define SHIP_MOT_LOG_PERIOD_MS         200U
#define SHIP_RC_INPUT_LOG_PERIOD_MS    500U
```

- `ENABLE_GPS_MODULE=1` 表示 GPS 初始化、轮询和 `0x12` 状态回传均进入当前固件。
- `SHIP_PROTOCOL_COMPAT_ENABLE=0` 表示当前使用调度器链路 `ShipProtocol_RunScheduler()`，不是额外兼容轮询入口。
- `SHIP_MANUAL_CONTROL_PERIOD_MS=10UL` 是内部手动控制目标刷新周期；`SHIP_MOT_LOG_PERIOD_MS` 和 `SHIP_RC_INPUT_LOG_PERIOD_MS` 是独立日志周期，二者不能作为控制节拍。
- `SHIP_YAW_HOLD_MANUAL_ENABLE=1` 是当前应用行为与旧版纯开环手动控制的主要差异点；当前直线自稳触发条件是左右电机预期差速小于最大单侧输入的 `20%`，且油门为前进。
- `SHIP_YAW_HOLD_OUTPUT_LIMIT=1000` 与 `Motor_SetSpeed()` 满量程统一；`SHIP_YAW_HOLD_FULL_ERROR_CD=1000` 表示 10.00° 偏航达到满控制输入；`SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE=320` 表示满输出时差速最多为当前基础油门的 32%，并继续受电机上限余量限制。
- `SHIP_YAW_HOLD_DERATE_START_CD=1000` / `SHIP_YAW_HOLD_DERATE_FULL_CD=2000` 表示 yaw 自稳基础速度在偏航误差 `10.00°` 后才开始降额，`20.00°` 后达到满降额，满降额基础速度上限为 `500`。
- `SHIP_YAW_HOLD_OUTPUT_SIGN=1` 是当前实船已验证的左右极性方向；后续返航、钓点巡航和任何自稳输出都必须复用这个公共参数，不允许在其他模块单独翻转。
- `SHIP_YAW_HOLD_KP_Q10=384`、`SHIP_YAW_HOLD_KI_Q10=0`、`SHIP_YAW_HOLD_KD_Q10=96` 是当前 yaw-hold PID 参数，`SHIP_YAW_HOLD_DEADBAND_CD=50` 用于压住 0.50° 内的小抖动，`SHIP_YAW_HOLD_LOG_PERIOD_MS=1000UL` 用于限制控制层 yaw-hold 诊断输出频率。

## 3. 启动顺序

```text
SYS_Init()
  -> GPIO_config()
  -> Switch_config()
  -> Timer_config()
  -> ADC_config()
  -> UART_config()
  -> I2C_config()
  -> log_init()
  -> Wireless_Init()
  -> GPS_Init()

main()
  -> MainLoop_Bootstrap()
     -> NorthCalib_Init()
     -> 其它主循环快照初始化

MainLoop 运行期首次进入配对/传感器启动链后
  -> Sensor_I2C_prepare()
  -> QMC6309_Init()
  -> QMI8658_RequestReinit() / QMI8658_Service()
  -> AHRS_Reset()
```

## 4. 主循环

```text
MainLoop_RunOnce()
  -> GPS_Poll()
  -> Wireless_Poll()
  -> ShipProtocol_RunScheduler()
  -> Wireless_SearchSignalPoll()
  -> IMU_ServicePoll()
  -> IMU_AhrsPoll()
  -> Task_Pro_Handler_Callback()
```

- `IMU_AhrsPoll()` 持续更新 AHRS 与 `HeadingEstimator`，为 yaw-hold 提供相对航向。
- `ShipProtocol_RunScheduler()` 负责旧遥控器配对、收帧、分发命令和合法帧后立即回发 `0x12`，同时内部执行 `AutoDrive_Init()`、`AutoDrive_LinkAliveTick()` 与 `AutoDrive_Poll()`。
- `AutoDrive_Poll()` 进入返航/钓点巡航后根据 GPS 新点更新目标航向与距离，到点阈值由 `AUTODRIVE_ARRIVE_DISTANCE_M=3` 判定；实际电机差速必须通过 `ShipControl_RequestGpsNav()` 输出，保持和手动自稳完全同一条 PID 路线。
- `MAG_StandalonePoll()` 当前关闭，磁力计由 AHRS 低频读取。

## 5. 旧遥控器协议核对

| 项目 | 当前状态 |
|------|----------|
| 帧格式 | `AA | len=2+payload | cmd | payload | XOR(len/cmd/payload) | BB`，与旧版一致 |
| 命令号 | `0x0F/0x10/0x11/0x12/0x13/0x14/0x15`，与旧版一致 |
| `0x11` 遥控 | payload 仍为 `lr, ud, key`；当前应用层叠加手动 yaw-hold，非旧版纯开环，且在小转向输入下会直接输出电机 PWM |
| `0x12` GPS 回传 | 固定 15 字节：`sat, angle, E, lon1, lon2, W, lat1, lat2, power_level, autodrive_status` |
| `0x13/0x14` 点位 | 10 字节旧格式：`lon_dir, lon1, lon2, lat_dir, lat1, lat2` |
| `0x15` 自动返航配置 | 正常 11 字节帧为 `switch + 10 字节点位`；有效点位写入返航原点，开关只在本次运行 RAM 中生效 |
| 返航/钓点巡航自稳 | `AutoDrive` 在 GPS `update_sequence` 变化时用最新当前点和目标点重算 `target_heading_cd` 与距离；电机修正统一提交给 `ShipControl_RequestGpsNav()`，必须复用 yaw-hold PID，不允许定时左/右转 |
| 电量回传 | 使用当前工程检测电压 ADC 通道 `ADC_CH8`，按旧版电量等级 `0..4` 回传 |
| 低电返航 | 电量等级 `0`、600 tick、自动驾驶关闭、当前油门小于 10 时触发返航入口 |

## 6. 存储与已知差异

- 当前无外置 EEPROM；`AutoDriveCfg_Save()` 只把返航原点写入 STC flash/EEPROM 区 `0x000600`，钓点不保存，自动返航开关只在运行期 RAM 中生效；D 键北向校准记录单独写入 `0x000200/0x000400` A/B 双槽。
- `0x15` 短包行为比旧版更防御：短包只保存开关，只有完整 `1 + 10` 字节时才更新返航点；正常旧遥控器完整帧不受影响。
- 自稳路径是工程级公共能力，不是手动遥控专用能力；手动直线自稳、定速巡航、GPS 返航、GPS 钓点巡航都必须共用 `ShipControl` / yaw-hold PID 输出链。
- 当前文档按工作区真实状态记录；如果后续要发布“严格旧版应用行为”，需要先关闭 `SHIP_YAW_HOLD_MANUAL_ENABLE` 或移除手动自稳门控，再更新本文档。

## 7. 最近变更

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-06-03 | `v1.7.71` | 修正 D 键北向校准交互：1s 内双击 D 触发，校准 busy 期间 E 键可取消；保留对准、直跑和总流程超时退出。 |
| 2026-05-31 | `v1.7.68` | 新增 D 键 GPS 北向校准链路：`NorthCalib` 负责状态机和 EEPROM A/B 双槽，`MainLoop_GetHeadingDeg100()` 统一叠加 `north_offset_cd`，README/模块文档同步到当前真实关系。 |
| 2026-05-18 | `v1.7.65` | 同步 GPS 定点巡航角度策略：`target_heading_cd` 随 GPS 新坐标实时重算，GPS 未更新时沿用上一帧目标角；当前船头角来自融合绝对航向 `MainLoop_GetHeadingDeg100()`，并记录磁罗盘方向/零点修正。 |
| 2026-05-16 | `v1.7.64` | 明确工程级自稳约束：所有自稳模式、返航循迹和钓点巡航都必须走 yaw-hold PID 公共路线；AutoDrive 只负责 GPS 规划目标航向和到点判断，不允许另写定时左/右转或第二套左右极性。 |
| 2026-05-15 | `v1.7.63` | 修正手动 yaw 自稳单位和量程，并恢复 10ms 手动控制连续刷新；日志打印与内部控制更新解耦；无遥控油门时不再启动空闲 yaw-hold，避免上电自转和遥控阶梯卡顿。 |
| 2026-05-13 | `v1.7.62` | 复核各 device README、补齐 `AutoDrive/README.md`，同步 `AutoDrive` 实际接入路径和头文件注释口径。 |
| 2026-05-13 | `v1.7.61` | 同步手动自稳开启、转向门限 10 和 PWM 输出口径，更新当前真实开关、主循环和已知差异。 |
| 2026-05-12 | `v1.7.59` | yaw-hold 参数收口到 `FeatureSwitch.h`，补齐中文注释。 |
| 2026-05-11 | `v1.7.58` | AHRS 切换为 Mahony 四元数融合，保留现有航向链和日志格式。 |

> 详细历史请查阅 `doc/project_doc/date.md`。
