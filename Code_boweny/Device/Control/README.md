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
  - GPS 进入前进巡航前的原地对准提交给 `ShipControl_RequestGpsAlign()`
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
- E 键定速巡航锁进入瞬间的融合航向，当前请求基础速度为 `800`。
- 定速巡航不走 GPS/yaw 大角度靠近减速的基速降档；`base` 保持请求速度，只在左右电机上叠加 yaw-hold 差速修正。
- 运行时可看 `[DATA] I: cruise run req=... base=... l=... r=... err=... pid=... diff=... tgt=...`，确认定速巡航有没有被内部降速。
- GPS 定点巡航由 `AutoDrive` 计算目标航向，`ShipControl` 只负责按目标航向自稳输出电机。
- GPS 对准阶段使用 `ShipControl_RequestGpsAlign()`，它与正常 GPS 巡航同属 `GPS_NAV_HEADING_HOLD` 模式，但使用单独的软 PID。
- yaw 自稳基础速度降额只在偏航误差较大时触发：`SHIP_YAW_HOLD_DERATE_START_CD=1000` 表示 `10.00°` 后开始降速，`SHIP_YAW_HOLD_DERATE_FULL_CD=2000` 表示 `20.00°` 后达到满降速，满降速基础速度上限为 `500`。
- 对准 PID 默认 `Kp=384(Q10)`、`Ki=0`、`Kd=0`；正常 yaw-hold 参数以 `User/FeatureSwitch.h` 为准，当前 `Kp=512(Q10)`、`Ki=128(Q10)`、`Kd=64(Q10)`。
- 对准阶段最终左右差速命令限制为 `SHIP_MOTOR_OUTPUT_MAX_COMMAND * 8%`，当前约 `68`，用于降低低速原地转向过快和抖动。
- `SHIP_YAW_HOLD_OUTPUT_LIMIT = 1000` 是 PID 内部归一化控制量，不是 PWM duty；最终电机命令仍由 `ShipControl_YawControlToSpeed()` 换算并受 `SHIP_MOTOR_OUTPUT_MAX_COMMAND` 限制。

## 配置来源

控制周期、yaw-hold 增益、差速限幅、日志节流和 PWM 总开关都以 `User/FeatureSwitch.h` 为准。
