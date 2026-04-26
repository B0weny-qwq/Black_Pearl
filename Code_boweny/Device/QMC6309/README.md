# QMC6309 Driver Notes

## Summary

- Device: QMC6309 3-axis magnetometer
- Runtime path: `User/System_init.c` -> `Code_boweny/Device/QMC6309/QMC6309.c`
- Bus: STC32G hardware I2C on `P1.4(SDA) / P1.5(SCL)`
- Address probe order: primary `0x7C`, alt `0x0C`
- Current default config: `CONTROL_1=0x1B`, `CONTROL_2=0x10` (10Hz)

## Current Bring-Up Result

The current runtime chain has been verified to work:

- `Probe addr=0x7C write=0xF8: ACK`
- `CHIP_ID=0x90`
- `CTRL1=0x1B CTRL2=0x10`
- XYZ readback returns valid signed data

## Important Files

- `QMC6309.c`: driver implementation
- `QMC6309.h`: public constants and API
- `doc/device_doc/QMC6309.md`: device behavior and register notes
- `doc/project_doc/total.md`: project-level bug record and I2C XSFR notes

## Bug Record

### 1. XSFR / EAXFR issue

The STC32G hardware I2C registers are in the extended SFR area:

- `I2CCFG`
- `I2CMSCR`
- `I2CMSST`

If `EAXFR` is not enabled first, writes to these registers do not actually hit the I2C controller. The observed symptom was:

- logs stopped at the first probe attempt
- the code appeared to enter `Start()`
- the low-level `Wait()` never completed

Fix:

- call `EAXSFR()` at the start of `SYS_Init()`

### 2. P1.4 / P1.5 overwritten by App init

Earlier bring-up failures were once caused by `ADtoUART_init()` reconfiguring all `P1.x` pins to high-impedance input, which overwrote the hardware I2C pin mode for `P1.4/P1.5`.

Current project status:

- `APP_config()` no longer enables `ADtoUART_init()`
- `SYS_Init()` still keeps `Sensor_I2C_prepare()` after `APP_config()` as a defensive restore step
- the shared I2C bus is therefore recovered explicitly before `QMC6309_Init()` and `QMI8658_Init()`

Fix:

- restore `P1.4/P1.5` to open-drain with pull-up after `APP_config()`
- restore `I2C_SW(I2C_P14_P15)`
- re-run `I2C_config()`

### 3. Misleading small-integer logs

Earlier debug logs showed values like `257`, which were not real GPIO/I2C states. That came from unsafe small-integer varargs formatting in the logging path.

Fix:

- avoid logging `bit` states as numeric `%u`
- use `H/L`, `Y/N`, or widened integer values

## Current Logging

The driver now keeps the useful logs and removes most redundant startup noise.

Kept:

- address probe result
- selected address
- ready / chip-id result
- control register readback
- bus recovery / error logs
- successful `WriteReg` data logs

Removed:

- temporary `TEST` logs in `Main.c`
- one-off dump/read self-test logs from `Main.c`
- some repetitive init debug logs

## APIs

Public APIs:

- `QMC6309_Init()`
- `QMC6309_ReadXYZ()`
- `QMC6309_ReadXYZFiltered()`
- `QMC6309_ReadID()`
- `QMC6309_SetODR()`
- `QMC6309_Wait_Ready()`
- `QMC6309_DumpRegs()`

## Low-Pass Filter Integration

The raw read path is still preserved:

- `QMC6309_ReadXYZ()` returns raw magnetometer data

The new filtered read path is:

- `QMC6309_ReadXYZFiltered()`

Runtime chain:

```text
QMC6309_ReadXYZFiltered()
  -> QMC6309_ReadXYZ()
  -> Filter_MagLowPass()
```

Notes:

- the first valid sample is passed through directly
- invalid raw frames do not update filter state
- filter state is reset after each successful `QMC6309_Init()`
