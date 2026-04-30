# Black Pearl v1.1 AHRS 融合汇报 README

> 本文档用于阶段汇报，说明当前 IMU + 磁力计姿态融合的实现方式、运行判断逻辑、已验证现象、当前限制和后续升级计划。

---

## 1. 当前结论

当前工程已经接入：

- QMI8658 六轴 IMU：三轴加速度 + 三轴陀螺仪
- QMC6309 三轴磁力计
- AHRS 定点互补滤波模块
- UART1 姿态调试日志

当前姿态输出包含：

```text
r = roll
p = pitch
y = yaw
yr = 相对 yaw，仅用于调试 90 度转向，不参与控制
```

当前 AHRS 能在 IMU 正常初始化后输出 `roll / pitch / yaw`，并通过 `flags` 判断融合链路状态。实测日志中已经出现：

```text
[AHRS] I: ... f=17
[AHRS] I: ... f=1F
```

其中 `f=1F` 表示 IMU、磁力计、加速度参考和陀螺零偏学习均已 ready。

当前算法定位：**工程验证级姿态融合**。它适合当前阶段验证外设、轴向、姿态趋势和相对转向，不是最终量产级导航算法。

---

## 2. 软件入口和数据链路

当前 AHRS 测试模式下主循环保留最小链路：

```text
Task_Pro_Handler_Callback()
IMU_HighRatePoll()
```

`IMU_HighRatePoll()` 做三件事：

1. 每约 `17ms` 读取一次 QMI8658 六轴数据。
2. 调用 `AHRS_UpdateRaw6Axis()` 更新 roll/pitch/yaw。
3. 每约 `100ms` 读取一次 QMC6309 磁力计，并调用 `AHRS_UpdateRawMag()` 慢修正 yaw。

整体链路：

```text
QMI8658 raw acc/gyro
-> 轴向映射
-> 陀螺零偏学习
-> 陀螺积分
-> 加速度修正 roll/pitch

QMC6309 raw mag
-> 轴向映射
-> 地磁 yaw 解算
-> 慢修正 yaw

AHRS_State
-> UART1 输出 r/p/y/yr/flags
```

---

## 3. 核心融合算法

当前实现是 **定点互补滤波**：

```text
短期动态：陀螺仪积分
长期稳定：加速度计修正 roll/pitch
航向参考：磁力计慢修正 yaw
```

代码中不使用浮点，角度统一用：

```text
deg * 100
```

例如：

```text
-753 = -7.53 deg
```

内部角度状态使用 Q8 定点格式保存，以减少低速积分时的整数截断。

---

## 4. roll / pitch 计算逻辑

加速度计提供重力方向。算法先估算当前加速度模长，并建立 1g 参考值：

```text
acc_norm = norm(ax, ay, az)
```

启动阶段采样 32 帧建立 `acc_1g_ref`。之后只有当当前加速度模长落在参考值附近时，才认为加速度可信：

```text
acc_valid = acc_norm 在 1g_ref ±35% 内
```

当 `acc_valid=1` 时，根据加速度解算 roll/pitch：

```text
acc_roll  = atan2(-ay, az)
acc_pitch = atan2(ax, sqrt_approx(ay^2 + az^2))
```

这里的 `atan2` 和 `sqrt` 都是整数近似函数，避免使用浮点。

最终 roll/pitch 不是直接使用加速度角，而是融合：

```text
roll  = roll  + gyro_x 积分
pitch = pitch + gyro_y 积分

roll  = roll  + (acc_roll  - roll)  / 32
pitch = pitch + (acc_pitch - pitch) / 32
```

含义：

- 陀螺仪负责快速变化。
- 加速度计负责长期拉回重力方向。
- `/32` 是当前修正强度，由 `AHRS_ACC_BLEND_SHIFT=5` 决定。

---

## 5. yaw 计算逻辑

yaw 的短期变化来自陀螺 Z 轴积分：

```text
yaw = yaw + gyro_z * dt
```

磁力计提供地磁航向参考。当前实现先对磁力计三轴做低通，再用水平面磁场计算 yaw：

```text
mag_yaw = atan2(my, mx)
```

然后慢慢修正 yaw：

```text
yaw = yaw + (mag_yaw - yaw) / 32
```

含义：

- 陀螺 Z 轴负责短期转向响应。
- 磁力计负责长期抑制 yaw 漂移。
- 修正很慢，目的是降低磁干扰对 yaw 的瞬时影响。

当前版本尚未做：

- 磁力计硬铁/软铁校准
- 倾斜补偿航向
- 电机磁干扰建模

所以 yaw 目前只用于趋势验证和调试，不建议直接作为最终控制航向。

---

## 6. 陀螺零偏学习逻辑

陀螺仪静止时会有零偏。当前算法会在启动静止阶段学习零偏：

条件：

```text
acc_valid = 1
gyro_x/y/z 均小于 8 deg/s
连续 128 帧满足条件
```

满足后，三轴陀螺平均值被保存为零偏：

```text
gyro_bias_x
gyro_bias_y
gyro_bias_z
```

之后每帧都先扣除零偏，再积分：

```text
gyro = gyro_raw - gyro_bias
```

当前阈值设置为 `8 deg/s`，原因是实测 QMI8658 静止时 X 轴曾出现约 `-5 deg/s` 的偏置。如果阈值保持早期的 `2 deg/s`，会导致零偏长期无法 ready，`flags` 卡在 `0x17`。

---

## 7. flags 判断逻辑

AHRS 状态通过 `flags` 输出：

```text
0x01 READY             姿态估计器已有有效输出
0x02 ACC_VALID         当前加速度模长可信
0x04 MAG_VALID         最近一次磁力计数据有效
0x08 GYRO_BIAS_READY   陀螺零偏学习完成
0x10 ACC_REF_READY     1g 加速度参考建立完成
0x20 DT_CLAMPED        本帧 dt 被钳位，说明主循环可能被阻塞
```

常见状态：

```text
0x17 = READY + ACC_VALID + MAG_VALID + ACC_REF_READY
```

说明 AHRS 有输出，但陀螺零偏还没学习完成。

```text
0x1F = READY + ACC_VALID + MAG_VALID + GYRO_BIAS_READY + ACC_REF_READY
```

说明当前阶段理想 ready 条件都满足。

---

## 8. UART 日志判断方式

启动后保持静止，日志可能先出现：

```text
[AHRS] I: r=+89.60 p=-33.73 y=-20.42 g=-3.96 -0.79 +1.83 f=17
```

含义：

- `r/p/y` 是当前绝对姿态角。
- `g=` 是扣零偏/滤波后的角速度，单位 `deg/s`。
- `f=17` 表示陀螺零偏还没 ready。

进入 `f=1F` 后，yaw 仍可能被磁力计慢慢拉动。为了避免相对 yaw 零点锁在启动收敛过程，现在不会立即输出 `yr`，而是输出：

```text
[AHRS] I: r=-5.80 p=+1.80 y=-120.00 ys=3 g=-0.10 +0.00 +0.03 f=1F
```

`ys` 是 yaw 稳定计数。当前要求：

```text
yaw 连续 6 个日志采样变化不超过 1.50 deg
```

满足后才锁定相对 yaw 零点，并输出：

```text
[AHRS] I: r=-5.80 p=+1.80 y=-123.00 yr=+0.00 f=1F
```

之后手动旋转 90 度，只看 `yr`：

```text
yr 约为 +90.00 或 -90.00
```

符号取决于安装方向和坐标约定。

---

## 9. 当前实测现象

已观察到的正常现象：

- QMC6309 可初始化，`CHIP_ID=0x90`。
- QMI8658 在旧 STC I2C 路径下可恢复读取。
- AHRS 可从 `f=17` 进入 `f=1F`。
- roll/pitch 会从启动初始值逐步收敛。
- yaw 会受磁力计慢修正影响，在启动后继续缓慢变化。

已观察到的问题：

- QMI8658 曾在分段 ACK 诊断路径下出现 `0x6A/0x6B DEVW_NACK`，同时 QMC6309 正常。这说明 I2C 主干和 QMC6309 可用，但 QMI8658 路径仍需要继续复核。
- 老 STC I2C 路径能让 IMU 跑起来，但它对 ACK 状态判断较弱，可能把某些总线异常表现成 `0xFF` 读值。
- yaw 收敛期间如果过早锁相对零点，会导致 `yr` 初值被污染，已通过 `ys` 稳定计数规避。

---

## 10. 当前限制

当前版本存在以下工程限制：

1. **不是完整四元数姿态算法**
   当前是欧拉角互补滤波，适合小角度、低动态调试。大角度快速运动时会有轴间耦合误差。

2. **磁力计未校准**
   没有硬铁/软铁校准。yaw 会受电机、船体金属、桌面金属和线缆电流影响。

3. **磁力计 yaw 没有倾斜补偿**
   当前 `mag_yaw=atan2(my,mx)` 只适合近似水平状态。板子大幅 roll/pitch 后，yaw 可信度下降。

4. **陀螺量程系数仍需最终确认**
   当前按 `2048 LSB/(deg/s)` 处理，来自当前 QMI8658 bring-up 配置假设。后续如果确认量程不同，必须同步修改 `AHRS_GYRO_LSB_PER_DPS`。

5. **I2C 路径仍需硬件级复核**
   由于 QMI8658 曾出现设备地址阶段无 ACK，后续需要用示波器或逻辑分析仪确认 SDA/SCL、CSB、SA0、VDDIO。

---

## 11. 后续升级路线

建议按风险优先级推进：

### 11.1 硬件与 I2C 稳定性

- 固定检查 QMI8658 `VDD / VDDIO / GND / CSB / SA0`。
- 用逻辑分析仪抓 `0x6A / 0x6B` ACK。
- 确认 QMI8658 是否必须固定 `CSB=VDDIO` 才进入 I2C 模式。
- 决定最终采用旧 STC I2C 路径，还是修正分段 ACK 诊断路径后统一。

### 11.2 坐标轴标定

- 静止放平时确认 roll/pitch 符号。
- 绕 X/Y/Z 单轴缓慢转动，确认对应轴角速度符号。
- 如方向不一致，只修改：

```text
AHRS_IMU_BODY_*_FROM / SIGN
AHRS_MAG_BODY_*_FROM / SIGN
```

### 11.3 陀螺量程和零偏标定

- 对固定角度旋转进行验证，例如手动旋转 90 度。
- 若 `yr` 系统性偏大/偏小，优先确认 `AHRS_GYRO_LSB_PER_DPS`。
- 增加启动静止校准结果日志，记录三轴 bias。

### 11.4 磁力计校准

- 增加最小/最大值采集，做硬铁 offset。
- 后续根据数据质量加入软铁比例校正。
- 增加磁场模长异常判断，磁干扰严重时暂停 yaw 修正。

### 11.5 倾斜补偿 yaw

当前磁力计 yaw 没有使用 roll/pitch 做倾斜补偿。升级后应先把磁场旋转到水平面，再计算 yaw。

### 11.6 升级为四元数融合

若后续需要更可靠的大角度姿态，建议从当前欧拉角互补滤波升级为：

- Mahony
- Madgwick
- 简化 EKF

升级目的：

- 减少欧拉角轴间耦合
- 支持更大姿态角
- 更自然地融合 gyro / acc / mag

### 11.7 控制系统接入

在接入船体控制前，应增加质量门控：

- `flags != 0x1F` 时不使用 yaw 控制。
- `DT_CLAMPED` 出现时降低姿态可信度。
- 磁力计无效时 yaw 只允许短时间陀螺积分，超过时间后降级。
- 姿态角突变时进入保护或重新锁零。

---

## 12. 阶段验收建议

当前阶段建议按以下标准验收：

1. 上电静止，QMI8658 能稳定读到 `WHO_AM_I=0x05`。
2. QMC6309 能稳定读到 `CHIP_ID=0x90`。
3. 静止若干秒后 AHRS 日志进入 `f=1F`。
4. `ys` 计满后出现 `yr=+0.00`。
5. 手动顺时针或逆时针转动 90 度，`yr` 接近 `+90.00` 或 `-90.00`。
6. 放回原方向后，`yr` 应回到接近 `0`。
7. roll/pitch 倾斜时方向正确，放平后能回到接近原值。

若以上条件满足，可以认为当前 AHRS 工程验证阶段通过；若要用于正式控制，还需要完成磁力计校准、倾斜补偿和硬件稳定性复核。

