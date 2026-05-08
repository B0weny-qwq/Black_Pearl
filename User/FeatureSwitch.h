#ifndef __FEATURE_SWITCH_H
#define __FEATURE_SWITCH_H

/*
 * 统一功能开关文件。
 * 这里用 0/1 控制各模块是否初始化、是否轮询。
 */

/* 核心模块开关：今晚联调档为开环控船，GPS+无线+磁力计开启，IMU关闭。 */
#define ENABLE_WIRELESS_MODULE         1
#define ENABLE_GPS_MODULE              1
#define ENABLE_MAG_MODULE              1
#define ENABLE_IMU_MODULE              0

/* 运行期辅助开关。 */
#define ENABLE_SHIP_PROTOCOL_SCHED     1
#define ENABLE_MAG_STANDALONE_POLL     1
#define ENABLE_IMU_AHRS_POLL           0
#define ENABLE_LOG_INIT                1

/* AHRS 仅日志调试模式，保留给现有日志过滤逻辑。 */
#define AHRS_TEST_ONLY                 0

/* IMU IIC 调试开关：打开后恢复 QMI8658 初始化与读数链路的详细日志。 */
#define QMI8658_DIAG_ENABLE            0
/* 5.1 凌晨老版复测：禁用软复位和 CTRL9 数据通路清理。 */
#define QMI8658_SOFT_RESET_ENABLE      0
#define QMI8658_CLEAR_DATAPATH_ENABLE  0
#define QMI8658_I2C_USE_SOFT           0
#define QMI8658_SOFT_I2C_DELAY_US      2U
#define QMI8658_SOFT_I2C_USE_P14_P15   1
#define QMI8658_READY_MODE_STATUS0     1
#define QMI8658_READY_MODE_STATUSINT   0
#define QMI8658_INIT_NONBLOCKING       0

/* 总线、波特率和初始化参数。 */
#define LOG_UART_BAUDRATE              115200UL
#define SERIAL_LOG_LEVEL               2U
#define GPS_UART_BAUDRATE              115200UL
#define GPS_DIAG_LOG_ENABLE            0U
#define SENSOR_I2C_SPEED_CFG           58U
#define SYSTEM_SPI_SPEED_CFG           SPI_Speed_4

/* 无线 SPI 选择：1=软件 SPI，0=硬件 SPI4。 */
#define ENABLE_LT8920_CHIP            1
#define ENABLE_KCT8206L_FRONTEND      1
#define WIRELESS_SPI_USE_SOFT         0
#define WIRELESS_HW_SPI_SPEED_CFG     SPI_Speed_16
#define WIRELESS_SOFT_SPI_DELAY_US    0

/* 无线协议与联调开关。 */
#define SHIP_PROTOCOL_POLL_ENABLE      1
#define SHIP_PROTOCOL_COMPAT_ENABLE    0
#define SHIP_PAIR_SYNC_WORD            0x03800380UL
#define SHIP_PAIR_SEND_TIMES           10U
#define SHIP_WAIT_TICKS_DEFAULT        30U
#define SHIP_PAIR_WAIT_RSP_TICKS       500U
#define SHIP_PAIR_RSP_EXPIRE_LOG_MS    5000UL
#define SHIP_RX_IDLE_WARN_MS           2000UL
#define SHIP_THROTTLE_TIMEOUT_MS       1500UL
#define SHIP_WORK_RX_REOPEN_TICKS      10U

/* 配对信道与固定 seed。 */
#define PAIR_CHANNEL                   0x7F
#define SHIP_PAIR_SEED_USE_CHIPID      0
#define SHIP_PAIR_SEED0                0x65
#define SHIP_PAIR_SEED1                0x65
#define SHIP_PAIR_SEED2                0xA0
#define SHIP_PAIR_SEED3                0x65
/* 配对阶段默认发射信道。 */
#define SHIP_PAIR_CHANNEL_DEFAULT      ((u8)(PAIR_CHANNEL))

/* 遥控油门 PWM 总开关。 */
#define SHIP_THROTTLE_PWM_ENABLE       1

/* 电池采样打印参数。 */
#define SHIP_ADC_REF_MV                3300UL
#define SHIP_BAT_DIV_NUM               1UL
#define SHIP_BAT_DIV_DEN               1UL
#define SHIP_ADC_LOG_ENABLE            1

/* 无线调试输出开关。 */
#if ENABLE_KCT8206L_FRONTEND
#define WIRELESS_FRONTEND_BYPASS_TEST 0
#else
#define WIRELESS_FRONTEND_BYPASS_TEST 1
#endif
#define WIRELESS_TX_ONLY_TEST         0
#define WIRELESS_PAIR_TX_ONLY_TEST    0
#define WIRELESS_CONTINUOUS_TX_TEST   0
#define WIRELESS_CARRIER_WAVE_TEST    0
#define WIRELESS_RX_TRACE_ENABLE      0
#define WIRELESS_TX_TRACE_ENABLE      0

/*
 * 保留旧代码路径兼容宏。
 * 新代码建议优先使用上面的 ENABLE_* 宏。
 */
#define WIRELESS_MINIMAL_TEST_ONLY     0
#define WIRELESS_SOFT_SPI_TEST         WIRELESS_SPI_USE_SOFT
#define LT8920_FIFO_DELAY_TEST         0
#define LT8920_FORCE_TX_ORDER_TEST     0

#endif
