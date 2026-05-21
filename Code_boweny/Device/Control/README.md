# ShipControl 统一控制层说明

`Code_boweny/Device/Control/` 是当前工程的船体运动控制仲裁层。

它的职责不是解析无线协议，也不是计算 GPS 点位，而是统一决定当前谁有权写电机目标，并复用同一套 yaw-hold PID 输出左右差速。

## 当前模式

- `STOP`
- `MANUAL_OPEN_LOOP`
- `MANUAL_YAW_HOLD`
- `CRUISE_HEADING_HOLD`
- `GPS_NAV_HEADING_HOLD`
- `FAILSAFE_STOP`

## 输入来源

- `ship_protocol.c`
  - `0x11` 遥控输入提交给 `ShipControl_UpdateManualInput()`
  - E 键定速巡航提交给 `ShipControl_RequestCruise()`
  - 链路/控制帧超时调用 `ShipControl_Stop()`
- `autodrive.c`
  - GPS 目标航向和基础速度提交给 `ShipControl_RequestGpsNav()`
  - 自动驾驶退出时调用 `ShipControl_StopGpsNav()`

## 输出边界

只有 `ShipControl.c` 直接调用：

- `Motor_Init()`
- `Motor_SetBothSpeed()`
- `Motor_StopAll()`
- `PID_UpdateTarget()`
- `PID_Reset()`

`wireless` 和 `AutoDrive` 不应再直接写左右电机，也不应再各自维护第二套 yaw PID。

## 关键行为

- 手动转向明显时走开环差速。
- 手动前进且转向回中稳定后，锁当前融合航向并进入手动 yaw 自稳。
- E 键定速巡航锁进入瞬间的融合航向，基础速度限制在 `850`，给 yaw-hold 差速留出电机余量。
- GPS 定点巡航由 `AutoDrive` 计算目标航向，`ShipControl` 只负责按目标航向自稳输出电机。
- GPS 对准阶段允许 `base_speed=0`，此时控制层可用原地差速转向对准目标航向。

## 配置来源

控制周期、yaw-hold 增益、差速限幅、日志节流和 PWM 总开关都以 `User/FeatureSwitch.h` 为准。
