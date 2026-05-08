#ifndef __FEATURE_SWITCH_H
#define __FEATURE_SWITCH_H

/*
 * 统一功能开关文件。
 * 按外设/功能分组管理，默认使用 0/1 控制是否初始化、是否轮询或是否启用。
 */

/* -------------------------------------------------------------------------- */
/* 系统主模块开关                                                             */
/* -------------------------------------------------------------------------- */
/* 当前联调配置：无线 + GPS + 磁力计开启，IMU 关闭。 */
#define ENABLE_WIRELESS_MODULE         1   /* 无线模块初始化总开关：1=初始化 LT8920/KCT8206L 并进入收发流程，0=完全关闭无线链路。 */
#define ENABLE_GPS_MODULE              1   /* GPS 模块初始化总开关：1=初始化 GPS 串口与解析状态机，0=不启动 GPS 接收与解析。 */
#define ENABLE_MAG_MODULE              1   /* 磁力计模块初始化总开关：1=初始化 QMC6309 并允许读取航向相关原始数据，0=不启用磁力计。 */
#define ENABLE_IMU_MODULE              0   /* IMU 模块初始化总开关：1=初始化 QMI8658/姿态链路，0=完全跳过 IMU 上电与配置。 */

/* 运行期任务开关。 */
#define ENABLE_SHIP_PROTOCOL_SCHED     1   /* 船控协议调度开关：1=运行遥控配对/收包/状态上报/电机控制调度，0=不执行业务协议主循环。 */
#define ENABLE_MAG_STANDALONE_POLL     1   /* 磁力计独立轮询开关：1=周期读取 QMC6309 并输出/更新磁场数据，0=初始化后不再主动轮询。 */
#define ENABLE_IMU_AHRS_POLL           0   /* IMU/AHRS 轮询开关：1=周期读取 IMU 并更新姿态解算，0=即使初始化 IMU 也不跑 AHRS 轮询。 */
#define ENABLE_LOG_INIT                1   /* 日志初始化开关：1=初始化日志系统并允许 LOGI/LOGW/LOGE/LOGD 输出，0=不打开日志输出。 */

/* AHRS 仅日志调试模式，保留给现有日志过滤逻辑。1=只看姿态日志，不走正常业务。 */
#define AHRS_TEST_ONLY                 0

/* -------------------------------------------------------------------------- */
/* 日志与串口基础参数                                                         */
/* -------------------------------------------------------------------------- */
#define LOG_UART_BAUDRATE              115200UL  /* 系统日志串口波特率。 */
#define SERIAL_LOG_LEVEL               2U        /* 串口日志等级：0=仅 ERROR，1=ERROR/WARN，2=ERROR/WARN/INFO，3=全部含 DEBUG。 */
#define GPS_UART_BAUDRATE              115200UL  /* GPS 串口波特率。 */
#define GPS_DIAG_LOG_ENABLE            0U        /* GPS 原始诊断日志开关。 */

/* -------------------------------------------------------------------------- */
/* 传感器总线与磁力计                                                         */
/* -------------------------------------------------------------------------- */
#define SENSOR_I2C_SPEED_CFG           58U       /* 传感器 I2C 速度配置，当前 58U 约等于 100kHz。 */

/* -------------------------------------------------------------------------- */
/* IMU / QMI8658                                                              */
/* -------------------------------------------------------------------------- */
/* IMU IIC 调试开关：打开后恢复 QMI8658 初始化与读数链路的详细日志。 */
#define QMI8658_DIAG_ENABLE            0
/* 老版本复测兼容项：禁用软复位和 CTRL9 数据通路清理。 */
#define QMI8658_SOFT_RESET_ENABLE      0
#define QMI8658_CLEAR_DATAPATH_ENABLE  0
#define QMI8658_I2C_USE_SOFT           0         /* 1=软件 I2C，0=硬件 I2C。 */
#define QMI8658_SOFT_I2C_DELAY_US      2U        /* 软件 I2C 单步延时，数值越大总线越慢。 */
#define QMI8658_SOFT_I2C_USE_P14_P15   1         /* 软件 I2C 是否使用 P1.4/P1.5。 */
#define QMI8658_READY_MODE_STATUS0     1         /* 通过 STATUS0 轮询 ready。 */
#define QMI8658_READY_MODE_STATUSINT   0         /* 通过 STATUSINT 轮询 ready。 */
#define QMI8658_INIT_NONBLOCKING       0         /* 1=非阻塞初始化。 */

/* -------------------------------------------------------------------------- */
/* 无线硬件层：LT8920 + KCT8206L                                              */
/* -------------------------------------------------------------------------- */
#define ENABLE_LT8920_CHIP             1         /* LT8920 射频芯片总开关。 */
#define ENABLE_KCT8206L_FRONTEND       1         /* KCT8206L 前端总开关。 */
#define SYSTEM_SPI_SPEED_CFG           SPI_Speed_4   /* 系统默认 SPI 分频。可选 SPI_Speed_2/4/8/16/32/64；分频值越小越快。 */
#define WIRELESS_SPI_USE_SOFT          0         /* 1=软件 SPI，0=硬件 SPI4。 */
#define WIRELESS_HW_SPI_SPEED_CFG      SPI_Speed_16  /* 无线硬件 SPI 分频。可选 SPI_Speed_2/4/8/16/32/64；分频值越小越快。 */
#define WIRELESS_SOFT_SPI_DELAY_US     0         /* 无线软件 SPI 单步延时，数值越大时序越慢。 */

/* 天线选择模式：
 * WIRELESS_FORCE_ANT_AUTO = 自动扫描后选择；
 * WIRELESS_FORCE_ANT_1    = 强制走 ANT1；
 * WIRELESS_FORCE_ANT_2    = 强制走 ANT2。
 */
#define WIRELESS_FORCE_ANT_AUTO        0U
#define WIRELESS_FORCE_ANT_1           1U
#define WIRELESS_FORCE_ANT_2           2U
#define WIRELESS_FORCE_ANT_MODE        WIRELESS_FORCE_ANT_1

/* 无线调试输出开关。 */
#if ENABLE_KCT8206L_FRONTEND
#define WIRELESS_FRONTEND_BYPASS_TEST  0         /* 0=走 KCT8206L，1=前端旁路测试。 */
#else
#define WIRELESS_FRONTEND_BYPASS_TEST  1
#endif
#define WIRELESS_TX_ONLY_TEST          0         /* 仅发射测试。 */
#define WIRELESS_PAIR_TX_ONLY_TEST     0         /* 仅配对发射测试。 */
#define WIRELESS_CONTINUOUS_TX_TEST    0         /* 连续发射测试。 */
#define WIRELESS_CARRIER_WAVE_TEST     0         /* 载波测试。 */
#define WIRELESS_RX_TRACE_ENABLE       0         /* 接收路径详细日志。 */
#define WIRELESS_TX_TRACE_ENABLE       0         /* 发送路径详细日志。 */

/* -------------------------------------------------------------------------- */
/* 无线协议 / 配对 / 遥控业务                                                 */
/* -------------------------------------------------------------------------- */
#define SHIP_PROTOCOL_POLL_ENABLE      1
#define SHIP_PROTOCOL_COMPAT_ENABLE    0
#define SHIP_PAIR_SYNC_WORD            0x03800380UL
#define SHIP_PAIR_SEND_TIMES           10U        /* 配对请求连续发送次数。 */
#define SHIP_WAIT_TICKS_DEFAULT        30U        /* 常规调度等待 tick。 */
#define SHIP_PAIR_WAIT_RSP_TICKS       500U       /* 配对后等待响应窗口 tick。 */
#define SHIP_PAIR_RSP_EXPIRE_LOG_MS    5000UL     /* 配对响应超时日志节流时间。 */
#define SHIP_RX_IDLE_WARN_MS           2000UL     /* 工作接收空闲告警阈值。 */
#define SHIP_THROTTLE_TIMEOUT_MS       1500UL     /* 遥控油门超时回中时间。 */
#define SHIP_WORK_RX_REOPEN_TICKS      10U        /* 工作接收 reopen 周期 tick。 */

/* 配对信道与 seed 配置。 */
#define PAIR_CHANNEL                   0x7F      /* 配对广播信道。 */
#define SHIP_PAIR_SEED_USE_CHIPID      0         /* 0=使用下面固定四字节 seed，1=改为基于芯片 ID 生成 seed。 */
#define SHIP_PAIR_SEED0                0x65      /* 固定 seed 字节 0。 */
#define SHIP_PAIR_SEED1                0x65      /* 固定 seed 字节 1。 */
#define SHIP_PAIR_SEED2                0xA0      /* 固定 seed 字节 2。 */
#define SHIP_PAIR_SEED3                0x65      /* 固定 seed 字节 3。 */
/* 配对阶段默认发射信道。 */
#define SHIP_PAIR_CHANNEL_DEFAULT      ((u8)(PAIR_CHANNEL))

/* -------------------------------------------------------------------------- */
/* 电机 / PWM / 电池采样                                                      */
/* -------------------------------------------------------------------------- */
/* 遥控油门 PWM 总开关。 */
#define SHIP_THROTTLE_PWM_ENABLE       1

/* 电池采样与分压参数。 */
#define SHIP_ADC_REF_MV                3300UL    /* ADC 参考电压，单位 mV。 */
#define SHIP_BAT_DIV_NUM               1UL       /* 电池分压换算分子；电池电压 = ADC 电压 * NUM / DEN。 */
#define SHIP_BAT_DIV_DEN               1UL       /* 电池分压换算分母；当前 1/1 表示未做额外分压换算。 */
#define SHIP_ADC_LOG_ENABLE            1         /* 电池 ADC 日志开关：1=打印采样日志，0=关闭。 */

/* -------------------------------------------------------------------------- */
/* 旧代码兼容宏                                                               */
/* -------------------------------------------------------------------------- */
/*
 * 保留旧代码路径兼容宏。
 * 新代码建议优先使用上面的 ENABLE_* 宏。
 */
#define WIRELESS_MINIMAL_TEST_ONLY     0   /* 旧最小化无线测试入口；1=仅跑最小无线 bring-up。 */
#define WIRELESS_SOFT_SPI_TEST         WIRELESS_SPI_USE_SOFT  /* 旧软件 SPI 测试宏，复用当前 WIRELESS_SPI_USE_SOFT。 */
#define LT8920_FIFO_DELAY_TEST         0   /* 旧 LT8920 FIFO 延时实验宏；1=启用历史 FIFO 延时测试路径。 */
#define LT8920_FORCE_TX_ORDER_TEST     0   /* 旧 LT8920 发送时序强制实验宏；1=启用历史 TX 顺序测试路径。 */

#endif
