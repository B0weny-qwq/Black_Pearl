#ifndef __FEATURE_SWITCH_H
#define __FEATURE_SWITCH_H

/*
 * Central feature switches for bring-up and integration.
 * Current profile: AHRS output only.
 */

/* Core modules */
#define ENABLE_WIRELESS_MODULE         1
#define ENABLE_GPS_MODULE              0
#define ENABLE_MAG_MODULE              1
#define ENABLE_IMU_MODULE              1

/* Runtime polling */
#define ENABLE_SHIP_PROTOCOL_SCHED     1
#define ENABLE_MAG_STANDALONE_POLL     0
#define ENABLE_IMU_AHRS_POLL           1
#define ENABLE_IMU_BASIC_POLL          0
#define ENABLE_LOG_INIT                1

/* Limit runtime logs to AHRS output plus hard errors. */
#define AHRS_TEST_ONLY                 0

/* Logging and serial */
#define LOG_UART_BAUDRATE              115200UL
#define SERIAL_LOG_LEVEL               2U
#define GPS_UART_BAUDRATE              115200UL
#define GPS_DIAG_LOG_ENABLE            0U

/* Sensor bus */
#define SENSOR_I2C_SPEED_CFG           58U

/* IMU / QMI8658 */
#define QMI8658_DIAG_ENABLE            0
#define QMI8658_SOFT_RESET_ENABLE      0
#define QMI8658_CLEAR_DATAPATH_ENABLE  0
#define AHRS_MAG_ENABLE                1
#define QMI8658_I2C_USE_SOFT           0
#define QMI8658_SOFT_I2C_DELAY_US      2U
#define QMI8658_SOFT_I2C_USE_P14_P15   1
#define QMI8658_READY_MODE_STATUS0     1
#define QMI8658_READY_MODE_STATUSINT   0
#define QMI8658_INIT_NONBLOCKING       0

/* Wireless hardware */
#define ENABLE_LT8920_CHIP             1
#define ENABLE_KCT8206L_FRONTEND       1
#define SYSTEM_SPI_SPEED_CFG           SPI_Speed_4
#define WIRELESS_SPI_USE_SOFT          0
#define WIRELESS_HW_SPI_SPEED_CFG      SPI_Speed_16
#define WIRELESS_SOFT_SPI_DELAY_US     0

/* Antenna selection */
#define WIRELESS_FORCE_ANT_AUTO        0U
#define WIRELESS_FORCE_ANT_1           1U
#define WIRELESS_FORCE_ANT_2           2U
#define WIRELESS_FORCE_ANT_MODE        WIRELESS_FORCE_ANT_1

/* Wireless diagnostics */
#if ENABLE_KCT8206L_FRONTEND
#define WIRELESS_FRONTEND_BYPASS_TEST  0
#else
#define WIRELESS_FRONTEND_BYPASS_TEST  1
#endif
#define WIRELESS_TX_ONLY_TEST          0
#define WIRELESS_PAIR_TX_ONLY_TEST     0
#define WIRELESS_CONTINUOUS_TX_TEST    0
#define WIRELESS_CARRIER_WAVE_TEST     0
#define WIRELESS_RX_TRACE_ENABLE       0
#define WIRELESS_TX_TRACE_ENABLE       0

/* Wireless protocol */
#define SHIP_PROTOCOL_POLL_ENABLE      1
#define SHIP_PROTOCOL_COMPAT_ENABLE    0
#define SHIP_PROTOCOL_DIAG_ENABLE      0
#define SHIP_PROTOCOL_ERROR_LOG_ENABLE 0
#define SHIP_PAIR_SYNC_WORD            0x03800380UL
#define SHIP_PAIR_SEND_TIMES           10U
#define SHIP_WAIT_TICKS_DEFAULT        30U
#define SHIP_PAIR_WAIT_RSP_TICKS       500U
#define SHIP_PAIR_RSP_EXPIRE_LOG_MS    5000UL
#define SHIP_RX_IDLE_WARN_MS           2000UL
#define SHIP_THROTTLE_TIMEOUT_MS       1500UL
#define SHIP_WORK_RX_REOPEN_TICKS      10U

/* Pairing seed */
#define PAIR_CHANNEL                   0x7F
#define SHIP_PAIR_SEED_USE_CHIPID      0
#define SHIP_PAIR_SEED0                0x65
#define SHIP_PAIR_SEED1                0x65
#define SHIP_PAIR_SEED2                0xA0
#define SHIP_PAIR_SEED3                0x65
#define SHIP_PAIR_CHANNEL_DEFAULT      ((u8)(PAIR_CHANNEL))

/* Motor / ADC */
#define SHIP_THROTTLE_PWM_ENABLE       1
#define SHIP_YAW_HOLD_ENABLE           1
#define SHIP_YAW_HOLD_PERIOD_MS        100UL
#define SHIP_YAW_HOLD_LOG_ENABLE       1
#define SHIP_YAW_HOLD_LOG_PERIOD_MS    1000UL
#define SHIP_YAW_HOLD_OUTPUT_LIMIT     60
#define SHIP_YAW_HOLD_KP_Q10           31
#define SHIP_YAW_HOLD_KI_Q10           0
#define SHIP_YAW_HOLD_KD_Q10           0
#define SHIP_ADC_REF_MV                3300UL
#define SHIP_BAT_DIV_NUM               1UL
#define SHIP_BAT_DIV_DEN               1UL
#define SHIP_ADC_LOG_ENABLE            1

/* Legacy compatibility */
#define WIRELESS_MINIMAL_TEST_ONLY     0
#define WIRELESS_SOFT_SPI_TEST         WIRELESS_SPI_USE_SOFT
#define LT8920_FIFO_DELAY_TEST         0
#define LT8920_FORCE_TX_ORDER_TEST     0

#endif
