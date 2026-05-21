# Black Pearl v1.1

## 1. 交付时先看这里

这个工程对外交付时，优先给对方这几个入口：

- 上位机目录：
  [tools/ship_log_viewer/](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer)
- 上位机详细使用说明：
  [tools/ship_log_viewer/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/README.md)
- 上位机页面：
  [tools/ship_log_viewer/ship_log_viewer.html](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/ship_log_viewer.html)
- 根目录一键启动：
  [start_ship_log_viewer.bat](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/start_ship_log_viewer.bat)
- PowerShell 启动：
  [start_ship_log_viewer.ps1](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/start_ship_log_viewer.ps1)

工程文档主入口：

- 工程总览：
  [doc/project_doc/total.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)
- 开发日志 / 变更记录：
  [doc/project_doc/date.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)

---

## 2. 上位机怎么用

上位机的具体使用、页面卡片含义、串口连接方式、日志识别规则、现场排错顺序，不在本 README 里重复展开。

直接看：

- [tools/ship_log_viewer/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/tools/ship_log_viewer/README.md)

如果只是现场打开：

1. 双击根目录 [start_ship_log_viewer.bat](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/start_ship_log_viewer.bat)
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
- 进入返航/钓点巡航后，`AutoDrive_Poll()` 在 GPS `update_sequence` 变化时用“当前 GPS 点 -> 目标点”重新计算 `target_heading_cd`，并持续提交给 `ShipControl_RequestGpsNav()`。
- GPS 没有新点的 10ms 控制周期内，yaw-hold PID 会继续使用上一次 GPS 计算出的目标航向；一旦 GPS 更新，目标航向立即重算。
- 当前船头角来自 `MainLoop_GetHeadingDeg100()` 的融合绝对航向，GPS 定点巡航禁止退回定时左/右转逻辑。

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

- `User/`
  主循环、FeatureSwitch、项目级入口配置
- `Code_boweny/Device/`
  外设和业务设备层
- `Code_boweny/Function/`
  AHRS、PID、Filter、Log 等功能模块
- `Driver/`
  STC32G 底层驱动
- `App/`
  板级外设应用封装
- `RVMDK/`
  Keil 工程、编译输出、map/list
- `tools/ship_log_viewer/`
  串口日志上位机
- `doc/project_doc/`
  项目总览、开发日志
- `doc/build_doc/`
  模块级设计/构建说明
- `ship_Gps_V2.1_20260406-115200/`
  旧版参考工程和协议对照资料

## 6.2 当前最常改的文件

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
