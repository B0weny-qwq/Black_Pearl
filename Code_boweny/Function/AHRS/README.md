# AHRS

`Code_boweny/Function/AHRS/` implements a fixed-point complementary attitude
fusion module for Black Pearl.

Recommended first tuning path:

1. Keep the default gains and verify raw axis mapping first.
2. If body +X is reversed, change `AHRS_IMU_BODY_X_SIGN` in `AHRS.h`.
3. If the QMI8658 gyro range changes, update `AHRS_GYRO_LSB_PER_DPS`.
4. If the boat feels too sluggish, lower `AHRS_ACC_BLEND_SHIFT`.
5. If wave vibration leaks into attitude, raise `AHRS_ACC_BLEND_SHIFT` or
   `AHRS_ACC_ANGLE_LPF_SHIFT`.

Runtime output is `deg * 100`.

Default IMU update period is 17 ms because the current QMI8658 bring-up
configuration uses `CTRL2/CTRL3 = 0x07`, which maps to 58 Hz in the local
driver header.

Body frame:

```text
+X = stern
+Y = starboard/right
+Z = up
```

The magnetometer yaw correction is intentionally slow and can be disabled by
setting `AHRS_MAG_ENABLE` to `0`.
