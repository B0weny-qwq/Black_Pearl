# AutoDrive 自动返航与点位巡航说明

`Code_boweny/Device/AutoDrive/` 是当前根目录工程里的旧版自动驾驶兼容层。
它不是独立主循环入口，而是由 `Code_boweny/Device/WIRELESS/ship_protocol.c`
在协议调度中初始化、喂狗和轮询。

## 当前接入位置

真实调用链：

```text
ShipProtocol_RunScheduler()
  -> ShipProtocol_InitRuntime()
     -> AutoDrive_Init()
  -> AutoDrive_LinkAliveTick()
  -> AutoDrive_Poll()

合法协议帧:
  0x13 -> AutoDrive_SetReturnPositionRaw()
  0x14 -> AutoDrive_SetFishPositionRaw()
  0x15 -> AutoDrive_SetSwitchRaw()

低电 / 链路超时:
  -> AutoDrive_TriggerReturn()
```

说明：

- `AutoDrive` 已进入当前主链，不是“预留未接入模块”。
- 但它仍服从旧协议格式和当前门控条件，不是全自动导航系统。

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
void AutoDrive_SetFishPositionRaw(const u8 *data_m);
void AutoDrive_SetSwitchRaw(const u8 *data_m, u8 len);
void AutoDrive_GetStoredConfig(AutoDrive_ReturnConfig_t *cfg);
void AutoDrive_GetCurrentPointRaw(AutoDrive_PointRaw_t *point);

void AutoDrive_LinkAliveTick(void);
void AutoDrive_LinkAliveKick(void);
```

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
  -> GET_DIRECTION
  -> RUNING
  -> 到点 / 超时 / 失败
  -> CLOSE + STOP
```

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

`AutoDrive_GetCurrentHeadingDeg()` 的优先级如下：

1. GPS 航向
   条件：`gps->course_deg_x100` 有效，且 `gps->speed_kmh_x100 >= 80`
2. AHRS 航向
   条件：`AHRS_IsReady() != 0`
3. 都不可用时返回 `65535`

说明：

- 也就是当前 AutoDrive 不是只靠 GPS，也不是只靠 AHRS。
- 高速移动时优先用 GPS 航向，低速或 GPS 航向不足时退回 AHRS yaw。

## 当前电机输出

`AutoDrive` 直接调用 `Motor_SetBothSpeed()` 输出双电机差速。

当前关键常量：

- `AUTODRIVE_DRIVE_PWM = 900`
- `AUTODRIVE_DRIVE_FAST_PWM = 1000`
- `AUTODRIVE_TURN_PWM = 850`
- `AUTODRIVE_ARRIVE_DISTANCE_M = 3`
- `AUTODRIVE_STRAIGHT_DISTANCE_X10_M = 15`
- `AUTODRIVE_WORK_OVERTIME = 10 * 60 * 100`

含义：

- 直行巡航使用 `900` 或 `1000`
- 转向差速使用 `850`
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
g_now_pwm_accelerator < 10
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

- `0x13` 设置返航点并尝试返航
- `0x14` 设置目标点并尝试去目标点
- `0x15` 保存自动返航开关，若长度至少 `11` 字节，同时保存返航点

## 当前配置存储

当前没有外置 EEPROM。
自动返航配置由 `autodrive_cfg.c` 写入 STC 内部 flash/EEPROM 区：

```c
#define AUTODRIVE_CFG_FLASH_ADDR 0x0001F800UL
```

当前镜像格式：

- `magic = 0x41554432`
- `version = 0x0002`
- `length`
- `checksum`
- `AutoDrive_ReturnConfig_t cfg`

默认配置：

- `auto_ret_onoff = 0x30`
- 返航点全零

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
