/**
 * @file    total.md
 * @brief   Black Pearl v1.1 当前工程总览
 * @author  boweny
 * @date    2026-05-13
 * @version v1.7.61
 */

# Black Pearl v1.1 当前工程总览

> 本文件描述根目录工程的真实运行状态。
> `black_-pearl-master/doc/project_doc` 仅作镜像说明，不反向定义当前代码现状。

## 1. 当前结论

- 当前运行档为 `Wireless + GPS + MAG + IMU + AHRS`，无线旧遥控器协议、GPS 回传和 AHRS 航向链同时启用。
- 旧遥控器空口数据格式已按 `ship_Gps_V2.1_20260406-115200` 复核：命令号、帧格式、`0x12` 15 字节 GPS 回传、`0x13/0x14/0x15` 点位格式保持旧版 wire 兼容。
- `0x12` 半球字段继续固定为 `E/W`，这是旧版 `nmea41_Get_EW()='E'`、`nmea41_Get_NS()='W'` 的兼容行为，不是新协议格式。
- 当前工作区 `0x11` wire payload 仍是旧版 `lr/ud/key`，但手动控制应用层已开启 `SHIP_YAW_HOLD_MANUAL_ENABLE=1`，并以 `SHIP_YAW_HOLD_STEER_GATE=10U` 作为直线自稳门限；当左右输入偏差不超过该门限且油门为前进时，会在当前电机 PWM 上叠加 yaw-hold 输出。当前控制周期为 `SHIP_YAW_HOLD_PERIOD_MS=150UL`，`Kp/死区/日志节流` 也统一收在 `User/FeatureSwitch.h` 顶部，便于现场调参。

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
#define SHIP_YAW_HOLD_ENABLE           1
#define SHIP_YAW_HOLD_MANUAL_ENABLE    1
#define SHIP_YAW_HOLD_PERIOD_MS        150UL
#define SHIP_YAW_HOLD_LOG_ENABLE       1
#define SHIP_YAW_HOLD_LOG_PERIOD_MS    1000UL
#define SHIP_YAW_HOLD_OUTPUT_LIMIT     100
#define SHIP_YAW_HOLD_STEER_GATE       10U
#define SHIP_YAW_HOLD_KP_Q10           4
#define SHIP_YAW_HOLD_DEADBAND_CD      150
#define SHIP_YAW_HOLD_KI_Q10           0
#define SHIP_YAW_HOLD_KD_Q10           0
```

- `ENABLE_GPS_MODULE=1` 表示 GPS 初始化、轮询和 `0x12` 状态回传均进入当前固件。
- `SHIP_PROTOCOL_COMPAT_ENABLE=0` 表示当前使用调度器链路 `ShipProtocol_RunScheduler()`，不是额外兼容轮询入口。
- `SHIP_YAW_HOLD_MANUAL_ENABLE=1` 是当前应用行为与旧版纯开环手动控制的主要差异点；当前直线自稳触发条件是 `|steering| <= 10` 且油门为前进。
- `SHIP_YAW_HOLD_KP_Q10=4` 继续保持保守 P-only，`SHIP_YAW_HOLD_DEADBAND_CD=150` 用于压住静止抖动，`SHIP_YAW_HOLD_LOG_PERIOD_MS=1000UL` 用于限制 `[MOT]` 诊断输出频率。

## 3. 启动顺序

```text
SYS_Init()
  -> APP_config()
  -> log_init()
  -> Wireless_Init()
  -> GPS_Init()
  -> Sensor_I2C_prepare()
  -> AHRS_Reset()
  -> QMC6309_Init()
  -> QMI8658_Init()
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
- `ShipProtocol_RunScheduler()` 负责旧遥控器配对、收帧、分发命令和合法帧后立即回发 `0x12`。
- `MAG_StandalonePoll()` 当前关闭，磁力计由 AHRS 低频读取。

## 5. 旧遥控器协议核对

| 项目 | 当前状态 |
|------|----------|
| 帧格式 | `AA | len=2+payload | cmd | payload | XOR(len/cmd/payload) | BB`，与旧版一致 |
| 命令号 | `0x0F/0x10/0x11/0x12/0x13/0x14/0x15`，与旧版一致 |
| `0x11` 遥控 | payload 仍为 `lr, ud, key`；当前应用层叠加手动 yaw-hold，非旧版纯开环，且在小转向输入下会直接输出电机 PWM |
| `0x12` GPS 回传 | 固定 15 字节：`sat, angle, E, lon1, lon2, W, lat1, lat2, power_level, autodrive_status` |
| `0x13/0x14` 点位 | 10 字节旧格式：`lon_dir, lon1, lon2, lat_dir, lat1, lat2` |
| `0x15` 自动返航配置 | 正常 11 字节帧为 `switch + 10 字节点位`，写入当前 MCU flash 配置区 |
| 电量回传 | 使用当前工程检测电压 ADC 通道 `ADC_CH8`，按旧版电量等级 `0..4` 回传 |
| 低电返航 | 电量等级 `0`、600 tick、自动驾驶关闭、当前油门小于 10 时触发返航入口 |

## 6. 存储与已知差异

- 当前无外置 EEPROM；自动返航配置通过 `AutoDriveCfg_Save()` 写入 STC flash/EEPROM 区，地址为 `0x0001F800`。
- `0x15` 短包行为比旧版更防御：短包只保存开关，只有完整 `1 + 10` 字节时才更新返航点；正常旧遥控器完整帧不受影响。
- 当前文档按工作区真实状态记录；如果后续要发布“严格旧版应用行为”，需要先关闭 `SHIP_YAW_HOLD_MANUAL_ENABLE` 或移除手动自稳门控，再更新本文档。

## 7. 最近变更

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-13 | `v1.7.61` | 同步手动自稳开启、转向门限 10 和 PWM 输出口径，更新当前真实开关、主循环和已知差异。 |
| 2026-05-12 | `v1.7.59` | yaw-hold 参数收口到 `FeatureSwitch.h`，补齐中文注释。 |
| 2026-05-11 | `v1.7.58` | AHRS 切换为 Mahony 四元数融合，保留现有航向链和日志格式。 |

> 详细历史请查阅 `doc/project_doc/date.md`。
