/**
 * @file    total.md
 * @brief   Black Pearl v1.1 当前工程总览
 * @author  boweny
 * @date    2026-05-12
 * @version v1.7.59
 */

# Black Pearl v1.1 当前工程总览

> 本文件描述根目录工程的真实运行状态。
> `black_-pearl-master/doc/project_doc` 仅作镜像说明，不反向定义当前代码现状。

## 1. 当前结论

- 当前运行档为 `IMU + MAG + AHRS`。
- 船体 yaw 自稳已启用，但参数保持保守，先保证差速修正不发散。
- 无线和 GPS 不进入姿态融合主链。

## 2. 当前开关

当前控制开关集中在 `User/FeatureSwitch.h`。

```c
#define ENABLE_WIRELESS_MODULE         1
#define ENABLE_GPS_MODULE              0
#define ENABLE_MAG_MODULE              1
#define ENABLE_IMU_MODULE              1

#define ENABLE_MAG_STANDALONE_POLL     0
#define ENABLE_IMU_AHRS_POLL           1
#define ENABLE_IMU_BASIC_POLL          0

#define SHIP_YAW_HOLD_ENABLE           1
#define SHIP_YAW_HOLD_PERIOD_MS        100UL
#define SHIP_YAW_HOLD_OUTPUT_LIMIT     60
#define SHIP_YAW_HOLD_KP_Q10           12
#define SHIP_YAW_HOLD_KI_Q10           0
#define SHIP_YAW_HOLD_KD_Q10           0
```

- `SHIP_YAW_HOLD_KP_Q10=12` 表示当前只保留比例环节，先压住过冲。
- `SHIP_YAW_HOLD_PERIOD_MS=100UL` 用于把差速控制节拍降下来。
- `SHIP_YAW_HOLD_OUTPUT_LIMIT=60` 限制左右油门差值，避免修正过猛。

## 3. 启动顺序

```text
SYS_Init()
  -> ... 外设初始化 ...
  -> Sensor_I2C_prepare()
  -> AHRS_Reset()
  -> QMC6309_Init()
  -> QMI8658_Init()
```

## 4. 主循环

```text
MainLoop_RunOnce()
  -> IMU_ServicePoll()
  -> IMU_AhrsPoll()
  -> Task_Pro_Handler_Callback()
  -> ShipProtocol 调度 / yaw hold
```

- `IMU_AhrsPoll()` 持续更新 AHRS 和 heading 链。
- `ShipProtocol` 使用当前 heading 进行静态自稳和遥控 yaw 保持。

## 5. 航向语义

- `yr` 是船体闭环使用的一维融合相对航向。
- `yg` 是 gyro 诊断相对航向。
- `ym` 是磁航向诊断相对值。
- `[MOT]` 日志展示模式、yaw、基础油门、输出和左右电机结果。

## 6. 最近变更

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-12 | `v1.7.59` | yaw-hold 参数收口到 `FeatureSwitch.h`，`Kp` 下调到 `12` 并补齐中文注释。 |
| 2026-05-11 | `v1.7.58` | AHRS 切换为 Mahony 四元数融合，保留现有航向链和日志格式。 |
| 2026-05-11 | `v1.7.57` | 重新确认 IMU 根因并恢复 AHRS 融合主链。 |

> 详细历史请查阅 `doc/project_doc/date.md`。
