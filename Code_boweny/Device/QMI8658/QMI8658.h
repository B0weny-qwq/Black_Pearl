/**
 * @file    QMI8658.h
 * @brief   QMI8658 6轴IMU驱动 — 寄存器定义与API声明
 * @author  boweny
 * @date    2026-04-22
 * @version v1.1
 *
 * @details
 * - 实现 QMI8658 6轴惯性测量单元（加速度计+陀螺仪）的驱动
 * - 支持轮询方式获取 Accelerometer / Gyroscope / Temperature 数据
 * - 保留原始陀螺仪读取接口，并新增软件低通后的滤波读取接口
 * - I2C 地址: 0x6B (SA0=浮空/高) / 0x6A (SA0=地)
 * - 默认配置: ACC ±4G / ODR=117Hz, GYRO ±128°/s / ODR=117Hz
 *
 * @hardware
 *   - I2C接口: P1.4(SDA) / P1.5(SCL)，硬件I2C
 *   - 系统时钟: Fosc = 24MHz
 *
 * @note    调用前必须先初始化 I2C 总线 (I2C_config)
 * @note    禁止使用浮点运算，数据以 int16 定点整数传递
 *
 * @see     Code_boweny/Device/QMI8658/QMI8658.c
 */

#ifndef __QMI8658_H__
#define __QMI8658_H__

#include "..\..\..\Driver\inc\STC32G_I2C.h"
#include "..\..\..\Driver\inc\STC32G_Delay.h"
#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\Function\Log\Log.h"

/*==============================================================
 *                        寄存器地址定义
 *==============================================================*/

/* 标识寄存器 */
#define QMI8658_REG_WHO_AM_I      0x00    /**< 芯片标识，只读，默认值 0x05 */
#define QMI8658_REG_REVISION_ID    0x01    /**< 版本号，只读 */
#define QMI8658_REG_RESET_STATE    0x4D    /**< 复位状态，只读。软复位后写入0xB0→0x60后，此处读0x80表示就绪 */

/* 控制寄存器 */
#define QMI8658_REG_CTRL1         0x02    /**< 数字低通滤波器配置 */
#define QMI8658_REG_CTRL2         0x03    /**< 加速度量程(bits[6:4]) + ODR(bits[3:0]) */
#define QMI8658_REG_CTRL3         0x04    /**< 陀螺仪量程(bits[6:4]) + ODR(bits[3:0]) */
#define QMI8658_REG_CTRL4         0x05    /**< 保留 */
#define QMI8658_REG_CTRL5         0x06    /**< 加速度计配置 */
#define QMI8658_REG_CTRL6         0x07    /**< 保留 */
#define QMI8658_REG_CTRL7         0x08    /**< 传感器使能: Bit1=gEN, Bit0=aEN */
#define QMI8658_REG_CTRL8         0x09    /**< FIFO/ODR 配置(可选) */
#define QMI8658_REG_CTRL9         0x0A    /**< AHB时钟门控(可选) */
#define QMI8658_REG_RESET         0x60    /**< 软复位寄存器，只写。写入0xB0触发复位 */

/* 状态寄存器 */
#define QMI8658_REG_STATUSINT     0x2D    /**< 中断状态(锁存) */
#define QMI8658_REG_STATUS0       0x2E    /**< 数据就绪状态: Bit0=aDA, Bit1=gDA, Bit2=TempDA */

/* 数据寄存器 */
#define QMI8658_REG_TEMP_L        0x33    /**< 温度数据低字节 (int12, 25°C=0) */
#define QMI8658_REG_TEMP_H        0x34    /**< 温度数据高字节 */
#define QMI8658_REG_AX_L          0x35    /**< X轴加速度低字节 */
#define QMI8658_REG_AX_H          0x36    /**< X轴加速度高字节 */
#define QMI8658_REG_AY_L          0x37    /**< Y轴加速度低字节 */
#define QMI8658_REG_AY_H          0x38    /**< Y轴加速度高字节 */
#define QMI8658_REG_AZ_L          0x39    /**< Z轴加速度低字节 */
#define QMI8658_REG_AZ_H          0x3A    /**< Z轴加速度高字节 */
#define QMI8658_REG_GX_L          0x3B    /**< X轴角速度低字节 */
#define QMI8658_REG_GX_H          0x3C    /**< X轴角速度高字节 */
#define QMI8658_REG_GY_L          0x3D    /**< Y轴角速度低字节 */
#define QMI8658_REG_GY_H          0x3E    /**< Y轴角速度高字节 */
#define QMI8658_REG_GZ_L          0x3F    /**< Z轴角速度低字节 */
#define QMI8658_REG_GZ_H          0x40    /**< Z轴角速度高字节 */

/*==============================================================
 *                         I2C 地址定义
 *==============================================================*/
#define QMI8658_I2C_ADDR_PRIMARY   0x6B    /**< SA0浮空/高电平时7位地址 */
#define QMI8658_I2C_ADDR_ALT      0x6A    /**< SA0接地时7位地址 */
#define QMI8658_I2C_ADDR          QMI8658_I2C_ADDR_PRIMARY  /**< 当前使用地址 */

/** 8位I2C写地址 = 7位地址 << 1 | 0 */
#define QMI8658_I2C_WRITE(addr)   ((addr) << 1)
/** 8位I2C读地址 = 7位地址 << 1 | 1 */
#define QMI8658_I2C_READ(addr)    (((addr) << 1) | 0x01)

/*==============================================================
 *                   加速度 ODR 配置 (CTRL2 高4位)
 *==============================================================*/
#define QMI8658_ACC_ODR_3HZ       0x0F    /**< 3Hz   */
#define QMI8658_ACC_ODR_11HZ      0x0E    /**< 11Hz  */
#define QMI8658_ACC_ODR_21HZ      0x0D    /**< 21Hz  */
#define QMI8658_ACC_ODR_29HZ      0x08    /**< 29Hz  */
#define QMI8658_ACC_ODR_58HZ      0x07    /**< 58Hz  */
#define QMI8658_ACC_ODR_117HZ     0x06    /**< 117Hz (默认) */
#define QMI8658_ACC_ODR_235HZ     0x05    /**< 235Hz */
#define QMI8658_ACC_ODR_470HZ     0x04    /**< 470Hz */
#define QMI8658_ACC_ODR_940HZ     0x03    /**< 940Hz */

/*==============================================================
 *                   陀螺仪 ODR 配置 (CTRL3 高4位)
 *==============================================================*/
#define QMI8658_GYRO_ODR_29HZ     0x08    /**< 29Hz  */
#define QMI8658_GYRO_ODR_58HZ     0x07    /**< 58Hz  */
#define QMI8658_GYRO_ODR_117HZ    0x06    /**< 117Hz (默认) */
#define QMI8658_GYRO_ODR_235HZ    0x05    /**< 235Hz */
#define QMI8658_GYRO_ODR_470HZ    0x04    /**< 470Hz */
#define QMI8658_GYRO_ODR_940HZ    0x03    /**< 940Hz */
#define QMI8658_GYRO_ODR_1880HZ   0x02    /**< 1880Hz */
#define QMI8658_GYRO_ODR_3760HZ   0x01    /**< 3760Hz */
#define QMI8658_GYRO_ODR_7520HZ   0x00    /**< 7520Hz */

/*==============================================================
 *                   加速度量程配置 (CTRL2 低4位)
 *==============================================================*/
#define QMI8658_ACC_RANGE_2G       0x00    /**< ±2G  ，灵敏度 16384 LSB/g */
#define QMI8658_ACC_RANGE_4G       0x10    /**< ±4G  ，灵敏度 8192 LSB/g  */
#define QMI8658_ACC_RANGE_8G       0x20    /**< ±8G  ，灵敏度 4096 LSB/g  */
#define QMI8658_ACC_RANGE_16G      0x30    /**< ±16G ，灵敏度 2048 LSB/g  */

/*==============================================================
 *                   陀螺音量程配置 (CTRL3 低4位)
 *==============================================================*/
#define QMI8658_GYRO_RANGE_16     0x00    /**< ±16°/s   ，灵敏度 2048 LSB/°/s */
#define QMI8658_GYRO_RANGE_32     0x10    /**< ±32°/s   ，灵敏度 1024 LSB/°/s */
#define QMI8658_GYRO_RANGE_64     0x20    /**< ±64°/s   ，灵敏度 512 LSB/°/s  */
#define QMI8658_GYRO_RANGE_125    0x30    /**< ±125°/s  ，灵敏度 256 LSB/°/s  */
#define QMI8658_GYRO_RANGE_250    0x40    /**< ±250°/s  ，灵敏度 128 LSB/°/s  */
#define QMI8658_GYRO_RANGE_512    0x50    /**< ±512°/s  ，灵敏度 64 LSB/°/s   */
#define QMI8658_GYRO_RANGE_1024   0x60    /**< ±1024°/s ，灵敏度 32 LSB/°/s   */
#define QMI8658_GYRO_RANGE_2048   0x70    /**< ±2048°/s ，灵敏度 16 LSB/°/s   */

/*==============================================================
 *                      初始化默认值
 *==============================================================*/
#define QMI8658_CTRL1_INIT        0x40    /**< 数字低通滤波器配置 */
#define QMI8658_CTRL2_INIT        0x07    /**< 当前模组已验证可出数的 bring-up 配置 */
#define QMI8658_CTRL3_INIT        0x07    /**< 当前模组已验证可出数的 bring-up 配置 */
#define QMI8658_CTRL5_INIT        0x11    /**< 加速度计配置 */
#define QMI8658_CTRL7_INIT        0x03    /**< 当前模组已验证可出数的 bring-up 配置 */

/*==============================================================
 *                      芯片标识值
 *==============================================================*/
#define QMI8658_CHIP_ID_VALUE     0x05    /**< WHO_AM_I 寄存器期望值 */
#define QMI8658_RESET_STATE_READY  0x80    /**< 复位完成后 RESET_STATE 应为 0x80 */

/*==============================================================
 *                      超时常量
 *==============================================================*/
#define QMI8658_RESET_DELAY_MS     500     /**< 软复位后等待时间 (ms)，数据手册要求~15ms，此处留安全余量 */
#define QMI8658_PWR_UP_DELAY_MS    500     /**< 上电稳定等待时间 (ms) */
#define QMI8658_INIT_RETRY_MAX     3       /**< 初始化重试次数 */
#define QMI8658_ENABLE_DELAY_MS    30      /**< 使能传感器后等待数据通路稳定 */
#define QMI8658_READY_TIMEOUT_MS   200     /**< 传感器就绪轮询超时 */

/*==============================================================
 *                      状态位宏
 *==============================================================*/
#define QMI8658_STATUSINT_AVAIL    0x01    /**< DRDY/数据可用状态位 */
#define QMI8658_STATUS0_A_DA       0x01    /**< 加速度数据就绪位 */
#define QMI8658_STATUS0_G_DA       0x02    /**< 陀螺仪数据就绪位 */
#define QMI8658_STATUS0_TEMP_DA   0x04    /**< 温度数据就绪位 */

/*==============================================================
 *                      数据有效性宏
 *==============================================================*/
/** 检查三轴加速度是否全为0 (无效) */
#define QMI8658_ACC_IS_ZERO(x, y, z)     (((x)==0) && ((y)==0) && ((z)==0))
/** 检查三轴陀螺仪是否全为0 (无效) */
#define QMI8658_GYRO_IS_ZERO(x, y, z)    (((x)==0) && ((y)==0) && ((z)==0))
/** 检查三轴数据是否全为-1 (无效) */
#define QMI8658_DATA_IS_INVALID(x, y, z) (((x)==-1) && ((y)==-1) && ((z)==-1))

/*==============================================================
 *                         外部变量声明
 *==============================================================*/
extern u8 QMI8658_I2C_Addr;       /**< 当前使用的I2C地址 */

/*==============================================================
 *                      函数声明
 *==============================================================*/

/**
 * @brief   初始化 QMI8658 IMU
 * @return  0=成功，-1=失败
 *
 * @details
 * 完整初始化流程:
 *   1. LOG 初始化开始
 *   2. 尝试主地址 (0x6B)，失败则尝试备地址 (0x6A)
 *   3. 读取 WHO_AM_I 确认芯片 (0x05)
 *   4. 检查 RESET_STATE: 0x80=已就绪, 0x00=需软复位
 *   5. 软复位 (写入 RESET=0xB0) → 等待 500ms
 *   6. 分阶段写入控制寄存器 (CTRL7→0x00 → CTRL1/2/3/5 → CTRL7→0x03)
 *   7. 等待数据就绪 (STATUS0.aDA=1)
 *   8. LOG 初始化完成
 *
 * @note    必须在 I2C_config() 初始化完成后调用
 */
s8 QMI8658_Init(void);

/**
 * @brief      读取 QMI8658 芯片ID
 * @return     WHO_AM_I 值 (0x05=正确)，0xFF=读取失败
 */
u8 QMI8658_ReadID(void);

/**
 * @brief      读取加速度数据
 * @param[out] x  指向X轴加速度的指针 (int16, LSB/g)
 * @param[out] y  指向Y轴加速度的指针 (int16, LSB/g)
 * @param[out] z  指向Z轴加速度的指针 (int16, LSB/g)
 * @return      0=成功，-1=数据无效或读取失败
 *
 * @details
 * 连续读取 6 字节 (0x35~0x3A)，小端序组装为 int16。
 * 默认量程 ±4G，灵敏度 8192 LSB/g，0x00FF≈0.02g。
 *
 * @example
 * @code
 * int16 ax, ay, az;
 * if (QMI8658_ReadAcc(&ax, &ay, &az) == 0) {
 *     LOGI("IMU", "ax=%d ay=%d az=%d", ax, ay, az);
 * }
 * @endcode
 */
s8 QMI8658_ReadAcc(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取陀螺仪数据
 * @param[out] x  指向X轴角速度的指针 (int16, LSB/°/s)
 * @param[out] y  指向Y轴角速度的指针 (int16, LSB/°/s)
 * @param[out] z  指向Z轴角速度的指针 (int16, LSB/°/s)
 * @return      0=成功，-1=数据无效或读取失败
 *
 * @details
 * 连续读取 6 字节 (0x3B~0x40)，小端序组装为 int16。
 * 默认量程 ±128°/s，灵敏度 256 LSB/°/s。
 *
 * @example
 * @code
 * int16 gx, gy, gz;
 * if (QMI8658_ReadGyro(&gx, &gy, &gz) == 0) {
 *     LOGI("IMU", "gx=%d gy=%d gz=%d", gx, gy, gz);
 * }
 * @endcode
 */
s8 QMI8658_ReadGyro(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取低通滤波后的陀螺仪数据
 * @param[out] x  指向X轴角速度的指针 (int16, LSB/°/s)
 * @param[out] y  指向Y轴角速度的指针 (int16, LSB/°/s)
 * @param[out] z  指向Z轴角速度的指针 (int16, LSB/°/s)
 * @return      0=成功，-1=原始数据无效、滤波失败或读取失败
 *
 * @details
 * 先调用 QMI8658_ReadGyro() 获取原始三轴角速度，
 * 再通过 Function/Filter 模块执行软件低通滤波。
 * 首帧有效数据直接作为滤波输出，后续帧逐步平滑。
 *
 * @example
 * @code
 * int16 gx, gy, gz;
 * if (QMI8658_ReadGyroFiltered(&gx, &gy, &gz) == 0) {
 *     LOGI("IMU", "gyro_f=%d %d %d", gx, gy, gz);
 * }
 * @endcode
 */
s8 QMI8658_ReadGyroFiltered(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取温度数据
 * @param[out] temp  指向温度值的指针 (int16, 25°C=0, 实际值=temp/256+25)
 * @return      0=成功，-1=读取失败
 *
 * @details
 * 读取 2 字节温度数据 (0x33~0x34)，int12 有符号数，0x0000=0°C，0x0100=256°C。
 * 换算公式: 实际温度(°C) = reg_value / 256.0 + 25
 * 注意: STC32G 无 FPU，禁止使用浮点运算，外部应用需自行转换
 *
 * @example
 * @code
 * int16 temp_raw;
 * if (QMI8658_ReadTemp(&temp_raw) == 0) {
 *     int temp_c = ((int32)temp_raw * 10 / 256) + 250; // 精度 0.1°C
 *     LOGI("IMU", "temp=%d.%d", temp_c/10, temp_c%10);
 * }
 * @endcode
 */
s8 QMI8658_ReadTemp(int16 *temp);

/**
 * @brief      读取完整9轴数据 (加速度+陀螺仪)
 * @param[out] ax  指向X轴加速度的指针
 * @param[out] ay  指向Y轴加速度的指针
 * @param[out] az  指向Z轴加速度的指针
 * @param[out] gx  指向X轴角速度的指针
 * @param[out] gy  指向Y轴角速度的指针
 * @param[out] gz  指向Z轴角速度的指针
 * @return      0=成功，-1=数据无效或读取失败
 *
 * @details
 * 使用 I2C 地址自动递增，一次读取 12 字节 (0x35~0x40)。
 * 效率高于分别调用 ReadAcc + ReadGyro。
 *
 * @example
 * @code
 * int16 ax, ay, az, gx, gy, gz;
 * if (QMI8658_ReadAll(&ax, &ay, &az, &gx, &gy, &gz) == 0) {
 *     LOGI("IMU", "a=%d %d %d g=%d %d %d", ax, ay, az, gx, gy, gz);
 * }
 * @endcode
 */
s8 QMI8658_ReadAll(int16 *ax, int16 *ay, int16 *az,
                   int16 *gx, int16 *gy, int16 *gz);

/**
 * @brief      轮询等待加速度数据就绪
 * @param[in]  timeout_ms  超时时间 (ms)
 * @return     0=数据就绪，-1=超时
 *
 * @details
 * 轮询 STATUS0 寄存器的 Bit0 (aDA)，等待加速度数据就绪。
 * 适用于需要同步等待数据到达的场景。
 */
s8 QMI8658_Wait_AccReady(u16 timeout_ms);

/**
 * @brief      轮询等待陀螺仪数据就绪
 * @param[in]  timeout_ms  超时时间 (ms)
 * @return     0=数据就绪，-1=超时
 *
 * @details
 * 轮询 STATUS0 寄存器的 Bit1 (gDA)，等待陀螺仪数据就绪。
 */
s8 QMI8658_Wait_GyroReady(u16 timeout_ms);

/**
 * @brief   执行 I2C 总线恢复
 * @return  none
 *
 * @details
 * 当 I2C 总线异常 (SDA/SCL 被拉低) 时，尝试恢复总线。
 * 禁用 I2C 模块，手动发送 9 个时钟脉冲迫使从机释放 SDA，最后发送 STOP。
 *
 * @note    此函数直接操作引脚，适用于 I2C 总线死锁场景
 */
void QMI8658_BusRecover(void);

/**
 * @brief   使能 QMI8658 IMU 传感器
 * @return  0=成功，-1=失败
 *
 * @details 写入 CTRL7=0x03，使能加速度计和陀螺仪
 */
s8 QMI8658_Enable(void);

/**
 * @brief   禁用 QMI8658 IMU 传感器
 * @return  0=成功，-1=失败
 *
 * @details 写入 CTRL7=0x00，禁用加速度计和陀螺仪，进入低功耗模式
 */
s8 QMI8658_Disable(void);

#endif
