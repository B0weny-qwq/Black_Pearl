# AutoDrive 自动返航、钓点巡航与北向校准说明

`Code_boweny/Device/AutoDrive/` 是当前根目录工程里的自动返航/钓点巡航层。
它不是独立主循环入口，而是由 `Code_boweny/Device/WIRELESS/ship_protocol.c`
在协议调度中初始化、保活和轮询；D 键北向校准也与它位于同一目录下，但作为独立小模块存在。

## 当前接入位置

真实调用链：

```text
ShipProtocol_RunScheduler()
  -> ShipProtocol_InitRuntime()
     -> AutoDrive_Init()
     -> NorthCalib_Init()
  -> AutoDrive_LinkAliveTick()
  -> AutoDrive_Poll()
  -> NorthCalib_Poll()

合法协议帧:
  0x13 -> AutoDrive_SetReturnPositionRaw()
  0x14 -> AutoDrive_SetFishPositionRaw()
  0x15 -> AutoDrive_SetSwitchRaw()

低电 / 链路超时:
  -> AutoDrive_TriggerReturn()

D 键长按:
  -> NorthCalib_RequestStart()
  -> ShipControl_RequestGpsAlign(0)
  -> ShipControl_RequestGpsNav(0, low_speed)
  -> EEPROM 保存 north_offset_cd
```

说明：

- `AutoDrive` 已进入当前主链，不是“预留未接入模块”。
- 它仍服从旧协议格式和当前门控条件，不是全自动导航系统。
- 航向基准由 `MainLoop_GetHeadingDeg100()` 统一输出；若做过 D 键校准，该接口会自动叠加 `north_offset_cd`。
- D 键北向校准的现场测试和对接判据见 `doc/project_doc/gps_north_calibration_d_key_test_plan.md`。

## 当前对外接口

```c
void AutoDrive_Init(void);
void AutoDrive_Poll(void);
void AutoDrive_Stop(void);
void AutoDrive_StopMotion(void);
void AutoDrive_TriggerReturn(void);
void AutoDrive_WorkOvertimeFail(void);

void AutoDrive_SetMode(u8 mode);
u8 AutoDrive_GetMode(void);
u8 AutoDrive_InActive(void);
u8 AutoDrive_IsBusy(void);
u8 AutoDrive_IsCanActive(const AutoDrive_PointRaw_t *point);

void AutoDrive_SetReturnPositionRaw(const u8 *data_m);
u8 AutoDrive_SetFishPositionRaw(const u8 *data_m);
void AutoDrive_SetSwitchRaw(const u8 *data_m, u8 len);
void AutoDrive_GetStoredConfig(AutoDrive_ReturnConfig_t *cfg);
void AutoDrive_GetCurrentPointRaw(AutoDrive_PointRaw_t *point);
u8 AutoDrive_GetFishPositionByIndexRaw(u8 index, AutoDrive_PointRaw_t *point);
u8 AutoDrive_GetLastFishCommandIndex(void);

void AutoDrive_LinkAliveTick(void);
void AutoDrive_LinkAliveKick(void);

void NorthCalib_Init(void);
void NorthCalib_UpdateRemoteInput(u8 lr, u8 ud, u8 key, u32 now_ms);
u8 NorthCalib_RequestStart(void);
void NorthCalib_Poll(void);
u8 NorthCalib_IsBusy(void);
int16 NorthCalib_GetHeadingOffsetCd(void);
```

## 当前 D 键北向校准

`NorthCalib.c/.h` 负责 D 键 GPS 北向校准，属于自动驾驶相关的独立小模块，不改变 `AutoDrive` 去点/返航状态机。

当前流程：

```text
长按 D 约 1.5s
  -> CHECK_READY 检查 GPS、heading、遥控链路、AutoDrive/巡航空闲
  -> ALIGN_NORTH 调 ShipControl_RequestGpsAlign(0)
  -> RUN_STRAIGHT 调 ShipControl_RequestGpsNav(0, 500) 直跑约 10m
  -> CALC 用 GPS 起终点航迹角 - 原始融合航向均值计算 north_offset_cd
  -> SAVE 写入 STC EEPROM/IAP
```

运行时使用方式：

- `MainLoop_GetRawHeadingDeg100()` 返回 AHRS 原始融合航向。
- `MainLoop_GetHeadingDeg100()` 返回 `raw + NorthCalib_GetHeadingOffsetCd()` 后的航向，供手动 yaw 自稳、E 键定速巡航、GPS 自动巡航共用。
- 直跑采样时以首个原始航向为参考累计最短角差，再还原平均值，避免跨 `0/360°` 或 `±180°` 时把样本算到反向。
- 校准期间人工打杆、遥控超时、GPS/heading 不 ready、直跑距离不足或 yaw 不稳定都会失败并停船。
- 校准 busy 时协议层会拒绝 `0x13/0x14/0x15` 自动巡航命令，并暂停低电自动返航触发，避免 AutoDrive 抢控制权。
- 新 offset 与已保存 offset 相差超过 `45.00°` 时只临时应用并打印 `offset jump`，不覆盖 EEPROM。

EEPROM 记录当前使用 A/B 双槽：`0x000200` 和 `0x000400`。单条记录包含 magic、version、`north_offset_cd`、confidence、sample distance、update count 和 checksum；加载时选择更新的有效槽，保存时写入另一个槽，写入失败不会擦掉旧有效记录。

现场测试时优先按 `doc/project_doc/gps_north_calibration_d_key_test_plan.md` 执行。该文档已经把标准水面测试、人工接管退出、GPS 不 ready、AutoDrive busy 隔离、offset 跳变保护和重启加载复测拆开，方便硬件、上位机和后续维护人员对接。

## 当前状态机

`autodrive.h` 当前状态定义：

- `AUTO_DRIVE_IDLE`
- `AUTO_DRIVE_START`
- `AUTO_DRIVE_GET_DIRECTION`
- `AUTO_DRIVE_RUNING`

模式定义：

- `AUTO_DRIVE_CLOSE`
- `AUTO_DRIVE_GO_FISISH_POSITION`
- `AUTO_DRIVE_GO_HOME_POSITION`

主流程：

```text
IDLE
  -> START
  -> GET_DIRECTION  // 大角度偏差时先用 ShipControl 原地对准
  -> RUNING
  -> 到点 / 超时 / 失败
  -> CLOSE + STOP
```

说明：`AUTO_DRIVE_GET_DIRECTION` 不再执行老工程那种“定时左/右转修正”；它在进入前进巡航前先调用 `ShipControl_RequestGpsAlign(target)` 原地低油门对准目标航向，对准后才进入 `RUNING`。

## 当前 GPS 对准机制

GPS 去钓点或返航启动时，状态机会先进入 `AUTO_DRIVE_GET_DIRECTION` 对准阶段，再进入 `AUTO_DRIVE_RUNING` 前进阶段。

对准阶段当前参数：

- 对准角度容差：`AUTODRIVE_ALIGN_TOLERANCE_CD = 1000`，即 `±10.00°`
- 对准放行前必须先“过零”：航向误差进入 `AUTODRIVE_ALIGN_ZERO_CROSS_CD = 50`，即 `±0.50°`，或误差符号从正到负/从负到正穿过 0
- 连续稳定计数：`AUTODRIVE_ALIGN_STABLE_TICKS = 20`，以 `10ms` 轮询计算，约 `200ms`
- 对准超时：`AUTODRIVE_ALIGN_TIMEOUT_TICKS = 800`，约 `8s`，超时后放行进入巡航，避免浪、磁环境或 PID 抖动导致一直卡在原地
- 对准阶段不使用前进基础速度，调用 `ShipControl_RequestGpsAlign()`，只允许原地差速转向

也就是说，系统不会在第一次刚摸到 `±8°` 窗口时立刻启动前进；必须先确认船头已经扫过目标航向附近，然后才开始累计 `±8°` 稳定时间。这样可以避免还没真正对准就提前进入 `RUNING`。

控制层对准阶段单独使用较软的 PID：

- `SHIP_GPS_ALIGN_KP_Q10 = 512`
- `SHIP_GPS_ALIGN_KI_Q10 = 0`
- `SHIP_GPS_ALIGN_KD_Q10 = 96`

注意：PID 输出里的 `SHIP_YAW_HOLD_OUTPUT_LIMIT = 1000` 是内部归一化控制量，不是 PWM duty，也不是最终电机命令。对准阶段最终差速命令会再限制为 `SHIP_MOTOR_OUTPUT_MAX_COMMAND * 18%`，当前 `SHIP_MOTOR_OUTPUT_MAX_COMMAND = 850`，所以原地对准最大差速约为 `153`。

进入 `RUNING` 后，`AutoDrive` 切回 `ShipControl_RequestGpsNav(target, base_speed)`，使用正常 GPS 导航 yaw-hold PID 和距离减速逻辑。

## 当前激活条件

点位能否真正进入自动驾驶，由 `AutoDrive_IsCanActive()` 决定。

当前门限：

- 目标点经度方向必须是 `E` 或 `W`
- 目标点经度整数段不能为 `0`
- 当前状态必须是 `AUTO_DRIVE_IDLE`
- GPS 必须可用
- 卫星数至少 `7`
- 当前经纬度不能为 `0`
- 当前点到目标点距离必须 `> 10m` 且 `< 800m`

对应代码常量：

- `AUTODRIVE_MIN_ACTIVE_DISTANCE_M = 10`
- `AUTODRIVE_MAX_ACTIVE_DISTANCE_M = 800`

## 当前航向来源

`AutoDrive` 不再自己实现 yaw PID 和左右电机极性，而是调用 `ShipControl_RequestGpsNav()`，复用统一控制层已经验证过的 yaw-hold 链路。

当前航向来源：

1. `MainLoop_GetHeadingDeg100()`
   条件：`MainLoop_IsHeadingReady() != 0`
2. 航向不可用时停止电机，不做开环直行

说明：

- GPS 用来给目标点算目标航向和到点距离。
- `ShipControl` 的 yaw-hold 自稳链路负责 PID、左右极性、差速限幅、陀螺阻尼和输出斜坡。
- 当前已经去掉老工程“定时左/右转修正”，不再用 `turn_times` 这种倒计时转向。

## 当前电机输出

`AutoDrive` 不直接输出左右差速；正常巡航由 `ShipControl_RequestGpsNav()` 进入统一控制层，航向自稳不可用时停止电机。

当前关键常量：

- `AUTODRIVE_CRUISE_BASE_SPEED = 850`
- `SHIP_YAW_HOLD_PERIOD_MS = 50`
- `SHIP_YAW_HOLD_DEADBAND_CD`
- `SHIP_YAW_HOLD_OUTPUT_LIMIT`
- `SHIP_YAW_HOLD_OUTPUT_SIGN`
- `AUTODRIVE_ARRIVE_DISTANCE_M = 3`
- `AUTODRIVE_WORK_OVERTIME = 10 * 60 * 100`

含义：

- 基础巡航速度为 `850`
- 每 `50ms` 按自稳模式做一次航向修正
- 左右电机极性严格跟随遥控自稳模式，不在 AutoDrive 内单独判断
- 距离目标点小于 `3m` 判定到点
- 运行超时会退出并置失败标志

## 当前链路保活与返航触发

`ship_protocol.c` 会在每次遥控合法命令后调用 `AutoDrive_LinkAliveKick()`。

`AutoDrive_LinkAliveTick()` 以约 `10ms` 节拍运行，当前门限：

- `AUTODRIVE_MANUAL_TIMEOUT_TICKS = 3000`
- `AUTODRIVE_MANUAL_CLOSE_TICKS = 300`

行为：

- 长时间未收到链路保活时，调用 `AutoDrive_TriggerReturn()`
- 关闭倒计时结束且当前不在自动驾驶模式时，执行 `AutoDrive_StopMotion()`

另外，`ship_protocol.c` 的低电逻辑也会触发返航入口：

```text
power_level == 0
AutoDrive_GetMode() == AUTO_DRIVE_CLOSE
ShipControl_GetManualAccelerator() < 10
```

## 当前协议格式

`AutoDrive` 接收的是旧遥控器固定点位格式。

点位 10 字节：

```text
lon_dir
lon_whole[BE]
lon_frac[BE]
lat_dir
lat_whole[BE]
lat_frac[BE]
```

对应命令：

- `0x13` 设置返航点到 RAM，并在条件满足时尝试返航
- `0x14` 接收钓点坐标，按收到顺序自动分配到 `1..5` 号 RAM 钓点
- `0x15` 更新 RAM 中的自动返航开关，若长度至少 `11` 字节，同时更新 RAM 返航点；开关不为 `0x30` 时立即尝试返航

`0x14` 钓点鉴别逻辑：

- 最多保存 5 个钓点，编号按遥控器先后发来的顺序自动分配为 `1..5`。
- 不要求一次收满 5 个钓点；只有 1 号钓点有效时，也可以正常去 1 号。
- 第一次收到未知有效坐标时会保存到下一个空槽，并同时按当前收到坐标尝试进入去钓点流程。
- 同一坐标重复发送时不会重复占用槽位，但仍会按该坐标尝试进入去钓点流程。
- 已保存坐标再次收到时会匹配对应编号，并按该坐标尝试进入去钓点流程。
- 已保存的钓点在本次上电期间一直保留；去过一次、到点停车、手动停止、超时失败或切换模式都不会删除 1..5 表。
- 5 个槽位已满后，未匹配任何已保存钓点的新坐标不会写入 1..5 表，但仍会作为本次临时目标尝试前往；`idx=0` 表示未入库。
- `AutoDrive_GetLastFishCommandIndex()` 记录最近一次 `0x14` 保存或匹配到的钓点编号；`AutoDrive_GetLastFishSaveResult()` 记录保存结果，供无线日志打印。

## 当前配置存储

当前自动返航配置是 RAM-only。
`AutoDriveCfg_Load()` / `AutoDriveCfg_Save()` 仍保留接口，但只读写 `autodrive_cfg.c` 内部 RAM 变量，不再擦写 STC flash/EEPROM。

默认配置：

- `auto_ret_onoff = 0x30`
- 返航点全零

注意：

- `0x13`、`0x15` 更新的返航点和开关掉电后不会保留。
- 钓点列表也只保存在 RAM 中，本次上电期间不会因去点/到点/停车而清空；复位或重新上电后清空。

## 当前已知边界

- `AutoDrive` 只有在 `ShipProtocol_RunScheduler()` 运行时才会被初始化和轮询
- 如果无线模块整体关闭，当前工程不会单独在主循环里调用 `AutoDrive_Poll()`
- 自动驾驶可进入不等于一定能稳定跑直线，仍依赖 GPS、AHRS、磁环境和水面扰动
- 当前 README 只描述根目录真实代码，不描述旧镜像工程的理想行为

## 相关文件

- `Code_boweny/Device/AutoDrive/autodrive.h`
- `Code_boweny/Device/AutoDrive/autodrive.c`
- `Code_boweny/Device/AutoDrive/autodrive_cfg.h`
- `Code_boweny/Device/AutoDrive/autodrive_cfg.c`
- `Code_boweny/Device/WIRELESS/ship_protocol.c`
- `doc/project_doc/total.md`
- `doc/project_doc/date.md`
