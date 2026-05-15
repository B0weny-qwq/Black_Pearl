# AHRS

`Code_boweny/Function/AHRS/` now implements a Mahony-style quaternion AHRS for
Black Pearl.

## Runtime interface

- Internal state: `q0/q1/q2/q3` float quaternion stored in `xdata`
- External output: `roll/pitch/yaw = deg * 100`
- Gyro output: `gyro_x/gyro_y/gyro_z = deg/s * 100`
- Public API: unchanged

```c
void AHRS_Reset(void);
s8 AHRS_UpdateRaw6Axis(int16 raw_ax, int16 raw_ay, int16 raw_az,
                       int16 raw_gx, int16 raw_gy, int16 raw_gz,
                       u16 dt_ms);
s8 AHRS_UpdateRawMag(int16 raw_mx, int16 raw_my, int16 raw_mz);
const AHRS_State_t *AHRS_GetState(void);
```

## Update flow

IMU path:

1. Raw axis mapping
2. Gyro bias learn and subtract
3. Gyro deadband and LPF
4. Quaternion propagation
5. Accel gravity error feedback when `acc_valid`

MAG path:

1. Raw axis mapping
2. MAG LPF
3. Vector normalization
4. Tilt-compensated heading error feedback

Default timing:

- IMU update: `17 ms`
- MAG update: `100 ms`

## Tuning entry points

1. Verify axis mapping first:
   `AHRS_IMU_BODY_*_FROM / SIGN`
   `AHRS_MAG_BODY_*_FROM / SIGN`
2. If QMI8658 gyro range changes, update `AHRS_GYRO_LSB_PER_DPS`.
3. If roll/pitch correction is too soft or too hard, retune:
   `AHRS_MAHONY_KP_ACC`
4. If yaw magnetic correction is too weak or too aggressive, retune:
   `AHRS_MAHONY_KP_MAG`
5. If magnetic environment is bad, set `AHRS_MAG_ENABLE=0` for gyro-only checks.

## Known limits

- No hard-iron / soft-iron calibration in this round
- Absolute yaw is usable for engineering debugging, not final navigation grade
- GPS is not fused into the AHRS chain in this round
