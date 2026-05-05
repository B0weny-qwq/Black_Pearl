# AHRS 姿态融合模块

`Code_boweny/Function/AHRS/` 提供面向 Black Pearl 船体控制的定点互补姿态融合模块。模块使用 QMI8658 六轴 IMU 和 QMC6309 地磁计，输出姿态角和角速度快照。

## 输出单位

| 字段 | 单位 |
|------|------|
| roll / pitch / yaw | `deg * 100` |
| gyro_x / gyro_y / gyro_z | `deg/s * 100` |
| dt | ms |

示例：`yaw_deg100 = 1234` 表示 `12.34°`。

## 默认坐标系

```text
+X = 船尾
+Y = 船右 / 右舷
+Z = 向上
```

如果实测方向与船体定义相反，可在 `AHRS.h` 中修改轴向映射宏：

- `AHRS_IMU_BODY_*_FROM`
- `AHRS_IMU_BODY_*_SIGN`
- `AHRS_MAG_BODY_*_FROM`
- `AHRS_MAG_BODY_*_SIGN`

## 调参建议

1. 先保持默认增益，确认原始轴向映射是否正确。
2. 如果船体 `+X` 方向反了，优先修改 `AHRS_IMU_BODY_X_SIGN`。
3. 如果修改了 QMI8658 陀螺仪量程，必须同步更新 `AHRS_GYRO_LSB_PER_DPS`。
4. 如果姿态响应过慢，可适当减小 `AHRS_ACC_BLEND_SHIFT`。
5. 如果波浪或震动明显进入姿态输出，可增大 `AHRS_ACC_BLEND_SHIFT` 或 `AHRS_ACC_ANGLE_LPF_SHIFT`。

## 地磁修正

地磁 yaw 修正有意设计为慢速修正，只用于抑制陀螺仪航向长期漂移。若现场磁干扰较强，可将 `AHRS_MAG_ENABLE` 设为 `0` 禁用地磁修正。

## 对外接口

```c
void AHRS_Reset(void);
s8 AHRS_Update6Axis(int16 ax, int16 ay, int16 az,
                    int16 gx, int16 gy, int16 gz,
                    u16 dt_ms);
s8 AHRS_UpdateRaw6Axis(int16 raw_ax, int16 raw_ay, int16 raw_az,
                       int16 raw_gx, int16 raw_gy, int16 raw_gz,
                       u16 dt_ms);
s8 AHRS_UpdateMag(int16 mx, int16 my, int16 mz);
s8 AHRS_UpdateRawMag(int16 raw_mx, int16 raw_my, int16 raw_mz);
const AHRS_State_t *AHRS_GetState(void);
u8 AHRS_IsReady(void);
```

## 注意事项

- 模块全程使用整数定点运算，不依赖 FPU。
- 当前默认 IMU 更新周期为 `17ms`。
- 重新初始化传感器后建议调用 `AHRS_Reset()` 清空历史滤波和零偏状态。
