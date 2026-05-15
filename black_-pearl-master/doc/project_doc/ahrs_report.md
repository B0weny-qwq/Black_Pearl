# Black Pearl v1.1 AHRS 融合报告

> 本文件是镜像说明。真实工程代码与真实工程文档以根目录为准。  
> 旧版“欧拉角分别积分 + `atan2(my,mx)` 磁修正”已经退役，不再代表当前实现。

## 1. 当前状态

当前工程已经接入：

- `QMI8658` 六轴 IMU
- `QMC6309` 三轴磁力计
- `Code_boweny/Function/AHRS` 四元数姿态融合
- `UART1` AHRS 调试日志

当前有效运行目标是：

- 恢复并稳定姿态融合链
- 让 `roll/pitch` 在静态和小动态下稳定可用
- 让 `yaw` 具备“短期靠陀螺、长期靠磁修正”的工程可用能力
- 保持现有日志格式和上位机兼容

## 2. 本轮架构变更

- 旧实现：欧拉角 `roll/pitch/yaw` 分别积分，再用加速度和磁力计做慢修正
- 新实现：Mahony 风格四元数传播 + accel/mag 误差反馈
- 外部 API、日志格式和 `AHRS_State_t` 保持不变
- AHRS 上下文迁到 `xdata`，避免 Keil C51 `EDATA` 溢出

## 3. 当前数据流

```text
QMI8658 raw acc/gyro
-> axis mapping
-> gyro bias learn
-> bias subtract
-> deadband + gyro LPF
-> quaternion propagation
-> accel gravity error feedback

QMC6309 filtered mag
-> axis mapping
-> mag LPF
-> normalization
-> tilt-compensated heading error feedback

quaternion
-> roll / pitch / yaw (deg * 100)
-> AHRS_State
-> UART1 AHRS logs
```

节拍固定为：

- IMU：`17ms`
- MAG：`100ms`

## 4. 当前标志位与日志

```text
0x01 READY
0x02 ACC_VALID
0x04 MAG_VALID
0x08 GYRO_BIAS_READY
0x10 ACC_REF_READY
0x20 DT_CLAMPED
```

- `f=17`：姿态输出已建立，但 `GYRO_BIAS_READY` 尚未完成
- `f=1F`：姿态、加速度参考、磁力计有效位、陀螺零偏都已 ready
- `g=`：当前三轴角速度
- `ys=`：yaw 锁零前稳定计数
- `yr=`：锁零后的相对 yaw
- `mv=0`：磁力计当前无效
- `me=0`：编译期关闭磁修正

## 5. 当前验收重点

1. 编译通过，确认 AHRS 路径重新进入链接。
2. 上电静置后 `flags` 从 `0x17` 进入 `0x1F`。
3. 平放时 `roll/pitch` 收敛到接近 0，`g=` 三轴接近 0。
4. `ys` 计满后出现 `yr=+0.00`。
5. 手动倾斜板子时 `roll/pitch` 方向正确，不再出现明显欧拉串轴。
6. 缓慢旋转船体时 `yaw` 连续变化，停止后不会像旧版那样长期单向漂移。

## 6. 当前剩余限制

1. 还没有做磁力计硬铁/软铁校准。
2. 绝对 yaw 已经加入倾斜补偿，但仍会受现场磁环境影响。
3. GPS 长期航向参考本轮没有接入。
4. PID、电机控制、自动航向保持本轮没有并入。
5. 四元数增益目前按工程调试默认值收口，后续仍可根据实船动态微调。

## 7. 下一阶段建议

1. 做磁力计偏置采集，至少补上硬铁校准。
2. 记录不同磁环境下的 `mv=0 / MAG_VALID` 表现，确认门控阈值。
3. 如果需要长期绝对航向，再把 GPS 作为低频参考接入。
4. 只有在姿态链稳定后，再把控制闭环接入。
