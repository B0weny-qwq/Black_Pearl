# Black Pearl v1.1

## 0. 快速找目录

如果只是想快速定位文件，先按下面这张表找：

| 想找什么 | 去哪里看 | 主要文件 |
| --- | --- | --- |
| 主循环、任务调度、总开关 | `User/` | `Main.c`, `MainLoop.c`, `Task.c`, `FeatureSwitch.h`, `Config.h` |
| 无线遥控、配对、船控协议 | `Code_boweny/Device/WIRELESS/` | `wireless.*`, `ship_protocol.*`, `lt8920.*` |
| 手动控制、yaw 自稳、定速巡航仲裁 | `Code_boweny/Device/Control/` | `ShipControl.*` |
| 自动驾驶、返航、钓点 | `Code_boweny/Device/AutoDrive/` | `autodrive.*`, `autodrive_cfg.*` |
| GPS 解析和定位状态 | `Code_boweny/Device/GPS/` | `GPS.*` |
| 电机输出 | `Code_boweny/Device/Motor/` | `Motor.*` |
| 磁力计 QMC6309 | `Code_boweny/Device/QMC6309/` | `QMC6309.*`, `QMC6309_port.*` |
| IMU QMI8658 | `Code_boweny/Device/QMI8658/` | `QMI8658.*`, `QMI8658_port.*` |
| AHRS / 航向融合 | `Code_boweny/Function/AHRS/` | `AHRS.*`, `HeadingEstimator.*` |
| PID、滤波、日志 | `Code_boweny/Function/` | `PID/`, `Filter/`, `Log/` |
| STC32G 底层驱动 | `Driver/` | `inc/`, `src/`, `isr/`, `lib/` |
| 板级外设例程/封装 | `App/` | `inc/`, `src/` |
| Keil 工程和编译产物 | `RVMDK/` | `STC32G-LIB.uvproj`, `list/` |
| 串口日志上位机 | `tools/ship_log_viewer/` | `ship_log_viewer.html`, `start_ship_log_viewer.*` |
| 协议包生成工具 | `tools/ship_packet_builder/` | `build_ship_packet.py` |
| 工程说明文档 | `doc/project_doc/` | `total.md`, `date.md` |
| 模块构建/调试文档 | `doc/build_doc/` | `README_GPS.md`, `README_wireless.md`, `IMU_QMI8658.md` |

推荐查找顺序：先看本 README 的目录索引，再看对应模块的 `README.md` 和 `.h` 注释，最后再进 `.c` 实现。

当前文档口径：

- `.h` 重点描述接口、参数单位和行为边界。
- 各模块 `README.md` 重点描述真实接入关系和运行链路。
- `doc/project_doc/total.md` 记录当前运行档。
- `doc/project_doc/date.md` 记录每次代码/注释/文档变更原因。

### GPS 自动巡航 / 自动调整船头方向去哪里找

这部分不要只看 `GPS/`。`GPS/` 只负责定位数据，真正“去点、算方向、调船头、写电机”分散在下面几层：

```text
无线协议触发
  Code_boweny/Device/WIRELESS/ship_protocol.c
    0x13/0x14/0x15 -> AutoDrive_Set...
    ShipProtocol_RunScheduler() -> AutoDrive_Poll()

GPS 点位和目标航向计算
  Code_boweny/Device/AutoDrive/autodrive.c
    AutoDrive_Poll()
    AutoDrive_UpdateTargetHeading()
    AutoDrive_ApplyAlignHeadingHold()
    AutoDrive_ApplyHeadingHold()
  Code_boweny/Device/AutoDrive/NorthCalib.c
    D 键长按 GPS 北向校准
    EEPROM A/B 双槽保存 north_offset_cd

船头自动修正 / yaw-hold / 差速输出
  Code_boweny/Device/Control/ShipControl.c
    ShipControl_RequestGpsAlign()
    ShipControl_RequestGpsNav()
    ShipControl_ApplyYawHoldTargetEx()
    ShipControl_YawControlToSpeed()
    Motor_SetBothSpeed()

当前船头角来源
  User/MainLoop.c
    IMU_AhrsPoll()
    MainLoop_GetRawHeadingDeg100()
    MainLoop_IsHeadingReady()
    MainLoop_GetHeadingDeg100()

航向融合算法
  Code_boweny/Function/AHRS/HeadingEstimator.c
    Heading_Update()
    Heading_GetDeg100()

GPS 原始定位来源
  Code_boweny/Device/GPS/GPS.c
    GPS_Poll()
    lat_deg1e7 / lon_deg1e7 / update_sequence
```

按问题查文件：

- “遥控器发去钓点/返航命令后怎么进自动巡航”：看 `ship_protocol.c` 的 `0x13`、`0x14`、`0x15` 分支和 `ShipProtocol_RunScheduler()`。
- “当前 GPS 点到目标点，目标航向怎么算”：看 `autodrive.c` 的 `AutoDrive_UpdateTargetHeading()`。
- “为什么先原地转船头再往前跑”：看 `autodrive.c` 的 `AUTO_DRIVE_GET_DIRECTION`、`AutoDrive_ApplyAlignHeadingHold()` 和 `ShipControl_RequestGpsAlign()`。
- “巡航中怎么持续调整船头方向”：看 `autodrive.c` 的 `AutoDrive_ApplyHeadingHold()`，它会调用 `ShipControl_RequestGpsNav(target_heading, base_speed)`。
- “D 键北向校准怎么触发和保存”：看 `ship_protocol.c` 的 D 键长按检测、`NorthCalib.c` 的 `CHECK_READY -> ALIGN_NORTH -> RUN_STRAIGHT -> CALC -> SAVE`，以及 `STC32G_EEPROM.c` 的 IAP 读写接口。
- “D 键北向校准怎么给别人测和对接”：看 `doc/project_doc/gps_north_calibration_d_key_test_plan.md`，里面按现场准备、标准测试、日志判据、失败原因和回退方式写。
- “PID 怎么把航向误差变成左右电机差速”：看 `ShipControl.c` 的 `ShipControl_ApplyYawHoldTargetEx()`、`ShipControl_ApplyYawHoldDamping()`、`ShipControl_YawControlToSpeed()`。
- “最终左右电机命令在哪里写”：看 `ShipControl.c` 里调用 `Motor_SetBothSpeed()` 的位置，再追到 `Code_boweny/Device/Motor/Motor.c`。
- “当前船头角从哪里来”：看 `User/MainLoop.c` 的 `IMU_AhrsPoll()`、`MainLoop_GetRawHeadingDeg100()`、`MainLoop_GetHeadingDeg100()`，再追 `HeadingEstimator.c`；`MainLoop_GetHeadingDeg100()` 会叠加北向校准保存的 `north_offset_cd`。
- “GPS 坐标和更新序号哪里来”：看 `GPS.c` 的 `GPS_Poll()`、`lat_deg1e7`、`lon_deg1e7`、`update_sequence`。

---

## 1. 交付时先看这里

这个工程对外交付时，优先给对方这几个入口：

- 上位机目录：
  [tools/ship_log_viewer/](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer)
- 上位机详细使用说明：
  [tools/ship_log_viewer/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/README.md)
- 上位机页面：
  [tools/ship_log_viewer/ship_log_viewer.html](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/ship_log_viewer.html)
- Windows 一键启动：
  [tools/ship_log_viewer/start_ship_log_viewer.bat](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/start_ship_log_viewer.bat)
- PowerShell 启动：
  [tools/ship_log_viewer/start_ship_log_viewer.ps1](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/start_ship_log_viewer.ps1)

工程文档主入口：

- 工程总览：
  [doc/project_doc/total.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)
- 开发日志 / 变更记录：
  [doc/project_doc/date.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)
- D 键 GPS 北向校准测试对接：
  [doc/project_doc/gps_north_calibration_d_key_test_plan.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/gps_north_calibration_d_key_test_plan.md)

---

## 2. 上位机怎么用

上位机的具体使用、页面卡片含义、串口连接方式、日志识别规则、现场排错顺序，不在本 README 里重复展开。

直接看：

- [tools/ship_log_viewer/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/README.md)

如果只是现场打开：

1. 双击 [tools/ship_log_viewer/start_ship_log_viewer.bat](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/start_ship_log_viewer.bat)
2. 选择串口
3. 点连接
4. 先看“配对状态”和“遥控链路”
5. 再看“遥控输入油门”和“实际输出油门”

---

## 3. 维护时先看哪里

维护这个工程，不要先盲改 `.c`。

推荐顺序：

1. 先看工程总览：
   [doc/project_doc/total.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)
2. 再看最近变更：
   [doc/project_doc/date.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)
3. 再看对应模块 `.h` 文件里的中文注释和 Doxygen 说明
4. 最后再改实现文件

原因很简单：

- `total.md` 说明当前真实运行档和开关状态
- `date.md` 说明为什么这么改过
- `.h` 注释说明模块接口、宏、参数、行为边界

---

## 4. 配置在哪里改

这个工程的维护原则是：

- 先找对应 `.h`
- 具体配置说明直接看对应 `.h` 里的中文注释
- 不要靠猜，不要只看 `.c`

### 4.1 总开关和功能开关

主配置入口：

- [User/FeatureSwitch.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/User/FeatureSwitch.h)

这里集中管理：

- 无线开关
- GPS 开关
- MAG 开关
- IMU / AHRS 开关
- 船控协议调度开关
- 手动 yaw 自稳开关
- yaw hold 参数
- 日志开关
- 兼容行为开关

具体每个宏是什么意思，直接看 `FeatureSwitch.h` 顶部和分组注释。

### 4.2 自动驾驶 / 返航 / 存储配置

看：

- [Code_boweny/Device/AutoDrive/autodrive.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/AutoDrive/autodrive.h)
- [Code_boweny/Device/AutoDrive/autodrive_cfg.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/AutoDrive/autodrive_cfg.h)

这里包含：

- 自动驾驶模式
- 返航点 / 目标点
- 保存与加载
- Flash/EEPROM 配置口径

关键行为：

- `0x13/0x14/0x15` 只负责写入返航点、钓点或自动返航配置，不直接下发固定航向角。
- 当前维护 5 个 RAM 钓点表和 1 个当前前往目标 `g_fish_position`；新的有效 `0x14` 会按顺序保存/匹配 1..5，同时也作为本次当前目标尝试前往。
- 钓点只在 RAM 中，断电丢失；5 个槽位满后新坐标不再入库，但仍可作为临时目标尝试前往。自动返航配置通过 `0x15` 保存到 RAM 配置接口，当前不会写 flash。
- 进入返航/钓点巡航后，`AutoDrive_Poll()` 在 GPS `update_sequence` 变化时用“当前 GPS 点 -> 目标点”重新计算 `target_heading_cd`，并持续提交给 `ShipControl_RequestGpsNav()`。
- GPS 没有新点的 10ms 控制周期内，yaw-hold PID 会继续使用上一次 GPS 计算出的目标航向；一旦 GPS 更新，目标航向立即重算。
- 当前船头角来自 `MainLoop_GetHeadingDeg100()` 的融合绝对航向，GPS 定点巡航禁止退回定时左/右转逻辑。
- 钓点/返航激活要求当前 GPS 可用、卫星数满足条件、目标距离大于 10m 且小于 800m；到目标 3m 内认为到达并停止。
- 前往钓点会先原地对准目标航向，再前进；运行中靠近目标会减速：20m 外速度 850，8m 到 20m 插值降速，3m 到 8m 低速爬行，3m 内停止。

### 4.3 船控协议 / 遥控器协议

看：

- [Code_boweny/Device/WIRELESS/ship_protocol.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.h)
- [Code_boweny/Device/WIRELESS/wireless.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/wireless.h)
- [Code_boweny/Device/WIRELESS/lt8920.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/lt8920.h)

这里包含：

- 配对
- 工作信道
- `0x11/0x12/0x13/0x14/0x15`
- 遥控在线状态
- GPS 回传

实际手动开环、手动 yaw 自稳、定速巡航和 GPS 航向保持统一由
`Code_boweny/Device/Control/ShipControl.*` 仲裁并最终写入电机目标。

当前手动 yaw 自稳降速逻辑：

- 只有进入 yaw 自稳后才会按航向误差压低基础速度。
- 偏航误差超过 `10.00°` 才开始降速，超过 `20.00°` 才达到满降速。
- 满降速时基础速度最低压到 `500`；小于 `10.00°` 的航向波动不再触发基速降档。

当前遥控 E 键定速巡航逻辑：

- 进入：按下 E，并且遥控油门输入 `ud - 100 >= 60`。
- 启动保护：进入时左右杆必须基本回中，`abs(lr - 100) <= 8`；船体 Z 轴角速度也要基本稳定，`abs(gyro_z) <= 8dps`。如果船还在转，固件会拒绝进入定速，避免一锁航向就先画弧。
- 退出：巡航中再次按 E，或油门输入 `ud - 100 <= -50`。
- 进入时锁定当前 `MainLoop_GetHeadingDeg100()` 为目标航向，当前请求基础速度为 `760`。
- 定速巡航起步有软启动：`base` 约从 `520` 在 `1.8s` 内线性拉到 `760`，减少刚进入时的弧线。
- 定速巡航不走 GPS/yaw 大角度靠近减速的基速降档；只叠加左右差速修正。高速时差速不再被电机上行余量卡死，必要时通过压低一侧电机来获得转向力。
- 巡航运行时不使用 GPS 定位；控制反馈来自 IMU/AHRS 航向，运动中主要靠 gyro 积分保持船头方向。

巡航相关 `[DATA]` 日志：

- `[DATA] I: cruise enter input=... raw_ud=... speed=760 hd=... start_th=60`
- `[DATA] I: cruise run req=... base=... l=... r=... err=... pid=... diff=... tgt=...`
- `[DATA] I: cruise exit reason=key-toggle ...`
- `[DATA] I: cruise exit reason=throttle ... stop_th=-50`
- `[DATA] I: cruise ignore input=... raw_ud=... need=60`
- `[DATA] I: cruise ignore reason=not-straight input=... steer=... gyro=... raw_ud=... raw_lr=...`

其中 `req` 是巡航请求速度，当前为 `760`；`base` 是控制层实际使用的基础速度，起步软启动时会从约 `520` 逐步升到 `760`；`l/r` 是最终写给左右电机的命令，`err/pid/diff/tgt` 用来看船头自稳修正量。

### 4.4 姿态 / 航向 / Heading

看：

- [Code_boweny/Function/AHRS/AHRS.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Function/AHRS/AHRS.h)
- [Code_boweny/Function/AHRS/HeadingEstimator.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Function/AHRS/HeadingEstimator.h)

这里包含：

- AHRS 状态定义
- HeadingEstimator 接口
- yaw / gyro / mag 相关参数说明

### 4.5 PID

看：

- [Code_boweny/Function/PID/PID.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Function/PID/PID.h)

### 4.6 电机 / PWM

看：

- [Code_boweny/Device/Motor/Motor.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/Motor/Motor.h)

### 4.7 GPS

看：

- [Code_boweny/Device/GPS/GPS.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/GPS/GPS.h)

### 4.8 磁力计 / IMU

看：

- [Code_boweny/Device/QMC6309/QMC6309.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/QMC6309/QMC6309.h)
- [Code_boweny/Device/QMI8658/QMI8658.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/QMI8658/QMI8658.h)

---

## 5. 维护时怎么做

建议统一按这个流程维护：

1. 先确认需求是“改功能”还是“改参数”
2. 如果只是改参数，先改对应 `.h` 或 `FeatureSwitch.h`
3. 改之前先看对应 `.h` 注释，确认宏和接口语义
4. 改完同步更新：
   [doc/project_doc/total.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)
   和
   [doc/project_doc/date.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)
5. 如果日志格式改了，也要同步更新：
   [tools/ship_log_viewer/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/README.md)
   和
   [tools/ship_log_viewer/ship_log_viewer.html](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/ship_log_viewer.html)

维护规则就一句话：

- 具体含义先查对应 `.h` 的注释

---

## 6. 工程整体目录

## 6.1 主要目录

```text
Black_Pearl_v1.1/
├─ User/                         主程序入口、主循环、任务调度、项目级配置
├─ Code_boweny/
│  ├─ Device/                    业务设备层：无线、GPS、电机、IMU、磁力计、控制、自动驾驶
│  └─ Function/                  通用功能层：AHRS、PID、Filter、Log
├─ Driver/                       STC32G 底层驱动、ISR、库文件
├─ App/                          板级外设应用封装和示例
├─ RVMDK/                        Keil 工程文件和编译输出
├─ tools/                        PC 调试工具和辅助脚本
├─ doc/                          项目文档、模块文档、调试记录
├─ .vscode/                      VS Code / Keil Assistant 配置
├─ README.md                     中文主说明
└─ README.zh-CN.md               中文 README 跳转说明
```

## 6.2 二级目录速查

### `User/`

- `Main.c`：程序入口
- `MainLoop.c/.h`：主循环和运行态数据
- `Task.c/.h`：任务调度
- `FeatureSwitch.h`：功能总开关
- `Config.h`：项目配置
- `System_init.c/.h`：系统初始化

### `Code_boweny/Device/`

- `AutoDrive/`：自动驾驶、返航、钓点和保存配置
- `Control/`：船控仲裁、手动控制、yaw 自稳、定速巡航、GPS 航向保持
- `GPS/`：GPS 数据解析和定位状态
- `Motor/`：左右电机输出
- `QMC6309/`：磁力计驱动和移植层
- `QMI8658/`：IMU 驱动和移植层
- `WIRELESS/`：无线芯片、配对、遥控协议、船控协议

### `Code_boweny/Function/`

- `AHRS/`：姿态解算、航向估计
- `Filter/`：滤波工具
- `Log/`：日志输出
- `PID/`：PID 控制器

### `doc/`

- `project_doc/`：工程总览、开发日志、控制策略、AHRS 报告
- `build_doc/`：GPS、无线、IMU、磁力计、日志等模块说明
- `return_home_logic/`：返航逻辑说明
- `tools/`：文档侧工具说明和旧版日志查看器

### `tools/`

- `ship_log_viewer/`：串口日志上位机，现场联调优先看这里
- `ship_packet_builder/`：船控协议包生成脚本

## 6.3 当前最常改的文件

- [User/FeatureSwitch.h](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/User/FeatureSwitch.h)
- [User/MainLoop.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/User/MainLoop.c)
- [Code_boweny/Device/WIRELESS/ship_protocol.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.c)
- [Code_boweny/Device/AutoDrive/autodrive.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/AutoDrive/autodrive.c)
- [Code_boweny/Function/AHRS/AHRS.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Function/AHRS/AHRS.c)
- [Code_boweny/Function/AHRS/HeadingEstimator.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Function/AHRS/HeadingEstimator.c)
- [Code_boweny/Device/Motor/Motor.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/Motor/Motor.c)
- [tools/ship_log_viewer/ship_log_viewer.html](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/ship_log_viewer.html)

---

## 7. 当前工程功能范围

当前工作区主功能包括：

- 无线遥控配对与控制
- 旧遥控器协议兼容
- `0x12` GPS 状态回传
- `0x13/0x14/0x15` 点位收发
- GPS 定位与航向
- 磁力计、IMU、AHRS、HeadingEstimator
- 手动差速控制
- 手动直线 yaw 自稳
- 自动驾驶 / 返航配置保存
- 串口日志上位机联调

具体运行档和开关状态，不以 README 为准，以：

- [doc/project_doc/total.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)

为准。

---

## 8. 最后一句

如果只是用工程：

- 先看上位机 README

如果是维护工程：

- 先看 `total.md`
- 再看 `date.md`
- 再查对应 `.h` 注释

不要跳过 `.h` 注释直接改实现。  
这个工程后续维护，默认就是按这个规则走。
