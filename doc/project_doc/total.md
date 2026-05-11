/**
 * @file    total.md
 * @brief   Black Pearl v1.1 current project summary
 * @author  boweny
 * @date    2026-05-11
 * @version v1.7.58
 */

# Black Pearl v1.1 当前工程总览

> 根目录 `doc/project_doc` 是当前真实工程文档源。
> `black_-pearl-master/doc/project_doc` 只做镜像说明，不反向定义代码现状。

## 1. 当前结论

- 之前 IMU 异常已确认是 `QMI8658` 器件本体损坏，不再继续把 I2C 或 AHRS 当成那次故障根因。
- 当前运行档位已经恢复为 `IMU + MAG + AHRS`，并且 AHRS 内核已经从欧拉角互补滤波切换为四元数传播。
- GPS、无线、控制闭环本轮继续关闭，不参与姿态融合链。
- yaw 策略固定为：短期信陀螺，长期由磁力计慢修正；GPS 只保留为下一阶段接口预留。

## 2. 当前功能开关

当前开关集中在 `User/FeatureSwitch.h`：

```c
#define ENABLE_WIRELESS_MODULE         0
#define ENABLE_GPS_MODULE              0
#define ENABLE_MAG_MODULE              1
#define ENABLE_IMU_MODULE              1

#define ENABLE_MAG_STANDALONE_POLL     0
#define ENABLE_IMU_AHRS_POLL           1
#define ENABLE_IMU_BASIC_POLL          0

#define AHRS_MAG_ENABLE                1
#define QMI8658_DIAG_ENABLE            0
#define QMI8658_INIT_NONBLOCKING       0
```

- `ENABLE_MAG_STANDALONE_POLL=0`，磁力计不再单独刷屏，但 `QMC6309` 仍由 AHRS 内部低频读取。
- `ENABLE_IMU_AHRS_POLL=1`、`ENABLE_IMU_BASIC_POLL=0`，当前主路径只保留姿态融合输出。
- `AHRS_MAG_ENABLE=0` 可临时切回 gyro-only 诊断，但不是默认运行态。

## 3. 真实启动顺序

当前 `SYS_Init()` 的真实顺序是：

```text
SYS_Init()
  -> GPIO_config()
  -> Switch_config()
  -> Timer_config()
  -> ADC_config()
  -> UART_config()
  -> I2C_config()
  -> EA = 1
  -> APP_config()
  -> log_init()
  -> Sensor_I2C_prepare()
  -> g_qmi8658_ready = 0
  -> AHRS_Reset()
  -> QMC6309_Init()
  -> QMI8658_Init()
```

- `Sensor_I2C_prepare()` 会先把 `P1.4/P1.5` 拉回传感器 I2C 状态。
- `AHRS_Reset()` 在传感器初始化前执行，保证四元数、零偏学习和滤波状态从干净起点开始。
- 当前 `ENABLE_GPS_MODULE=0`、`ENABLE_WIRELESS_MODULE=0`，启动链路不会进入 GPS 或无线初始化。

## 4. 真实主循环

当前 `MainLoop_RunOnce()` 的有效路径是：

```text
MainLoop_RunOnce()
  -> if (IMU_ServicePoll()) IMU_AhrsPoll()
  -> Task_Pro_Handler_Callback()
```

- `IMU_ServicePoll()` 负责维护 `QMI8658` ready/error 状态。
- `IMU_AhrsPoll()` 每 `17ms` 读取一次 `QMI8658_ReadAll()`，并调用 `AHRS_UpdateRaw6Axis()`。
- `IMU_AhrsPoll()` 每 `100ms` 读取一次 `QMC6309_ReadXYZFiltered()`，并调用 `AHRS_UpdateRawMag()`。
- `Task_Pro_Handler_Callback()` 保持当前任务框架与 `P3.6` LED 节拍，不因为 AHRS 恢复而被绕开。

## 5. 当前 AHRS 内核

当前 `Code_boweny/Function/AHRS` 的真实行为：

- 内部状态为 `q0/q1/q2/q3` 浮点四元数，AHRS 上下文已迁到 `xdata`，避免 Keil C51 `EDATA` 溢出。
- IMU 路径为：轴映射 -> 陀螺零偏学习 -> 零偏扣除 -> deadband -> 低通 -> 四元数传播。
- 加速度路径为：`1g` 参考自学习 + 模长门控；只有 `acc_valid` 时才参与重力方向误差反馈。
- 磁力计路径为：轴映射 -> 低通 -> 归一化 -> 倾斜补偿后的 Mahony 航向误差反馈。
- 对外接口保持不变：`AHRS_GetState()`、`roll/pitch/yaw_deg100`、`gyro_*_dps100`、`flags` 都沿用现有结构。

## 6. 当前 AHRS 日志口径

现有日志格式保持不变，上位机无需改正则：

```text
[AHRS] I: r=+1.23 p=-0.45 y=-8.90 g=+0.02 -0.01 +0.03 f=17
[AHRS] I: r=+1.20 p=-0.40 y=-8.85 ys=4 g=+0.01 +0.00 +0.01 f=1F
[AHRS] I: r=+1.18 p=-0.38 y=-8.80 yr=+0.00 f=1F
```

- `r/p/y`：当前绝对姿态角，单位 `deg`。
- `g=`：当前三轴角速度，单位 `deg/s`。
- `ys=`：yaw 锁零前的稳定计数。
- `yr=`：锁零后的相对 yaw。
- `f=17`：`READY + ACC_VALID + MAG_VALID + ACC_REF_READY`。
- `f=1F`：在 `0x17` 基础上额外包含 `GYRO_BIAS_READY`。
- `mv=0`：磁力计当前无效。
- `me=0`：编译期关闭磁修正，仅保留 gyro-only 路径。

## 7. 当前限制与下一步

- 本轮没有加入磁力计硬铁/软铁校准，所以绝对 yaw 仍属于“工程可用、未最终标定”状态。
- GPS 仍未接入航向融合主链，只保留为下一阶段低频长期参考。
- PID、电机控制、自动航向保持没有并入本轮，避免把“姿态融合恢复”和“控制闭环联调”耦合。
- 如果现场方向不符，只调整 `AHRS_IMU_BODY_*` / `AHRS_MAG_BODY_*` 宏，不在业务层分散取反。

## 8. 最近版本

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-11 | `v1.7.58` | AHRS 内核切换为四元数 Mahony 融合，保留现有日志与接口 |
| 2026-05-11 | `v1.7.57` | 确认 IMU 根因是器件本体损坏，恢复 `IMU + MAG + AHRS` 主链 |
| 2026-05-11 | `v1.7.56` | 复核传感器 I2C 路径并清理过严的总线空闲前置判定 |

> 详细历史请查阅 `doc/project_doc/date.md`。
