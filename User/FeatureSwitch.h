#ifndef __FEATURE_SWITCH_H
#define __FEATURE_SWITCH_H

/**
 * @file    FeatureSwitch.h
 * @brief   Black Pearl v1.1 功能开关与联调参数集中定义
 * @author  boweny
 * @date    2026-05-12
 * @version v1.7.59
 *
 * @details
 * 本文件集中管理当前工程的功能开关、联调阈值和协议参数。
 * 这里的内容不是“随手写的宏集合”，而是当前固件真实运行档位的配置源。
 *
 * 约定：
 * - 1 / ENABLE / TRUE 表示启用
 * - 0 / DISABLE / FALSE 表示关闭
 * - 修改这里的参数，会直接影响启动链路、日志输出、无线协议节奏和船体 yaw 自稳行为
 *
 * 当前工程 profile：
 * - Wireless + GPS + MAG + IMU + AHRS 运行
 * - 旧遥控器协议与 `0x12/0x13/0x14/0x15` 回传链路保持启用
 * - 船体手动 yaw 自稳开启，但保持保守 P-only 参数
 *
 * @note
 * 需要调参时，优先改本文件，不要在业务层到处散落重复常量。
 */

/* PID / yaw hold */
/**
 * @name   PID 与船体 yaw 自稳参数
 * @brief  船体闭环差速控制相关参数，放在文件顶部便于调试和快速取值
 * @{
 */
/**
 * @brief  电机 PWM 总开关
 * @details
 * - 1：允许 Motor_SetBothSpeed 生效
 * - 0：禁用 PWM 输出，仅保留日志
 */
#define SHIP_THROTTLE_PWM_ENABLE       1

/**
 * @brief  手动控制目标刷新周期
 * @details
 * - 单位为 ms，当前值 10ms
 * - 只决定内部 `Motor_SetBothSpeed()` 目标刷新节拍
 * - 不等同于遥控器 `0x11` 输入帧率、日志打印周期或底层 PWM 硬件频率
 */
#define SHIP_MANUAL_CONTROL_PERIOD_MS  10UL

/**
 * @brief  船体 yaw 自稳总开关
 * @details
 * - 1：启用差速 yaw 自稳
 * - 0：关闭自稳，仅保留手动/开环控制
 */
#define SHIP_YAW_HOLD_ENABLE           1

/**
 * @brief  遥控器在线时是否允许手动油门分支内启用 yaw 自稳
 * @details
 * - 0：遥控器接入后不抢油门，yaw 自稳只在无遥控时的空闲态接管
 * - 1：保留原有手动航向保持逻辑
 */
#define SHIP_YAW_HOLD_MANUAL_ENABLE    1

/**
 * @brief  船体 yaw 自稳控制周期
 * @details
 * - 当前值 100ms
 * - 周期过快会放大噪声，过慢会降低修正响应
 */
#define SHIP_YAW_HOLD_PERIOD_MS        100UL

/**
 * @brief  船体 yaw 自稳日志开关
 * @details
 * - 1：输出 `[MOT]` 诊断日志
 * - 0：关闭 motor 诊断日志
 */
#define SHIP_YAW_HOLD_LOG_ENABLE       1

/**
 * @brief  船体 yaw 自稳日志周期
 * @details
 * - 当前为 1000ms，避免上位机被高频日志刷屏
 * - 只限制 yaw-hold 诊断打印，不参与 PID 或电机目标刷新
 */
#define SHIP_YAW_HOLD_LOG_PERIOD_MS    1000UL

/**
 * @brief  差速输出限幅
 * @details 左右电机差速总输出限制，防止 yaw 修正过猛。
 */
#define SHIP_YAW_HOLD_OUTPUT_LIMIT     1000

/**
 * @brief  yaw 自稳达到满修正输入的偏航误差（centi-degree）
 * @details
 * - 单位为 0.01 度，1000 表示 10.00 度
 * - yaw 误差会先归一化到 -1000 ~ +1000，再进入 PID
 * - 这样 PID 数值和电机 speed 满量程保持一致，避免小角度直接顶满
 */
#define SHIP_YAW_HOLD_FULL_ERROR_CD    1000

/**
 * @brief  yaw 自稳最大差速比例（permille）
 * @details
 * - 500 表示 PID 满输出时，差速修正最多为当前基础油门的 50%
 * - 可防止自稳态把单侧电机直接压死或拉满
 */
#define SHIP_YAW_HOLD_DIFF_LIMIT_PERMILLE 500

/**
 * @brief  手动自稳的转向门限
 * @details
 * - 单位为遥控器轴心偏差值，100 表示满量程中心偏差
 * - 当左右摇杆偏差不超过该值时，认为用户想直线前进，允许进入 yaw 自稳
 * - 当前建议值为 10，等价于大约 10% 的转向输入
 */
#define SHIP_YAW_HOLD_STEER_GATE       15U

/**
 * @brief  yaw 自稳比例增益
 * @details
 * - Q10 定点
 * - 当前值偏保守，先保证不明显过冲
 */
#define SHIP_YAW_HOLD_KP_Q10           1024

/**
 * @brief  yaw 自稳误差死区（centi-degree）
 * @details 小误差直接忽略，降低静止抖动和输出突增
 */
#define SHIP_YAW_HOLD_DEADBAND_CD      80

/**
 * @brief  yaw 自稳积分增益
 * @details 当前关闭，避免静态状态下积分堆积。
 */
#define SHIP_YAW_HOLD_KI_Q10           0

/**
 * @brief  yaw 自稳微分增益
 * @details 当前关闭，避免噪声放大。
 */
#define SHIP_YAW_HOLD_KD_Q10           0
/** @} */

/* Core modules */
/**
 * @name   核心模块开关
 * @brief  控制无线、GPS、磁力计和 IMU 是否参与当前固件运行档位
 * @{
 */
/**
 * @brief  无线模块总开关
 * @details
 * - 1：启用 LT8920 无线链路、配对逻辑和遥控协议
 * - 0：完全关闭无线业务，仅保留本地传感器与日志
 */
#define ENABLE_WIRELESS_MODULE         1

/**
 * @brief  GPS 模块总开关
 * @details
 * - 1：启用 GPS 初始化、轮询和状态回传
 * - 0：关闭 GPS，避免串口和航向参考链路干扰当前联调
 */
#define ENABLE_GPS_MODULE              1

/**
 * @brief  磁力计模块总开关
 * @details
 * - 1：启用 QMC6309 读取与磁航向估计
 * - 0：关闭磁力计相关逻辑，AHRS 仅保留 gyro / acc 路径
 */
#define ENABLE_MAG_MODULE              1

/**
 * @brief  IMU 模块总开关
 * @details
 * - 1：启用 QMI8658 初始化与姿态融合
 * - 0：关闭 IMU，主循环不再进入 AHRS 轮询
 */
#define ENABLE_IMU_MODULE              1
/** @} */

/* Runtime polling */
/**
 * @name   运行期轮询开关
 * @brief  控制主循环中各业务轮询是否进入调度
 * @{
 */
/**
 * @brief  无线协议调度器开关
 * @details
 * - 1：主循环调用 ship protocol scheduler
 * - 0：不调度无线业务，适合纯传感器排障
 */
#define ENABLE_SHIP_PROTOCOL_SCHED     1

/**
 * @brief  磁力计独立轮询开关
 * @details
 * - 1：磁力计单独刷屏，适合排查 QMC6309 本体
 * - 0：不独立刷屏，由 AHRS 内部低频读取
 */
#define ENABLE_MAG_STANDALONE_POLL     1

/**
 * @brief  AHRS 姿态轮询开关
 * @details
 * - 1：主循环进入 AHRS_UpdateRaw6Axis / AHRS_UpdateRawMag 路径
 * - 0：关闭姿态融合，只保留基础读数或其他业务
 */
#define ENABLE_IMU_AHRS_POLL           1

/**
 * @brief  IMU 基础读数轮询开关
 * @details
 * - 1：只打印原始 IMU 数据，适合 bring-up
 * - 0：关闭基础 raw 打印，避免与 AHRS 输出重复
 */
#define ENABLE_IMU_BASIC_POLL          0

/**
 * @brief  日志初始化开关
 * @details
 * - 1：启动时初始化串口日志系统
 * - 0：跳过日志初始化，仅适合极端裁剪场景
 */
#define ENABLE_LOG_INIT                1
/** @} */

/* Test / diagnostics */
/**
 * @name   测试与诊断开关
 * @brief  控制是否压缩运行输出，只保留关键日志
 * @{
 */
/**
 * @brief  AHRS 测试模式
 * @details
 * - 0：正常运行，保留当前项目标准日志
 * - 1：尽量收敛日志输出，仅保留姿态和硬错误
 */
#define AHRS_TEST_ONLY                 0
/** @} */

/* Logging and serial */
/**
 * @name   串口与日志参数
 * @brief  定义主日志与 GPS 日志的波特率和诊断等级
 * @{
 */
/**
 * @brief  主日志串口波特率
 * @details 当前工程使用 115200，保证上位机与现场串口稳定显示。
 */
#define LOG_UART_BAUDRATE              115200UL

/**
 * @brief  主日志输出等级
 * @details
 * - 2U：当前默认等级，兼顾调试信息和日志体积
 */
#define SERIAL_LOG_LEVEL               2U

/**
 * @brief  GPS 串口波特率
 * @details 当前关闭 GPS 时仍保留该参数，避免后续启用时散落修改。
 */
#define GPS_UART_BAUDRATE              115200UL

/**
 * @brief  GPS 诊断日志开关
 * @details
 * - 0U：关闭 GPS 诊断刷屏
 * - 1U：打印 GPS 解析与状态细节
 */
#define GPS_DIAG_LOG_ENABLE            0U
/** @} */

/* Sensor bus */
/**
 * @name   传感器总线参数
 * @brief  定义当前 IMU / MAG 共用的 I2C 速度档位
 * @{
 */
/**
 * @brief  传感器 I2C 速度配置
 * @details 当前值 58U 用于当前硬件的稳定初始化与周期读取。
 */
#define SENSOR_I2C_SPEED_CFG           58U
/** @} */

/* IMU / QMI8658 */
/**
 * @name   IMU 与 AHRS 相关参数
 * @brief  定义 QMI8658 初始化、诊断和 AHRS 运行模式
 * @{
 */
/**
 * @brief  QMI8658 诊断开关
 * @details
 * - 0：关闭器件级 verbose 日志
 * - 1：打印寄存器和状态细节，便于 bring-up
 */
#define QMI8658_DIAG_ENABLE            0

/**
 * @brief  QMI8658 软件复位开关
 * @details 当前关闭，避免启动阶段额外不确定性。
 */
#define QMI8658_SOFT_RESET_ENABLE      0

/**
 * @brief  QMI8658 清理数据路径开关
 * @details 当前关闭，保留稳定的 bring-up 路径。
 */
#define QMI8658_CLEAR_DATAPATH_ENABLE  0

/**
 * @brief  AHRS 是否启用磁力计修正
 * @details
 * - 1：允许磁力计参与航向慢修正
 * - 0：只保留 gyro / acc 路径，方便磁干扰排查
 */
#define AHRS_MAG_ENABLE                1

/**
 * @brief  QMI8658 是否使用软件 I2C
 * @details
 * - 0：使用硬件 I2C
 * - 1：使用软件 I2C
 */
#define QMI8658_I2C_USE_SOFT           0

/**
 * @brief  软件 I2C 延时
 * @details 仅在 `QMI8658_I2C_USE_SOFT=1` 时生效。
 */
#define QMI8658_SOFT_I2C_DELAY_US      2U

/**
 * @brief  软件 I2C 引脚选择
 * @details 当前工程保留 P1.4 / P1.5 作为软件 I2C 兼容路径。
 */
#define QMI8658_SOFT_I2C_USE_P14_P15   1

/**
 * @brief  以 STATUS0 作为 ready 判定来源
 * @details
 * - 1：优先用 STATUS0 判断 ready
 * - 0：使用其他状态位或兼容策略
 */
#define QMI8658_READY_MODE_STATUS0     1

/**
 * @brief  以 STATUSINT 作为 ready 判定来源
 * @details 当前关闭，避免双判定路径引入歧义。
 */
#define QMI8658_READY_MODE_STATUSINT   0

/**
 * @brief  QMI8658 初始化是否非阻塞
 * @details 当前关闭，使用阻塞式 bring-up，便于稳定复现。
 */
#define QMI8658_INIT_NONBLOCKING       0
/** @} */

/* Wireless hardware */
/**
 * @name   无线硬件参数
 * @brief  定义 LT8920 与前端芯片的基本开关和 SPI 档位
 * @{
 */
/**
 * @brief  LT8920 芯片开关
 * @details
 * - 1：启用无线芯片
 * - 0：关闭无线硬件相关路径
 */
#define ENABLE_LT8920_CHIP             1

/**
 * @brief  KCT8206L 前端开关
 * @details 当前打开，表示使用现有前端板级路径。
 */
#define ENABLE_KCT8206L_FRONTEND       1

/**
 * @brief  系统 SPI 速度
 * @details 用于系统级 SPI 外设的基础档位。
 */
#define SYSTEM_SPI_SPEED_CFG           SPI_Speed_4

/**
 * @brief  无线是否使用软件 SPI
 * @details 当前关闭，使用硬件 SPI 提升稳定性。
 */
#define WIRELESS_SPI_USE_SOFT          0

/**
 * @brief  无线硬件 SPI 速度
 * @details 当前档位为 SPI_Speed_16，适合现有无线链路。
 */
#define WIRELESS_HW_SPI_SPEED_CFG      SPI_Speed_16

/**
 * @brief  无线软件 SPI 延时
 * @details 仅在 `WIRELESS_SPI_USE_SOFT=1` 时生效。
 */
#define WIRELESS_SOFT_SPI_DELAY_US     0
/** @} */

/* Antenna selection */
/**
 * @name   天线选择
 * @brief  指定当前无线链路使用哪一路天线
 * @{
 */
/**
 * @brief  自动选择天线
 */
#define WIRELESS_FORCE_ANT_AUTO        0U

/**
 * @brief  强制使用天线 1
 */
#define WIRELESS_FORCE_ANT_1           1U

/**
 * @brief  强制使用天线 2
 */
#define WIRELESS_FORCE_ANT_2           2U

/**
 * @brief  当前强制模式
 * @details 现阶段固定为天线 1，避免自动切换带来额外变量。
 */
#define WIRELESS_FORCE_ANT_MODE        WIRELESS_FORCE_ANT_1
/** @} */

/* Wireless diagnostics */
/**
 * @name   无线诊断开关
 * @brief  控制无线前端旁路、发射测试和 trace 输出
 * @{
 */
#if ENABLE_KCT8206L_FRONTEND
/**
 * @brief  前端旁路测试
 * @details
 * - 0：正常使用前端
 * - 1：旁路前端做纯芯片测试
 */
#define WIRELESS_FRONTEND_BYPASS_TEST  0
#else
#define WIRELESS_FRONTEND_BYPASS_TEST  1
#endif

/**
 * @brief  仅发送测试
 * @details 当前关闭，避免占用无线链路。
 */
#define WIRELESS_TX_ONLY_TEST          0

/**
 * @brief  配对仅发送测试
 */
#define WIRELESS_PAIR_TX_ONLY_TEST     0

/**
 * @brief  连续发送测试
 */
#define WIRELESS_CONTINUOUS_TX_TEST    0

/**
 * @brief  载波测试
 */
#define WIRELESS_CARRIER_WAVE_TEST     0

/**
 * @brief  接收 trace 输出
 */
#define WIRELESS_RX_TRACE_ENABLE       0

/**
 * @brief  发送 trace 输出
 */
#define WIRELESS_TX_TRACE_ENABLE       0
/** @} */

/* Wireless protocol */
/**
 * @name   无线协议参数
 * @brief  定义配对、超时、重连与协议兼容策略
 * @{
 */
/**
 * @brief  无线协议调度器开关
 * @details 当前开启，主循环会进入 ship protocol 调度。
 */
#define SHIP_PROTOCOL_POLL_ENABLE      1

/**
 * @brief  兼容旧协议格式开关
 */
#define SHIP_PROTOCOL_COMPAT_ENABLE    0

/**
 * @brief  无线协议调试日志开关
 */
#define SHIP_PROTOCOL_DIAG_ENABLE      1

/**
 * @brief  无线协议错误日志开关
 */
#define SHIP_PROTOCOL_ERROR_LOG_ENABLE 0

/**
 * @brief  配对同步字
 */
#define SHIP_PAIR_SYNC_WORD            0x03800380UL

/**
 * @brief  配对发送次数
 */
#define SHIP_PAIR_SEND_TIMES           10U

/**
 * @brief  默认等待 tick
 */
#define SHIP_WAIT_TICKS_DEFAULT        30U

/**
 * @brief  配对等待响应 tick
 */
#define SHIP_PAIR_WAIT_RSP_TICKS       500U

/**
 * @brief  配对响应超时日志周期
 */
#define SHIP_PAIR_RSP_EXPIRE_LOG_MS    5000UL

/**
 * @brief  接收空闲告警周期
 */
#define SHIP_RX_IDLE_WARN_MS           2000UL

/**
 * @brief  油门超时判定周期
 */
#define SHIP_THROTTLE_TIMEOUT_MS       1500UL

/**
 * @brief  工作接收重开 tick
 */
#define SHIP_WORK_RX_REOPEN_TICKS      10U
/** @} */

/* Pairing seed */
/**
 * @name   配对种子
 * @brief  定义无线配对所使用的固定种子或芯片 ID 方式
 * @{
 */
/**
 * @brief  配对频道
 */
#define PAIR_CHANNEL                   0x7F

/**
 * @brief  是否使用芯片 ID 作为配对种子
 */
#define SHIP_PAIR_SEED_USE_CHIPID      0

/**
 * @brief  配对种子字节 0
 */
#define SHIP_PAIR_SEED0                0x65

/**
 * @brief  配对种子字节 1
 */
#define SHIP_PAIR_SEED1                0x65

/**
 * @brief  配对种子字节 2
 */
#define SHIP_PAIR_SEED2                0xA0

/**
 * @brief  配对种子字节 3
 */
#define SHIP_PAIR_SEED3                0x65

/**
 * @brief  配对频道默认值
 */
#define SHIP_PAIR_CHANNEL_DEFAULT      ((u8)(PAIR_CHANNEL))
/** @} */

/* Motor / ADC */
/**
 * @name   电机与 ADC 参数
 * @brief  控制电池电压换算与剩余 ADC 诊断参数
 * @{
 */

/**
 * @brief  ADC 基准电压
 */
#define SHIP_ADC_REF_MV                3300UL

/**
 * @brief  电池分压分子
 */
#define SHIP_BAT_DIV_NUM               1UL

/**
 * @brief  电池分压分母
 */
#define SHIP_BAT_DIV_DEN               1UL

/**
 * @brief  ADC 电池日志开关
 */
#define SHIP_ADC_LOG_ENABLE            1

/**
 * @brief  串口/姿态/传感器日志频率控制
 * @details
 * - 这些宏单位均为 ms
 * - 0 表示不限制，1 以上表示最小打印间隔
 * - 这些宏只限制日志输出，不参与手动控制目标刷新或 PWM 硬件输出
 */
#define SHIP_RX_LOG_PERIOD_MS          100U
#define SHIP_RX_CRC_LOG_THRESHOLD      10U
#define SHIP_MOT_LOG_PERIOD_MS         100U
#define SHIP_RC_INPUT_LOG_PERIOD_MS    100U
#define SHIP_POWER_LOG_PERIOD_MS       10000U
#define SHIP_MAG_LOG_PERIOD_MS         1000U
#define SHIP_IMU_LOG_PERIOD_MS         1000U
#define SHIP_PROTO_DEBUG_ENABLE        0U
#define SHIP_PROTO_DEBUG_PERIOD_MS     100U
/** @} */

/* Legacy compatibility */
/**
 * @name   旧版兼容开关
 * @brief  保留给历史代码路径和旧调试脚本使用
 * @{
 */
/**
 * @brief  仅保留最小化测试路径
 */
#define WIRELESS_MINIMAL_TEST_ONLY     0

/**
 * @brief  无线软 SPI 兼容开关
 */
#define WIRELESS_SOFT_SPI_TEST         WIRELESS_SPI_USE_SOFT

/**
 * @brief  旧 FIFO 延迟测试开关
 */
#define LT8920_FIFO_DELAY_TEST         0

/**
 * @brief  旧 TX 顺序强制测试开关
 */
#define LT8920_FORCE_TX_ORDER_TEST     0
/** @} */

#endif
