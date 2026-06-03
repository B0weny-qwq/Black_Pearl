# D 键 GPS 北向校准测试对接文档

记录日期：2026-06-01

适用版本：G1 最小侵入式实现，`NorthCalib.c/.h` 已接入 `ship_protocol.c` 调度链。

## 1. 对接目标

这份文档给测试、硬件、上位机和后续维护人员使用。目标是让对接方不用先读完整源码，也能知道：

- D 键校准怎么触发。
- 现场要准备什么条件。
- 日志里应该看哪些字段。
- 成功、失败、可疑结果分别怎么判断。
- EEPROM 保存和重启加载怎么复测。
- 对接其它模块时哪些接口可以用，哪些不要直接碰。

当前 G1 代码行为是：

```text
1s 内双击 D
  -> 检查 GPS / heading / 遥控 / AutoDrive 空闲
  -> 船先对准“系统认为的 0°”
  -> 低速直跑约 10m
  -> 用 GPS 起终点航迹角减去直跑期间原始融合航向均值
  -> 保存 north_offset_cd
  -> 停船退出
```

说明：算法本质上不要求必须真实朝北跑，只要稳定直线跑够距离，就可以通过 `gps_course - avg_raw_heading` 求出 heading 坐标系偏移。但当前 G1 为了现场操作和状态机简单，仍固定用 `0°` 作为校准目标方向。后续如果要改成任意方向直线校准，应另起一个小改动，不混在本轮测试里。

## 2. 相关文件

代码入口：

- `Code_boweny/Device/WIRELESS/ship_protocol.c`：D 键双击检测、E 键取消、遥控输入转发、busy 期间命令拦截。
- `Code_boweny/Device/AutoDrive/NorthCalib.c`：校准状态机、GPS 航迹角计算、EEPROM 保存。
- `Code_boweny/Device/AutoDrive/NorthCalib.h`：对外接口和失败原因枚举。
- `User/MainLoop.c`：`MainLoop_GetRawHeadingDeg100()` 与 `MainLoop_GetHeadingDeg100()`。
- `Code_boweny/Device/GPS/GPS.c`：GPS 解析状态、`lat_deg1e7/lon_deg1e7/update_sequence`。
- `Driver/src/STC32G_EEPROM.c`：STC EEPROM/IAP 读写接口。

文档入口：

- `doc/project_doc/gps_north_calibration_d_key.md`：设计方案和 G1 实现记录。
- `Code_boweny/Device/AutoDrive/README.md`：AutoDrive 与 NorthCalib 接入关系。
- `Code_boweny/Device/GPS/README.md`：GPS 数据来源和职责边界。
- `doc/project_doc/date.md`：变更记录。

## 3. 测试环境准备

硬件和环境：

- 船体、电池、遥控器、GPS 模块、磁力计和 IMU 均已接好。
- 水面空旷，前方至少 `15m` 无障碍，避免岸边、桥下、树下和强磁环境。
- GPS 能稳定定位，卫星数建议不低于 `7`。
- 遥控链路在线，D 键手感正常。
- 船桨附近无人，测试前确认急停/人工接管方式。

软件和日志：

- 烧录包含 `NorthCalib.c/.h` 的固件。
- 打开串口日志或上位机，能看到 `[NCAL]` 日志。
- 建议同时记录 `0x12` 回传里的 GPS 状态、当前 heading、目标 heading 和距离。
- 测试前记录固件版本、日期、测试地点、天气/浪况和电池电压。

校准前状态：

- GPS ready。
- Heading ready。
- AutoDrive 没有正在去钓点/返航。
- E 键定速巡航未开启；校准过程中按 E 会主动退出本次校准。
- 遥控摇杆在中位附近，避免一进入校准就被判断为人工接管。

## 4. 关键参数和判据

当前 G1 参数：

| 项目 | 当前值 | 含义 |
| --- | --- | --- |
| D 键双击窗口 | `1000ms` | 1s 内双击 D 触发北向校准 |
| E 键取消 | 按键边沿 | 校准 busy 期间退出本次校准，不保存 |
| 对准目标 | `0°` | 系统认为的北向 |
| 对准稳定窗口 | `±5.00°` 连续约 `200ms` | 进入直跑前的放行条件 |
| 对准超时 | `8s` | 超时失败退出 |
| 直跑速度 | `500` | 低速直跑基础速度 |
| 目标距离 | `10m` | 达到后进入计算 |
| 直跑超时 | `30s` | 超时失败退出 |
| 总流程超时 | `45s` | 兜底失败退出 |
| 最小保存距离 | `8m` | 小于该距离失败，不保存 |
| 最小卫星数 | `7` | 小于该值失败 |
| offset 跳变保护 | `45.00°` | 与旧值差太大时不覆盖 EEPROM |
| EEPROM 槽位 | `0x000200` / `0x000400` | A/B 双槽保存 |

成功判据：

- 日志出现 `[NCAL] start`。
- 对准阶段有 `[NCAL] align heading=... err=...`。
- 直跑阶段有 `[NCAL] run dist=... heading=... gps=...`，距离逐步增加。
- 计算阶段有 `[NCAL] calc course=... avg_heading=... offset=... old=...`。
- 保存阶段有 `[NCAL] save ok offset=... conf=... dist=...`。
- 船自动停船退出。
- 重启后能看到 EEPROM 加载旧 offset 的日志，或后续自动巡航首航大弧线明显减小。

失败不应出现的行为：

- GPS/heading 不 ready 时仍然强行保存。
- 直跑不足 `8m` 仍保存。
- 人工打杆后仍继续写 EEPROM。
- 新旧 offset 差超过 `45.00°` 时覆盖旧槽。
- 校准 busy 时还能被 `0x13/0x14/0x15` 抢走控制权。

## 5. 标准水面测试流程

### T1：正常校准

目的：确认 D 键能完成一次完整校准并保存。

步骤：

1. 上电，等待遥控器连接。
2. 等待 GPS ready，卫星数不低于 `7`。
3. 等待 heading ready。
4. 把船放到空旷水面，确认前方至少 `15m` 无障碍。
5. 摇杆回中，在 `1s` 内双击 D。
6. 观察船是否先原地对准，再低速直跑。
7. 船跑到约 `10m` 后应自动停船。
8. 保存日志，记录 `course/avg_heading/offset/old/dist/conf`。

预期：

- 校准完成，日志出现 `save ok`。
- `dist` 应接近或大于 `10m`。
- `offset` 不应明显乱跳；同一场地连续两次测试建议差异控制在几度到十度以内，浪大或 GPS 抖动时可放宽，但不应几十度乱变。

### T2：重启加载

目的：确认 EEPROM 保存后能在重启后生效。

步骤：

1. 完成 T1 并确认 `save ok`。
2. 断电重启。
3. 观察启动日志。
4. 进入普通手动 yaw 自稳或自动巡航前，确认 `MainLoop_GetHeadingDeg100()` 已使用保存后的 offset。
5. 做一次去点/返航短距离测试，观察首航大弧线是否改善。

预期：

- 启动时加载 A/B 双槽中 `update_count` 更新且 checksum 有效的记录。
- 若 EEPROM 两槽都无效，应默认 `north_offset_cd = 0`，不能崩溃或乱用旧内存。

### T3：人工接管退出

目的：确认校准过程中按 E 或人工打杆能阻止保存。

步骤：

1. 按 T1 进入校准。
2. 在对准或直跑期间按 E；另一次复测中轻推前后或左右摇杆，超过中位死区。
3. 观察日志和船体行为。

预期：

- 按 E 时校准失败退出，原因应接近 `user cancel`；打杆时原因应接近 `manual override`。
- 船停止或回到安全控制状态。
- 不出现 `save ok`。
- 旧 EEPROM offset 不被覆盖。

### T4：GPS 不 ready 拒绝

目的：确认 GPS 条件不足时不会启动保存。

步骤：

1. 在室内、遮挡环境或 GPS 未稳定前双击 D。
2. 观察日志。

预期：

- 进入或尝试进入后快速失败，原因应接近 `GPS not ready`。
- 不直跑，不保存。

### T5：AutoDrive busy 隔离

目的：确认 NorthCalib 与自动巡航不会抢控制权。

步骤：

1. 先让 AutoDrive 进入去点或返航状态。
2. 双击 D。
3. 或在 NorthCalib busy 期间发送 `0x13/0x14/0x15`。

预期：

- AutoDrive busy 时 D 键校准被拒绝。
- NorthCalib busy 时 `0x13/0x14/0x15` 被协议层拒绝或延后，不应中断校准状态机。
- 校准期间低电自动返航触发被暂停，退出后恢复原有逻辑。

### T6：offset 跳变保护

目的：确认异常大偏移不会覆盖旧 EEPROM。

步骤：

1. 先完成一次可信校准并保存。
2. 在 GPS 多路径、强磁干扰、明显横漂或人为改变传感器方向的情况下再做一次校准。
3. 观察 `offset` 与 `old` 差值。

预期：

- 如果差值超过 `45.00°`，日志应出现 `offset jump` 相关失败。
- 本次 offset 可临时应用于当前运行，但不覆盖 EEPROM。
- 重启后仍加载旧可信 offset。

## 6. 日志字段说明

典型日志：

```text
[NCAL] start
[NCAL] align heading=... err=...
[NCAL] run dist=... heading=... gps=...
[NCAL] calc course=... avg_heading=... offset=... old=...
[NCAL] save ok offset=... conf=... dist=...
[NCAL] fail reason=...
```

字段含义：

| 字段 | 含义 | 判断方式 |
| --- | --- | --- |
| `heading` | 修正后的当前航向，单位 `0.01°` | 对准阶段应逐步靠近 `0°` |
| `dist` | GPS 起点到当前点距离，单位 m | 直跑阶段应逐步增加 |
| `gps` | 当前 GPS 经纬度原始定点值 | 应随 GPS 更新变化 |
| `course` | GPS 起点到终点航迹角，单位 `0.01°` | 代表真实运动方向 |
| `avg_heading` | 直跑期间原始融合航向均值，单位 `0.01°` | 用于和 GPS 航迹比较 |
| `offset` | 本次计算出的北向修正量 | `course - avg_heading` 包装到 `[-180°, 180°)` |
| `old` | EEPROM 当前生效旧 offset | 用于跳变保护 |
| `conf` | 本次保存置信度 | 距离达到目标时通常更高 |

人工核算公式：

```text
offset = wrap180(course - avg_heading)
corrected_heading = wrap360(raw_heading + offset)
```

注意：当前 `MainLoop_GetHeadingDeg100()` 输出的是修正后航向；校准计算内部会调用 `MainLoop_GetRawHeadingDeg100()` 取原始融合航向，避免旧 offset 参与本次平均值。

## 7. 失败原因排查表

| 现象 | 可能原因 | 处理建议 |
| --- | --- | --- |
| 双击 D 没反应 | 两次 D 超过 `1000ms` 窗口，或当前已有任务 busy | 重新在 1s 内双击，确认 AutoDrive/E 键巡航未运行 |
| `GPS not ready` | 未定位、卫星数不足、经纬度为 0 | 到空旷处等待定位稳定 |
| `heading not ready` | IMU/AHRS 未完成初始化或磁力计异常 | 先确认上位机 heading 是否稳定 |
| `remote timeout` | 遥控链路断开或输入快照超时 | 检查配对和遥控电量 |
| `user cancel` | 校准 busy 期间按下 E | 需要退出时使用；若误触，重新双击 D 开始 |
| `manual override` | 校准期间摇杆偏离中位 | 摇杆回中后重测 |
| `distance too short` | 没跑够最小保存距离 | 保证前方空间，避免中途接管 |
| `yaw unstable` | 浪太大、原地对准不稳、船体持续转动 | 换平稳水域，或后续执行 G2 对准增强 |
| `offset jump` | 新旧 offset 差过大 | 先按异常处理，不急着覆盖旧参数 |
| `EEPROM` | 写入或读回校验失败 | 检查 EEPROM/IAP 地址和供电稳定性 |

## 8. 对接边界

给其它模块使用时按下面边界：

- 自动巡航、E 键定速巡航、手动 yaw 自稳继续使用 `MainLoop_GetHeadingDeg100()`。
- 只有校准模块内部需要原始融合航向时，才使用 `MainLoop_GetRawHeadingDeg100()`。
- 不要在 GPS 模块里保存或修改北向 offset；GPS 模块只提供定位状态。
- 不要在 AutoDrive 去点状态机里直接读写 NorthCalib EEPROM 记录；运行时统一从 `MainLoop_GetHeadingDeg100()` 消费修正后的 heading。
- 对外只需要知道 `NorthCalib_IsBusy()` 可用于控制权隔离，`NorthCalib_GetHeadingOffsetCd()` 可用于诊断显示。
- EEPROM A/B 双槽是 NorthCalib 私有保存策略，外部模块不要直接改 `0x000200/0x000400`。

## 9. 测试记录模板

```text
测试编号：
日期时间：
测试地点：
固件版本 / commit：
测试人员：
水面情况：
GPS 卫星数：
是否首次校准：
old offset：
course：
avg_heading：
new offset：
dist：
confidence：
日志是否 save ok：
重启后是否加载：
自动巡航首航表现：
结论：
备注：
```

建议至少保留：

- 一次正常成功日志。
- 一次重启加载日志。
- 一次人工接管失败日志。
- 一次 GPS 不 ready 失败日志。

## 10. 回退方式

软件回退：

- 从 `ship_protocol.c` 移除 D 键双击触发、E 键取消、NorthCalib busy guard 和 `NorthCalib_Poll()` 调用。
- 从 `MainLoop.c/.h` 移除 `MainLoop_GetRawHeadingDeg100()` 对外接口，并让 `MainLoop_GetHeadingDeg100()` 直接返回 AHRS 融合航向。
- 从 Keil 工程移除 `NorthCalib.c/.h`。

现场回退：

- 旧 EEPROM 记录保留不会影响回退后固件，因为回退固件不再读取该 offset。
- 如果只是怀疑本次校准不准，优先重新做一次正常校准；不要直接修改 GPS 模块或 AutoDrive 目标航向逻辑。

