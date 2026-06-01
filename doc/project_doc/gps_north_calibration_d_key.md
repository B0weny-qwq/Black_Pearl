# D 键 GPS 北向校准方案

记录日期：2026-05-31

## 1. 背景

当前自动巡航的核心链路是：

```text
遥控器发送目标点
  -> 船端用当前 GPS 点和目标点计算目标航向
  -> 船头先原地对准目标航向
  -> 前进过程中持续用 yaw-hold 修正左右电机差速
```

这条链路本身是合理的，但它有一个前置条件：

```text
当前船头绝对航向必须可靠
```

当前船头绝对航向主要依赖磁力计给 HeadingEstimator 提供初始北向参考。现场现象说明这个北向参考不稳定：

- 首次启动去点时容易绕大弧线。
- 靠近目标点后，路径逐渐变直。
- 后面几次跑同一类任务时，看起来像系统自己纠正了方向。

当前代码中没有基于 GPS 航迹自动学习北向的逻辑。因此这个现象更可能是：

```text
启动时磁力计北向偏差较大
  -> 船以错误船头角起跑
  -> 跑起来后陀螺短时间保持较稳定
  -> GPS 目标航向持续重算
  -> 船逐步被拉回正确路线
```

为降低首航大弧线问题，需要引入一个简单、可控、可现场验证的北向校准流程。

## 2. 目标

第一版目标不是做完整磁力计标定，而是做一个低成本的“真北对齐”：

```text
磁力计提供粗略北
GPS 航迹验证真实北
陀螺保持短时航向连续
EEPROM 保存上一次可信北向修正
```

校准完成后，系统保存一个 `north_offset_cd`：

```text
修正后航向 = 原始融合航向 + north_offset_cd
```

其中 `north_offset_cd` 单位为 0.01 度，范围建议归一化到 `[-18000, 18000)`。

## 3. 用户操作

使用当前未绑定业务的 D 键作为入口。

建议交互：

```text
长按 D 约 1.5s - 2s
  -> 进入 GPS 北向校准
  -> 船自动朝自己认为的北方对准
  -> 船低速向前直跑约 10m
  -> 系统根据 GPS 起终点计算真实航迹角
  -> 校验通过后保存北向修正
  -> 停船退出
```

面向用户的简单说明可以是：

```text
将船放到空旷水面，确认前方至少 15m 无障碍。
长按 D 进入北向校准。
船会自动朝北慢速前进一小段，完成后自动停船。
```

## 4. 状态机建议

建议新增独立模块，例如：

```text
Code_boweny/Device/AutoDrive/NorthCalib.c
Code_boweny/Device/AutoDrive/NorthCalib.h
```

第一版状态机：

```text
IDLE
  D 长按触发
  -> CHECK_READY

CHECK_READY
  检查 GPS ready、heading ready、遥控链路在线、当前未自动巡航
  -> ALIGN_NORTH

ALIGN_NORTH
  ShipControl_RequestGpsAlign(0)
  航向误差进入阈值后
  -> RUN_STRAIGHT

RUN_STRAIGHT
  记录起点 GPS、起始航向
  ShipControl_RequestGpsNav(0, low_speed)
  直跑距离达到 10m
  -> CALC

CALC
  用 GPS 起点到终点计算真实航迹角
  计算 north_offset_cd
  校验质量
  -> SAVE 或 FAILED

SAVE
  写入 EEPROM
  停船
  -> DONE

FAILED
  停船
  不覆盖旧参数
  -> IDLE
```

## 5. 核心计算

### 5.1 GPS 航迹角

复用 `autodrive.c` 中当前点到目标点的 bearing 计算思路：

```text
gps_course_cd = bearing(start_gps, end_gps)
```

单位使用 0.01 度。

### 5.2 航向修正量

第一版建议记录直跑期间的平均原始航向：

```text
avg_heading_cd = average(MainLoop_GetHeadingDeg100())
north_offset_cd = wrap180(gps_course_cd - avg_heading_cd)
```

如果校准动作强制目标航向为 0 度，理论上也可以简化理解为：

```text
north_offset_cd ≈ gps_course_cd
```

但保存时仍建议使用 `gps_course_cd - avg_heading_cd`，这样后续可以扩展为任意方向校准，而不只限于北向。

## 6. 质量门控

不能只要跑够距离就保存，否则错误 GPS 或横漂会污染 EEPROM。

建议第一版门控：

- GPS 可用，卫星数不少于 7。
- 起终点距离不少于 8m，目标距离建议 10m。
- 校准期间遥控器没有人工打杆接管。
- 校准期间 yaw 角速度不能长期过大。
- 校准期间航向误差不能长期过大。
- 船必须处于前进状态，不能原地转圈。
- 计算出的 offset 必须在合理范围内。
- 如果新 offset 与旧 offset 差异超过 45 度，第一版建议只临时使用并打印日志，不立即覆盖 EEPROM。

## 7. 保存策略

现有 `AutoDriveCfg_Save()` 当前只是 RAM 保存，不是真正写 flash/EEPROM。北向校准需要使用 STC 内部 EEPROM/IAP 接口：

```text
EEPROM_read_n()
EEPROM_write_n()
EEPROM_SectorErase()
```

建议独立保存结构：

```c
typedef struct
{
    u32 magic;
    u8 version;
    int16 north_offset_cd;
    u8 confidence;
    u16 sample_distance_m;
    u16 update_count;
    u16 checksum;
} NorthCalib_Record_t;
```

建议字段：

- `magic`：识别有效记录。
- `version`：结构版本。
- `north_offset_cd`：北向修正量，单位 0.01 度。
- `confidence`：本次校准质量评分。
- `sample_distance_m`：本次校准使用的 GPS 航迹距离。
- `update_count`：成功保存次数。
- `checksum`：防止 EEPROM 数据损坏。

保存原则：

- 校准失败不覆盖旧参数。
- EEPROM 校验失败时使用默认 `north_offset_cd = 0`。
- 不要高频写 EEPROM。
- 一次 D 键校准最多保存一次。
- 后续如果做自动学习，也必须限制写入频率。

## 8. 运行时使用方式

建议在航向输出处统一应用修正，而不是让 AutoDrive、ShipControl 各自处理。

理想入口：

```text
MainLoop_GetHeadingDeg100()
```

内部返回：

```text
wrap360(Heading_GetDeg100(&g_heading) + north_offset_cd)
```

这样调用方不需要知道北向修正是否存在：

- 手动 yaw 自稳继续使用同一个接口。
- E 键定速巡航继续使用同一个接口。
- GPS 自动巡航继续使用同一个接口。
- AutoDrive debug snapshot 也自然显示修正后的航向。

如果担心影响手动自稳，第一版也可以只给 GPS 自动巡航使用一个新接口：

```c
u16 MainLoop_GetNavHeadingDeg100(void);
```

但长期看，统一修正绝对航向更清晰。

## 9. 与现有自动巡航的关系

D 键北向校准不是替代自动巡航，而是给自动巡航提供更可靠的 heading 坐标系。

当前自动巡航仍保持：

```text
目标点 bearing -> target_heading_cd
当前 heading -> yaw error
yaw-hold PID -> 左右电机差速
```

北向校准只修正其中的：

```text
当前 heading
```

所以它的作用是减少“目标航向正确，但当前航向坐标系偏了”导致的首航大弧线。

## 10. 日志建议

建议添加关键日志，便于现场判断是否校准成功：

```text
[NCAL] start
[NCAL] align heading=... err=...
[NCAL] run dist=... heading=... gps=...
[NCAL] calc course=... avg_heading=... offset=... old=...
[NCAL] save ok offset=... conf=... dist=...
[NCAL] fail reason=...
```

失败原因建议枚举：

- GPS not ready
- heading not ready
- manual override
- distance too short
- yaw unstable
- offset jump too large
- EEPROM write failed

## 11. 现场验证

详细测试对接文档见：`doc/project_doc/gps_north_calibration_d_key_test_plan.md`。

岸上准备：

1. 确认 D 键当前没有其他业务。
2. 确认 GPS 状态回传正常。
3. 确认 heading ready 后再开始。
4. 确认前方至少 15m 空旷。

水面测试：

1. 上电，等待遥控器连接。
2. 等待 GPS ready 和 heading ready。
3. 长按 D 进入北向校准。
4. 观察船是否先原地对准，再低速直跑。
5. 跑够约 10m 后自动停船。
6. 查看日志中的 `course`、`avg_heading`、`offset`。
7. 重启设备。
8. 直接执行去点/返航，观察首航大弧线是否明显改善。

更细的测试拆分：

- T1 正常校准：确认 `start -> align -> run -> calc -> save ok` 全链路。
- T2 重启加载：确认 EEPROM A/B 双槽记录可被重新加载。
- T3 人工接管退出：确认校准期间打杆不保存。
- T4 GPS 不 ready 拒绝：确认定位不足时不直跑、不保存。
- T5 AutoDrive busy 隔离：确认校准和去点/返航不抢控制权。
- T6 offset 跳变保护：确认新旧 offset 差超过 `45.00°` 时不覆盖旧 EEPROM。

判断标准：

- 保存后的 offset 稳定，不应每次相差几十度。
- 重启后首次自动巡航路径应明显更直。
- 校准失败时不应覆盖旧参数。
- D 键校准期间人工打杆应退出或失败，不应继续写入参数。

## 12. 后续扩展

第一版只做 D 键手动触发校准。

后续可以扩展：

- 自动巡航成功直线段中，低频更新 north_offset。
- 保存多次 offset 的滑动平均。
- 结合磁力计场强质量，决定是否信任磁力计。
- 增加上位机显示当前 north_offset 和校准质量。
- 将磁力计硬铁/软铁校准与 GPS 北向校准分开保存。

核心原则保持不变：

```text
磁力计负责粗北
GPS 航迹负责验北
EEPROM 负责记住可信修正
陀螺负责短时间连续性
```

## 13. 连续实现目标清单

本节按可连续执行的 goal 组织，后续实现时可以从 `G1` 开始逐项推进。每个 goal 都包含现象、目标、建议改动、验收标准和风险边界。

### G1：D 键北向校准主流程

现象：

- 首次自动巡航依赖磁力计粗北，容易绕大弧线。
- 后续路径变直，但不是代码真的学习了北向。

目标：

- 长按 D 触发一次可控的 GPS 北向校准。
- 船自动对准自认为的北方，直跑约 10m。
- 用 GPS 航迹角修正 heading 坐标系。
- 校验通过后保存 `north_offset_cd`。

建议改动：

- 新增 `NorthCalib.c/.h`。
- 在 `ship_protocol.c` 中对 D 键做长按检测，建议 `1500ms - 2000ms` 触发。
- 状态机使用 `CHECK_READY -> ALIGN_NORTH -> RUN_STRAIGHT -> CALC -> SAVE`。
- 校准期间人工打杆立即退出或失败。

验收标准：

- 长按 D 后进入校准，短按 D 不触发。
- 校准失败不覆盖旧参数。
- 成功后日志能看到 `course`、`avg_heading`、`offset`、`save ok`。
- 重启后自动巡航首航大弧线明显减小。

风险边界：

- 不在 GPS 不 ready 时启动。
- 不在 heading 不 ready 时启动。
- 不在普通自动巡航或定速巡航正在运行时强行插入。

### G2：原地对准增强，抗波浪

现象：

- 自动巡航或北向校准进入原地对准时，左右差速有时干不过波浪。
- 船头迟迟转不到目标角度，导致起跑方向仍然偏。

目标：

- 原地对准阶段转向力更强。
- 只增强 GPS 对准阶段，不明显影响普通手动驾驶手感。

当前相关参数：

```text
ShipControl.c
  SHIP_GPS_ALIGN_DIFF_PERCENT = 18
  SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP = 30
```

建议改动：

- 将 `SHIP_GPS_ALIGN_DIFF_PERCENT` 从 `18` 提高到 `25 - 30`。
- 对 GPS align 阶段单独允许更大的差速限幅。
- 如仍然转不动，再考虑对 align 阶段单独提高 yaw 输出变化步长，不直接影响普通巡航。

验收标准：

- 船在小浪中能明显更快完成原地对准。
- 对准阶段不出现持续来回大幅过冲。
- 对准完成后进入直跑时，左右电机输出能平稳切换。

风险边界：

- 不建议直接全局提高普通 yaw-hold 增益。
- 不建议把手动开环转向也一起增强。
- 对准差速过大可能导致过冲，需要现场看日志中的 `err/pid/diff`。

### G3：E 键定速巡航更容易进入，速度略提高

现象：

- 当前 E 键定速巡航进入门槛偏高。
- 巡航速度偏保守。
- 起步爬升偏慢，用户感觉不够干脆。

目标：

- 降低进入定速巡航的油门门槛。
- 略提高定速巡航请求速度。
- 加快进入巡航后的速度爬升。

当前相关参数：

```text
ship_protocol.c
  SHIP_CRUISE_KEY_START_INPUT = 60
  SHIP_CRUISE_KEY_SPEED = 760

ShipControl.c
  SHIP_CRUISE_RAMP_MS = 1800
  SHIP_CRUISE_RAMP_MIN_BASE = 520
```

建议改动：

- `SHIP_CRUISE_KEY_START_INPUT`：`60 -> 45` 或 `50`。
- `SHIP_CRUISE_KEY_SPEED`：`760 -> 800`，现场动力足够再试 `820`。
- `SHIP_CRUISE_RAMP_MS`：`1800ms -> 1200ms`。
- `SHIP_CRUISE_RAMP_MIN_BASE`：`520 -> 560` 或 `580`。

如果后续改成离散步进式软启动，则每步增加量应加大；当前代码是按时间线性插值，因此第一版用缩短 ramp 时间和提高起始 base 来达到“每次增加步长更大”的效果。

验收标准：

- 中等前进油门即可进入 E 键定速巡航。
- 进入巡航后 1 秒左右能明显达到稳定速度。
- 不因进入门槛降低导致误触发。
- 船还在明显旋转时仍应拒绝进入巡航。

风险边界：

- `SHIP_CRUISE_STEER_START_MAX` 和 `SHIP_CRUISE_GYRO_START_MAX_DPS` 不建议第一轮放宽。
- 先降低油门门槛和提高速度，不同时放开所有保护条件。

### G4：遥控器转向灵敏度降低 20%

现象：

- 当前手动转向太敏感，小幅左右杆就容易给出过大的差速。
- 现场操控不够细腻，容易把船头打过。

目标：

- 只降低遥控器左右转向灵敏度约 20%。
- 不降低前后油门响应。
- 不影响 GPS 自动巡航的闭环修正能力。

当前相关参数：

```text
ShipControl.c
  SHIP_STEERING_MAX_COMMAND = 700
  SHIP_STEERING_DEADBAND = 8
```

建议改动：

- `SHIP_STEERING_MAX_COMMAND`：`700 -> 560`。
- 第一轮先不改 `SHIP_THROTTLE_MAX_COMMAND`。
- 如中位附近仍敏感，再把 `SHIP_STEERING_DEADBAND` 从 `8 -> 10`。

验收标准：

- 手动小幅转向更柔和。
- 满杆仍能完成有效转向。
- 手动前进直线时更不容易被轻微左右杆扰动带偏。
- 自动巡航和 GPS align 的差速能力不被这个改动削弱。

风险边界：

- 不要用降低总电机输出的方式解决转向灵敏度。
- 不要把 GPS align 的转向力和手动转向灵敏度绑到同一个参数上。

### G5：参数分组与可回退

现象：

- 现在多个体验问题都集中在 `ShipControl.c` 和 `ship_protocol.c` 的宏参数上。
- 如果一次性改太多，现场不好判断哪个参数带来了改善或副作用。

目标：

- 每一组参数改动都可以单独验证。
- 现场失败时能快速回退。

建议分组：

```text
Group A：北向校准功能
  D 长按
  NorthCalib 状态机
  EEPROM 保存

Group B：GPS 原地对准增强
  SHIP_GPS_ALIGN_DIFF_PERCENT
  align 专用输出限制

Group C：E 键定速巡航体验
  SHIP_CRUISE_KEY_START_INPUT
  SHIP_CRUISE_KEY_SPEED
  SHIP_CRUISE_RAMP_MS
  SHIP_CRUISE_RAMP_MIN_BASE

Group D：手动转向灵敏度
  SHIP_STEERING_MAX_COMMAND
  SHIP_STEERING_DEADBAND
```

验收顺序：

1. 先验证 Group D，确认遥控手感变柔和。
2. 再验证 Group B，确认原地对准能抗波浪。
3. 再验证 Group C，确认定速巡航更容易进入且起步更干脆。
4. 最后验证 Group A，因为北向校准需要水面直跑空间和日志观察。

风险边界：

- 每轮只改一个 group。
- 每轮都保留日志截图或串口记录。
- 如果出现明显过冲、误触发、无法转向，优先回退最近一组参数。

## 14. Goal 跑任务格式

后续执行时，每个任务建议按下面格式记录，方便连续推进：

```text
Goal:
  G2 原地对准增强，抗波浪。

Change:
  SHIP_GPS_ALIGN_DIFF_PERCENT 18 -> 25。

Verify:
  1. 水面小浪环境进入 GPS align。
  2. 观察是否能转到目标角度。
  3. 观察是否过冲。
  4. 记录 CTRL 日志 err/pid/diff。

Pass:
  5 秒内能完成对准，且不持续过冲。

Rollback:
  恢复 SHIP_GPS_ALIGN_DIFF_PERCENT = 18。
```

建议每个 goal 完成后再进入下一个 goal，避免多个体验变化互相干扰。

## 14. G1 实现记录

实现日期：2026-05-31

本次已按 G1 做最小侵入式实现，只引入 D 键北向校准主流程，不同时调整 G2/G3/G4/G5 参数。

### 已落地代码

- 新增 `Code_boweny/Device/AutoDrive/NorthCalib.c/.h`，独立维护北向校准状态机。
- `ship_protocol.c`：
  - D 键短按仍无业务动作。
  - D 键保持约 `1500ms` 后调用 `NorthCalib_RequestStart()`。
  - 校准 busy 时拦截手动电机更新，并拒绝 `0x13/0x14/0x15` 自动巡航命令，避免抢控制权。
  - `ShipProtocol_RunScheduler()` 继续按 10ms 节拍调用 `AutoDrive_Poll()`，随后调用 `NorthCalib_Poll()`。
- `User/MainLoop.c/.h`：
  - 新增 `MainLoop_GetRawHeadingDeg100()`，供校准计算原始融合航向。
  - `MainLoop_GetHeadingDeg100()` 统一返回 `raw + NorthCalib_GetHeadingOffsetCd()` 后的航向。
- `RVMDK/STC32G-LIB.uvproj`：
  - 加入 `NorthCalib.c/.h`。
  - include path 增加 `Code_boweny/Device/AutoDrive`。

### 当前状态机

```text
IDLE
  D 长按触发
  -> CHECK_READY

CHECK_READY
  GPS ready / heading ready / 遥控在线 / AutoDrive 空闲 / ShipControl 非自动模式
  -> ALIGN_NORTH

ALIGN_NORTH
  ShipControl_RequestGpsAlign(0)
  航向误差连续约 200ms 进入 ±5.00°
  -> RUN_STRAIGHT

RUN_STRAIGHT
  记录 GPS 起点，以首个原始航向为参考持续累计最短角差
  ShipControl_RequestGpsNav(0, 500)
  GPS 起终点距离达到约 10m
  -> CALC

CALC
  gps_course_cd = bearing(start_gps, end_gps)
  avg_heading_cd = 参考首样本还原后的原始融合航向平均值
  north_offset_cd = wrap180(gps_course_cd - avg_heading_cd)
  -> SAVE 或 FAILED

SAVE
  EEPROM 写入 magic/version/offset/confidence/distance/update_count/checksum
  写后读回校验
  停船退出
```

### EEPROM 策略

- A/B 双槽地址：`0x000200`、`0x000400`。
- 单槽记录大小：16 字节。
- 加载时选择 `update_count` 更新且 checksum 有效的槽。
- 保存时写入另一个槽，写后读回校验；写入失败时旧有效槽仍保留。
- 新 offset 与已保存 offset 相差超过 `45.00°` 时，只临时应用本次 offset，并以 `offset jump` 失败退出，不写 EEPROM。
- 上电读取 EEPROM 校验失败时使用默认 `north_offset_cd = 0`。

### 日志

当前关键日志：

```text
[NCAL] start
[NCAL] align heading=... err=...
[NCAL] run dist=... heading=... gps=...
[NCAL] calc course=... avg_heading=... offset=... old=...
[NCAL] save ok offset=... conf=... dist=...
[NCAL] fail reason=...
```

### 回退

如现场需要回退 G1：

- 从 `ship_protocol.c` 移除 `NorthCalib` include、D 键长按检测、busy guard 和 `NorthCalib_Poll()` 调用。
- 从 `MainLoop.c/.h` 移除 `MainLoop_GetRawHeadingDeg100()` 对外接口，并让 `MainLoop_GetHeadingDeg100()` 直接返回 `Heading_GetDeg100()`。
- 从 `RVMDK/STC32G-LIB.uvproj` 移除 `NorthCalib.c/.h`。
- 保留 EEPROM 中旧记录不会影响回退后运行，因为没有代码再读取该 offset。

## 15. Graphify 优先执行规程

本项目已经有 `graphify-out/` 关系图。后续执行本文档中的 goal 时，默认先读关系图，再读少量源文件，避免每次把 README、GRAPH_REPORT 或大段源码全部塞进上下文。

固定入口：

```text
C:\AgentHarness\bin\graphify.ps1
```

推荐顺序：

```text
1. 先 query 当前 goal
2. 不够再 explain 关键概念或符号
3. 需要跨模块关系时再 path
4. 最后只打开命中的少数源文件
5. 改完代码后 update .
```

常用命令：

```powershell
C:\AgentHarness\bin\graphify.ps1 query "G2 原地对准增强 SHIP_GPS_ALIGN_DIFF_PERCENT ShipControl_RequestGpsAlign"
C:\AgentHarness\bin\graphify.ps1 explain "ShipControl_RequestGpsAlign"
C:\AgentHarness\bin\graphify.ps1 path "ShipProtocol_HandleKey" "ShipControl_RequestCruise"
C:\AgentHarness\bin\graphify.ps1 update .
```

执行约束：

- 有 `graphify-out/graph.json` 时，先用 `query`，不要直接全文搜索全仓库。
- 只有做 broad architecture review 时才读 `GRAPH_REPORT.md`。
- 查询结果不够时再打开具体 `.c/.h/.md` 文件。
- 修改代码后必须运行 `update .`，保持图和源码同步。
- dirty 的 `graphify-out/` 文件是正常现象，不作为跳过 Graphify 的理由。

## 16. 文档与代码关系索引

为了减少缓存输入，后续 goal 只按需要读取下面的最小文件集合。

### G1：D 键北向校准

优先 Graphify 查询：

```text
G1 D键 北向校准 NorthCalib GPS 航迹 EEPROM ShipProtocol HandleKey
```

最小文件集合：

- `Code_boweny/Device/WIRELESS/ship_protocol.c`：D 键长按入口。
- `Code_boweny/Device/AutoDrive/autodrive.c`：GPS 点位、bearing、距离计算可复用逻辑。
- `Code_boweny/Device/Control/ShipControl.c`：`ShipControl_RequestGpsAlign()` 和 `ShipControl_RequestGpsNav()`。
- `User/MainLoop.c`：`MainLoop_GetHeadingDeg100()` 和 heading ready。
- `Driver/src/STC32G_EEPROM.c`、`Driver/inc/STC32G_EEPROM.h`：EEPROM 读写接口。

相关文档：

- `doc/project_doc/gps_north_calibration_d_key.md`
- `doc/project_doc/gps_north_calibration_d_key_test_plan.md`
- `doc/project_doc/magnetometer_calibration_product_notes.md`
- `Code_boweny/Function/AHRS/README.md`
- `doc/build_doc/README_GPS.md`

### G2：原地对准增强，抗波浪

优先 Graphify 查询：

```text
G2 原地对准 抗波浪 SHIP_GPS_ALIGN_DIFF_PERCENT ShipControl_RequestGpsAlign yaw output slew
```

最小文件集合：

- `Code_boweny/Device/Control/ShipControl.c`
- `Code_boweny/Device/AutoDrive/autodrive.c`

重点符号：

- `SHIP_GPS_ALIGN_DIFF_PERCENT`
- `SHIP_YAW_HOLD_DIFF_SLEW_PER_STEP`
- `ShipControl_RequestGpsAlign()`
- `ShipControl_LimitGpsAlignYawOutput()`
- `ShipControl_ApplyYawOutputSlew()`

### G3：E 键定速巡航体验

优先 Graphify 查询：

```text
G3 E键 定速巡航 进入门槛 速度 软启动 ShipProtocol_HandleKey ShipControl_RequestCruise
```

最小文件集合：

- `Code_boweny/Device/WIRELESS/ship_protocol.c`
- `Code_boweny/Device/Control/ShipControl.c`

重点符号：

- `SHIP_CRUISE_KEY_START_INPUT`
- `SHIP_CRUISE_KEY_SPEED`
- `SHIP_CRUISE_RAMP_MS`
- `SHIP_CRUISE_RAMP_MIN_BASE`
- `ShipProtocol_HandleKey()`
- `ShipControl_RequestCruise()`
- `ShipControl_ApplyCruiseBaseRamp()`

### G4：遥控器转向灵敏度降低

优先 Graphify 查询：

```text
G4 遥控器 转向灵敏度 SHIP_STEERING_MAX_COMMAND ShipControl_SteeringToSignedSpeed
```

最小文件集合：

- `Code_boweny/Device/Control/ShipControl.c`

重点符号：

- `SHIP_STEERING_MAX_COMMAND`
- `SHIP_STEERING_DEADBAND`
- `ShipControl_SteeringToSignedSpeed()`
- `ShipControl_ApplyAxisCurve()`

### G5：参数分组与回退

优先 Graphify 查询：

```text
G5 参数分组 回退 GPS align cruise steering ShipControl ship_protocol
```

最小文件集合：

- `doc/project_doc/gps_north_calibration_d_key.md`
- `Code_boweny/Device/Control/ShipControl.c`
- `Code_boweny/Device/WIRELESS/ship_protocol.c`

输出要求：

- 每次只改一个 group。
- 最终回复必须说明改了哪些参数。
- 最终回复必须说明如何回退。
- 有测试或编译结果时必须列出。

## 17. 低上下文执行模板

后续跑任何 goal 时，建议按这个低上下文模板推进：

```text
Step 1: Graphify query
  用当前 goal 的一句话查询图。

Step 2: Scope
  只列出需要打开的 2-5 个文件。

Step 3: Patch
  只改当前 goal 对应参数或模块。

Step 4: Verify
  编译、静态检查或至少做符号级检查。

Step 5: Graphify update
  运行 C:\AgentHarness\bin\graphify.ps1 update .

Step 6: Report
  用 Goal / Change / Verify / Rollback 格式报告。
```

示例：

```text
Goal:
  G4 遥控器转向灵敏度降低 20%。

Graphify:
  query "G4 遥控器 转向灵敏度 SHIP_STEERING_MAX_COMMAND"

Scope:
  只读 ShipControl.c。

Change:
  SHIP_STEERING_MAX_COMMAND 700 -> 560。

Verify:
  检查 ShipControl_SteeringToSignedSpeed() 仍只影响手动转向。

Rollback:
  恢复 SHIP_STEERING_MAX_COMMAND = 700。
```
