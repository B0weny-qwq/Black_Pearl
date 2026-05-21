# Black Pearl 控制逻辑最小收口方案

## Summary

按 EmbedForge 项目标准评估后，当前最大问题不是算法本身，而是
`ship_protocol.c` 同时承担了无线协议、按键业务、手动控制、yaw-hold PID、
电机输出和 AutoDrive 调度，导致手动自稳、定速巡航、GPS 定点巡航抢同一套
电机输出。

本次建议采用“最小收口”方案：保留现有 Keil / `Code_boweny` 工程结构，
新增一个统一控制层，把“谁有权控制电机”和“yaw-hold 如何输出左右电机”
从 wireless 中收口出来。

## Key Changes

- 新增统一控制状态机 `ShipControl`，只允许一个控制模式拥有电机：
  - `STOP`
  - `MANUAL_OPEN_LOOP`
  - `MANUAL_YAW_HOLD`
  - `CRUISE_HEADING_HOLD`
  - `GPS_NAV_HEADING_HOLD`
  - `FAILSAFE_STOP`
- `ship_protocol.c` 降级为无线协议入口：
  - 继续解析 `0x11/0x13/0x14/0x15`、配对、GPS 回传。
  - `0x11` 只上报 `lr/ud/key` 给 `ShipControl`。
  - 按键只触发控制请求，不直接 `Motor_SetBothSpeed()`。
  - 移除或封装现有散落的 `ShipProtocol_ApplyMotion()`、手动 yaw-hold、定速巡航全局变量。
- `ShipControl` 统一输出电机：
  - 手动控制：根据油门/转向输入决定开环或手动自稳；只有油门前进且转向回中才锁当前航向。
  - 定速巡航：按键进入，锁定进入瞬间的绝对航向，使用保留差速余量的固定 `base_speed`，不再依赖持续油门输入。
  - GPS 定点巡航：`AutoDrive` 只计算目标航向、距离、到点/失败状态，把 `target_heading_cd + base_speed` 提交给 `ShipControl`。
- yaw-hold PID 从 wireless 迁到控制层：
  - 复用现有参数：`SHIP_YAW_HOLD_*`、`PID`、`MainLoop_GetHeadingDeg100()`、`MainLoop_GetGyroZDps100()`。
  - 保留现有左右极性、差速限幅、陀螺阻尼、输出斜坡、base derate 行为。
  - 统一提供 `ShipControl_RequestCruise()` / `ShipControl_RequestGpsNav()`，禁止 AutoDrive 或 wireless 再直接写第二套 yaw PID。
- `AutoDrive` 改为导航规划模块：
  - 保留 GPS 点位、距离、目标航向、到点判断。
  - 删除对 `ShipProtocol_ApplyYawHoldTarget()` 的依赖，改调 `ShipControl_RequestGpsNav(...)`。
  - `AutoDrive_StopMotion()` 改为请求控制层停止 GPS 模式，不直接 `Motor_StopAll()`，避免跨层抢电机。
- `MainLoop_RunOnce()` 保持现有节拍：
  - `Wireless_Poll()` 和 `ShipProtocol_RunScheduler()` 继续处理通信。
  - `AutoDrive_Poll()` 可继续由协议调度调用，或迁到主循环统一调用；最小方案先保留位置。
  - `Motor_Service()` 仍在主循环末尾执行，所有目标速度只由 `ShipControl` 最终写入。

## Public Interfaces

新增控制层接口：

```c
void ShipControl_Init(void);
void ShipControl_Tick(u32 now_ms);
void ShipControl_UpdateManualInput(u8 lr, u8 ud, u8 key, u32 now_ms);
void ShipControl_RequestCruise(u16 heading_cd, int16 base_speed);
void ShipControl_RequestGpsNav(u16 target_heading_cd, int16 base_speed);
void ShipControl_Stop(u8 reason);
u8 ShipControl_IsAutoMode(void);
```

AutoDrive 对外语义保持：

- `0x13/0x14/0x15` 协议格式不变。
- 到点、超时、GPS 不 ready、heading 不 ready 时退出并请求 `ShipControl_Stop(...)`。

wireless 对外协议保持：

- 旧遥控器 payload 不变：`0x11 = lr/ud/key`。
- E 键沿用当前语义：进入/退出定速巡航；进入时锁当前绝对航向。

## Test Plan

- 静态检查：
  - `ship_protocol.c` 中只允许协议、按键、链路、回包逻辑；不再直接散落 `Motor_SetBothSpeed()`。
  - `AutoDrive` 不再 include `ship_protocol.h`。
  - 只有 `ShipControl` 负责 yaw-hold PID 和最终电机目标。
- 编译检查：
  - 更新 Keil 工程加入 `ShipControl.c/.h` 和 include path。
  - 确认 STC/Keil C 语法兼容，避免 C99-only 写法。
- 台架场景：
  - 遥控回中：电机停止。
  - 手动前进+转向：开环差速，无 yaw-hold 抢方向。
  - 手动前进+转向回中：锁当前航向并自稳。
  - E 键进入定速：无持续油门输入也按固定速度和锁定航向运行。
  - E 键退出定速：停止并清 PID。
  - GPS 目标点有效：AutoDrive 只更新目标航向，控制层输出航向保持。
  - 遥控超时/低电/GPS 丢失/heading 丢失：当前模式退出，电机停止或进入已有返航逻辑。

## Assumptions

- 本次不做 EmbedForge 标准目录大迁移，只做控制边界收口。
- 不改旧遥控器协议，不新增无线 payload 字段。
- 当前 `MainLoop_GetHeadingDeg100()` 的融合绝对航向作为定速巡航和 GPS 巡航主反馈。
- 没有编码器，所以“定速”仍是固定 PWM/base speed，不是真实速度闭环。
- `SHIP_YAW_HOLD_OUTPUT_SIGN=1`、现有电机极性和 yaw-hold 参数先视为实船已验证，不在本次重调。
