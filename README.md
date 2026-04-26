# Black Pearl v1.1

Black Pearl v1.1 is an embedded control project based on the STC32G MCU and built with Keil MDK / C251. The current firmware integrates GPS, IMU, magnetometer, wireless communication, motor PWM output, logging, filtering, and fixed-point PID control.

The project is organized around a layered structure: system entry and scheduling in `User/`, official peripheral drivers in `Driver/`, vendor example code in `App/`, and project-specific modules in `Code_boweny/`.

## Project Overview

| Item | Description |
|------|-------------|
| MCU | STC32G |
| Main clock | 24 MHz |
| Toolchain | Keil MDK / C251 |
| Log port | UART1, `P3.0 / P3.1` |
| GPS port | UART2, `P1.0 / P1.1` |
| I2C bus | `P1.4 / P1.5` |
| Wireless chipset | LT8920 + KCT8206L |
| Motor output | PWMA CH3 / CH4 |

## Directory Structure

```text
Black_Pearl_v1.1/
├── User/                     # System entry, initialization, main loop, task scheduler
├── Driver/                   # Official STC peripheral driver library
├── App/                      # Official example application layer
├── Code_boweny/              # Project-specific modules
│   ├── Function/
│   │   ├── Log/              # UART1 logging system
│   │   ├── Filter/           # Q8 fixed-point low-pass filter
│   │   └── PID/              # Q10 fixed-point PID controller
│   └── Device/
│       ├── GPS/              # GPS NMEA0183 parser
│       ├── QMI8658/          # IMU driver
│       ├── QMC6309/          # Magnetometer driver
│       ├── WIRELESS/         # LT8920 wireless driver
│       └── MOTOR/            # Dual motor PWM driver
├── RVMDK/                    # Keil project files
└── doc/                      # Project documentation
```

## Enabled Modules

### GPS

The GPS module receives NMEA0183 data through UART2. It currently supports `GGA`, `RMC`, `GSA`, `GSV`, and `VTG` sentences.

The parser uses an internal FIFO and incremental state machine. Sentences are accepted only after XOR checksum validation, and parsed positioning data is exposed through `GPS_GetState()`.

### IMU and Magnetometer

The project currently uses:

- `QMI8658` as the IMU, polled at high rate in the main loop
- `QMC6309` as the magnetometer

Both devices share the hardware I2C bus on `P1.4 / P1.5`.

### Wireless Communication

The wireless module is based on `LT8920 + KCT8206L` and uses SPI4.

Current behavior:

- Single-chip half-duplex communication
- RX by default
- TX is enabled only during transmission, then the module returns to RX
- Dual-antenna scan during startup
- Register polling instead of external interrupt handling

### Motor Driver

The motor driver uses `PWMA CH3 / CH4` for dual motor output.

| Motor | PWM Pins |
|-------|----------|
| Left motor | `P2.4 / P2.5` |
| Right motor | `P2.6 / P2.7` |

The speed range is `-1000` to `+1000`, with internal saturation. The motor module is not initialized automatically during system startup to avoid unexpected motion after power-on.

### PID Controller

The PID module provides a generic positional PID controller using Q10 fixed-point parameters:

```text
1024 = 1.0
```

The module only calculates control output. It does not directly access PWM, GPIO, sensors, or communication peripherals. It is intended as a base component for future speed, heading, and attitude control loops.

## Startup Flow

The current `SYS_Init()` flow is:

```text
EAXSFR()
-> GPIO_config()
-> Switch_config()
-> Timer_config()
-> UART_config()
-> I2C_config()
-> EA = 1
-> APP_config()
-> log_init()
-> GPS_Init()
-> Wireless_Init()
-> Sensor_I2C_prepare()
-> QMC6309_Init()
-> QMI8658_PowerOnSelfTest()
```

## Main Loop

```c
Wireless_MinimalTestUnit();

while (1)
{
    GPS_Poll();
    Wireless_Poll();
    ShipProtocol_Poll();
    Wireless_SearchSignalPoll();
    Task_Pro_Handler_Callback();
    IMU_HighRatePoll();
}
```

The main loop currently handles GPS parsing, wireless polling, ship protocol processing, wireless signal search, Timer0-driven tasks, and high-rate IMU sampling.

## Resource Usage

| Resource | Usage |
|----------|-------|
| UART1 | Log output |
| UART2 | GPS |
| I2C | QMI8658 / QMC6309 |
| SPI4 | LT8920 wireless module |
| Timer0 | 1 ms system tick |
| Timer1 | UART1 baud-rate generator |
| Timer2 | UART2 baud-rate generator |
| PWMA CH3 | Left motor PWM |
| PWMA CH4 | Right motor PWM |

## Development Notes

- `Driver/` contains the official STC driver library and should generally remain unchanged.
- Floating-point arithmetic should be avoided in firmware modules.
- `%f` should not be used in log output.
- UART2 depends on Timer2, so example modules that reuse Timer2 must stay disabled.
- After the motor module occupies `P2.4` to `P2.7`, these pins should not be reused for SPI, LCM, or general GPIO tasks.
- The wireless module initializes SPI4 internally and does not use the original SPI example initialization flow.

## Documentation

More detailed documentation is available in:

- `doc/project_doc/total.md` - project overview
- `doc/project_doc/date.md` - change log
- `doc/build_doc/README_GPS.md` - GPS module notes
- `doc/build_doc/README_wireless.md` - wireless module notes
- `Code_boweny/Device/MOTOR/README.md` - motor driver notes
- `Code_boweny/Function/PID/README.md` - PID controller notes

## Version

Current project version: `Black Pearl v1.1`

This README is based on the project state documented on 2026-04-27.
