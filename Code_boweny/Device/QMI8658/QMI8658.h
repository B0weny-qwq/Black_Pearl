/**
 * @file    QMI8658.h
 * @brief   QMI8658 六轴 IMU 驱动寄存器定义与接口声明。
 * @author  boweny
 * @date    2026-05-05
 * @version v1.1
 *
 * @details
 * 提供 QMI8658 加速度计、陀螺仪和温度数据读取接口，支持主/备用 I2C
 * 地址探测、传感器使能、数据就绪轮询、软件低通读取和 bring-up 诊断。
 *
 * @hardware
 * - I2C: P1.4(SDA) / P1.5(SCL)
 * - 系统时钟: Fosc = 24MHz
 *
 * @note    调用前必须先完成 I2C_config()。
 * @note    输出数据均为 int16 定点整数，避免在 STC32G 上引入浮点运算。
 *
 * @see     Code_boweny/Device/QMI8658/QMI8658.c
 */

#ifndef __QMI8658_H__
#define __QMI8658_H__

#include "..\..\..\Driver\inc\STC32G_I2C.h"
#include "..\..\..\Driver\inc\STC32G_Delay.h"
#include "..\..\..\Driver\inc\STC32G_GPIO.h"
#include "..\..\Function\Log\Log.h"

#define QMI8658_REG_WHO_AM_I      0x00    /**< 芯片标识寄存器，只读，期望值 0x05。 */
#define QMI8658_REG_REVISION_ID    0x01    /**< 芯片版本号寄存器，只读。 */
#define QMI8658_REG_RESET_STATE    0x4D    /**< 复位状态寄存器，只读，0x80 表示复位完成。 */
#define QMI8658_REG_CTRL1         0x02    /**< 控制寄存器 1，配置地址自增和数据格式。 */
#define QMI8658_REG_CTRL2         0x03    /**< 控制寄存器 2，配置加速度计量程和 ODR。 */
#define QMI8658_REG_CTRL3         0x04    /**< 控制寄存器 3，配置陀螺仪量程和 ODR。 */
#define QMI8658_REG_CTRL4         0x05    /**< 控制寄存器 4，保留配置。 */
#define QMI8658_REG_CTRL5         0x06    /**< 控制寄存器 5，加速度计滤波相关配置。 */
#define QMI8658_REG_CTRL6         0x07    /**< 控制寄存器 6，保留/扩展数据通路配置。 */
#define QMI8658_REG_CTRL7         0x08    /**< 控制寄存器 7，传感器使能位，Bit1=gEN，Bit0=aEN。 */
#define QMI8658_REG_CTRL8         0x09    /**< 控制寄存器 8，FIFO/ODR 扩展配置。 */
#define QMI8658_REG_CTRL9         0x0A    /**< 控制寄存器 9，命令和扩展控制。 */
#define QMI8658_REG_FIFO_WTM      0x13    /**< FIFO 水位阈值寄存器。 */
#define QMI8658_REG_FIFO_CTRL     0x14    /**< FIFO 控制寄存器。 */
#define QMI8658_REG_FIFO_STATUS   0x16    /**< FIFO 状态寄存器。 */
#define QMI8658_REG_RESET         0x60    /**< 软复位寄存器，只写，写 0xB0 触发复位。 */
#define QMI8658_REG_STATUSINT     0x2D    /**< 中断状态寄存器。 */
#define QMI8658_REG_STATUS0       0x2E    /**< 数据就绪状态寄存器。 */
#define QMI8658_REG_TIMESTAMP_L   0x30    /**< 时间戳低字节寄存器。 */
#define QMI8658_REG_TIMESTAMP_M   0x31    /**< 时间戳中字节寄存器。 */
#define QMI8658_REG_TIMESTAMP_H   0x32    /**< 时间戳高字节寄存器。 */
#define QMI8658_REG_TEMP_L        0x33    /**< 温度数据低字节寄存器。 */
#define QMI8658_REG_TEMP_H        0x34    /**< 温度数据高字节寄存器。 */
#define QMI8658_REG_AX_L          0x35    /**< X 轴加速度低字节寄存器。 */
#define QMI8658_REG_AX_H          0x36    /**< X 轴加速度高字节寄存器。 */
#define QMI8658_REG_AY_L          0x37    /**< Y 轴加速度低字节寄存器。 */
#define QMI8658_REG_AY_H          0x38    /**< Y 轴加速度高字节寄存器。 */
#define QMI8658_REG_AZ_L          0x39    /**< Z 轴加速度低字节寄存器。 */
#define QMI8658_REG_AZ_H          0x3A    /**< Z 轴加速度高字节寄存器。 */
#define QMI8658_REG_GX_L          0x3B    /**< X 轴陀螺仪低字节寄存器。 */
#define QMI8658_REG_GX_H          0x3C    /**< X 轴陀螺仪高字节寄存器。 */
#define QMI8658_REG_GY_L          0x3D    /**< Y 轴陀螺仪低字节寄存器。 */
#define QMI8658_REG_GY_H          0x3E    /**< Y 轴陀螺仪高字节寄存器。 */
#define QMI8658_REG_GZ_L          0x3F    /**< Z 轴陀螺仪低字节寄存器。 */
#define QMI8658_REG_GZ_H          0x40    /**< Z 轴陀螺仪高字节寄存器。 */

#define QMI8658_I2C_ADDR_PRIMARY   0x6B    /**< SA0 浮空或高电平时的 7 位 I2C 地址。 */
#define QMI8658_I2C_ADDR_ALT      0x6A    /**< SA0 接地时的 7 位 I2C 地址。 */
#define QMI8658_I2C_ADDR          QMI8658_I2C_ADDR_PRIMARY  /**< 默认使用的 I2C 地址。 */
#define QMI8658_I2C_WRITE(addr)   ((addr) << 1)             /**< 由 7 位地址生成 8 位 I2C 写地址。 */
#define QMI8658_I2C_READ(addr)    (((addr) << 1) | 0x01)    /**< 由 7 位地址生成 8 位 I2C 读地址。 */

#define QMI8658_ACC_ODR_3HZ       0x0F    /**< 加速度计输出数据率 3Hz。 */
#define QMI8658_ACC_ODR_11HZ      0x0E    /**< 加速度计输出数据率 11Hz。 */
#define QMI8658_ACC_ODR_21HZ      0x0D    /**< 加速度计输出数据率 21Hz。 */
#define QMI8658_ACC_ODR_29HZ      0x08    /**< 加速度计输出数据率 29Hz。 */
#define QMI8658_ACC_ODR_58HZ      0x07    /**< 加速度计输出数据率 58Hz。 */
#define QMI8658_ACC_ODR_117HZ     0x06    /**< 加速度计输出数据率 117Hz。 */
#define QMI8658_ACC_ODR_235HZ     0x05    /**< 加速度计输出数据率 235Hz。 */
#define QMI8658_ACC_ODR_470HZ     0x04    /**< 加速度计输出数据率 470Hz。 */
#define QMI8658_ACC_ODR_940HZ     0x03    /**< 加速度计输出数据率 940Hz。 */

#define QMI8658_GYRO_ODR_29HZ     0x08    /**< 陀螺仪输出数据率 29Hz。 */
#define QMI8658_GYRO_ODR_58HZ     0x07    /**< 陀螺仪输出数据率 58Hz。 */
#define QMI8658_GYRO_ODR_117HZ    0x06    /**< 陀螺仪输出数据率 117Hz。 */
#define QMI8658_GYRO_ODR_235HZ    0x05    /**< 陀螺仪输出数据率 235Hz。 */
#define QMI8658_GYRO_ODR_470HZ    0x04    /**< 陀螺仪输出数据率 470Hz。 */
#define QMI8658_GYRO_ODR_940HZ    0x03    /**< 陀螺仪输出数据率 940Hz。 */
#define QMI8658_GYRO_ODR_1880HZ   0x02    /**< 陀螺仪输出数据率 1880Hz。 */
#define QMI8658_GYRO_ODR_3760HZ   0x01    /**< 陀螺仪输出数据率 3760Hz。 */
#define QMI8658_GYRO_ODR_7520HZ   0x00    /**< 陀螺仪输出数据率 7520Hz。 */

#define QMI8658_ACC_RANGE_2G       0x00    /**< 加速度计量程 +/-2g。 */
#define QMI8658_ACC_RANGE_4G       0x10    /**< 加速度计量程 +/-4g。 */
#define QMI8658_ACC_RANGE_8G       0x20    /**< 加速度计量程 +/-8g。 */
#define QMI8658_ACC_RANGE_16G      0x30    /**< 加速度计量程 +/-16g。 */
#define QMI8658_GYRO_RANGE_16     0x00    /**< 陀螺仪量程 +/-16dps。 */
#define QMI8658_GYRO_RANGE_32     0x10    /**< 陀螺仪量程 +/-32dps。 */
#define QMI8658_GYRO_RANGE_64     0x20    /**< 陀螺仪量程 +/-64dps。 */
#define QMI8658_GYRO_RANGE_125    0x30    /**< 陀螺仪量程 +/-125/128dps。 */
#define QMI8658_GYRO_RANGE_250    0x40    /**< 陀螺仪量程 +/-250dps。 */
#define QMI8658_GYRO_RANGE_512    0x50    /**< 陀螺仪量程 +/-512dps。 */
#define QMI8658_GYRO_RANGE_1024   0x60    /**< 陀螺仪量程 +/-1024dps。 */
#define QMI8658_GYRO_RANGE_2048   0x70    /**< 陀螺仪量程 +/-2048dps。 */

#define QMI8658_CTRL1_INIT        0x40    /**< 初始化 CTRL1：地址自增，小端数据。 */
#define QMI8658_CTRL2_INIT        0x07    /**< 初始化 CTRL2：当前 bring-up 实测配置。 */
#define QMI8658_CTRL3_INIT        0x07    /**< 初始化 CTRL3：当前 bring-up 实测配置。 */
#define QMI8658_CTRL5_INIT        0x11    /**< 初始化 CTRL5：加速度计滤波配置。 */
#define QMI8658_CTRL6_INIT        0x00    /**< 初始化 CTRL6：关闭扩展数据通路。 */
#define QMI8658_CTRL7_INIT        0x03    /**< 初始化 CTRL7：使能加速度计和陀螺仪。 */
#define QMI8658_CTRL8_INIT        0x00    /**< 初始化 CTRL8：关闭 FIFO/同步采样扩展功能。 */
#define QMI8658_CLEAR_DATAPATH_ENABLE 0   /**< 初始化时是否清理扩展数据通路。 */
#define QMI8658_CTRL9_CMD_ACK     0x00    /**< CTRL9 命令 ACK/NOP。 */
#define QMI8658_CTRL9_CMD_RST_FIFO 0x04   /**< CTRL9 重置 FIFO 命令。 */
#define QMI8658_FIFO_WTM_INIT     0x00    /**< 初始化 FIFO 水位阈值。 */
#define QMI8658_FIFO_CTRL_BYPASS  0x00    /**< FIFO bypass 模式。 */
#define QMI8658_CHIP_ID_VALUE     0x05    /**< WHO_AM_I 期望值。 */
#define QMI8658_RESET_STATE_READY  0x80    /**< RESET_STATE 就绪值。 */
#define QMI8658_RESET_DELAY_MS     500     /**< 软复位后等待时间，单位 ms。 */
#define QMI8658_PWR_UP_DELAY_MS    500     /**< 上电稳定等待时间，单位 ms。 */
#define QMI8658_INIT_RETRY_MAX     3       /**< 初始化最大重试次数。 */
#define QMI8658_ENABLE_DELAY_MS    30      /**< 传感器使能后等待时间，单位 ms。 */
#define QMI8658_READY_TIMEOUT_MS   200     /**< 数据就绪轮询超时时间，单位 ms。 */
#define QMI8658_STATUSINT_AVAIL    0x01    /**< 中断/数据可用状态位。 */
#define QMI8658_STATUS0_A_DA       0x01    /**< 加速度数据就绪位。 */
#define QMI8658_STATUS0_G_DA       0x02    /**< 陀螺仪数据就绪位。 */
#define QMI8658_STATUS0_TEMP_DA   0x04    /**< 温度数据就绪位。 */
#define QMI8658_ACC_IS_ZERO(x, y, z)     (((x)==0) && ((y)==0) && ((z)==0))       /**< 判断三轴加速度是否全 0。 */
#define QMI8658_GYRO_IS_ZERO(x, y, z)    (((x)==0) && ((y)==0) && ((z)==0))       /**< 判断三轴陀螺仪是否全 0。 */
#define QMI8658_DATA_IS_INVALID(x, y, z) (((x)==-1) && ((y)==-1) && ((z)==-1))    /**< 判断三轴数据是否为无效哨兵值。 */

extern u8 QMI8658_I2C_Addr;       /**< 当前实际使用的 7 位 I2C 地址。 */

/**
 * @brief   初始化 QMI8658 IMU。
 * @return  0=成功，-1=失败。
 */
s8 QMI8658_Init(void);

/**
 * @brief   读取 QMI8658 芯片 ID。
 * @return  WHO_AM_I 值；0xFF 表示读取失败。
 */
u8 QMI8658_ReadID(void);

/**
 * @brief      读取三轴加速度原始数据。
 * @param[out] x  X 轴加速度输出指针。
 * @param[out] y  Y 轴加速度输出指针。
 * @param[out] z  Z 轴加速度输出指针。
 * @return     0=成功，-1=空指针、数据无效或读取失败。
 */
s8 QMI8658_ReadAcc(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取三轴陀螺仪原始数据。
 * @param[out] x  X 轴角速度输出指针。
 * @param[out] y  Y 轴角速度输出指针。
 * @param[out] z  Z 轴角速度输出指针。
 * @return     0=成功，-1=空指针、数据无效或读取失败。
 */
s8 QMI8658_ReadGyro(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取低通滤波后的三轴陀螺仪数据。
 * @param[out] x  X 轴滤波后角速度输出指针。
 * @param[out] y  Y 轴滤波后角速度输出指针。
 * @param[out] z  Z 轴滤波后角速度输出指针。
 * @return     0=成功，-1=原始读取失败、滤波失败或参数无效。
 */
s8 QMI8658_ReadGyroFiltered(int16 *x, int16 *y, int16 *z);

/**
 * @brief      读取温度原始数据。
 * @param[out] temp  温度原始值输出指针。
 * @return     0=成功，-1=读取失败或空指针。
 */
s8 QMI8658_ReadTemp(int16 *temp);

/**
 * @brief      一次性读取加速度和陀螺仪六轴数据。
 * @param[out] ax  X 轴加速度输出指针。
 * @param[out] ay  Y 轴加速度输出指针。
 * @param[out] az  Z 轴加速度输出指针。
 * @param[out] gx  X 轴角速度输出指针。
 * @param[out] gy  Y 轴角速度输出指针。
 * @param[out] gz  Z 轴角速度输出指针。
 * @return     0=成功，-1=空指针、数据无效或读取失败。
 */
s8 QMI8658_ReadAll(int16 *ax, int16 *ay, int16 *az,
                   int16 *gx, int16 *gy, int16 *gz);

/**
 * @brief      轮询等待加速度数据就绪。
 * @param[in]  timeout_ms  超时时间，单位 ms。
 * @return     0=就绪，-1=超时。
 */
s8 QMI8658_Wait_AccReady(u16 timeout_ms);

/**
 * @brief      轮询等待陀螺仪数据就绪。
 * @param[in]  timeout_ms  超时时间，单位 ms。
 * @return     0=就绪，-1=超时。
 */
s8 QMI8658_Wait_GyroReady(u16 timeout_ms);

/**
 * @brief   尝试恢复异常 I2C 总线。
 * @return  none
 */
void QMI8658_BusRecover(void);

/**
 * @brief   使能 QMI8658 加速度计和陀螺仪。
 * @return  0=成功，-1=失败。
 */
s8 QMI8658_Enable(void);

/**
 * @brief   禁用 QMI8658 加速度计和陀螺仪。
 * @return  0=成功，-1=失败。
 */
s8 QMI8658_Disable(void);

/**
 * @brief   打印 QMI8658 关键寄存器原始值。
 * @return  none
 */
void QMI8658_DumpRawRegs(void);

/**
 * @brief   获取最近一次 QMI8658 底层 I2C 错误码。
 * @return  内部错误码，0=无错误，非 0=传输或协议错误。
 */
u8 QMI8658_GetLastI2cError(void);

/**
 * @brief   获取最近一次 QMI8658 底层 I2C 错误名称。
 * @return  指向静态错误名称字符串的指针。
 */
char *QMI8658_GetLastI2cErrorName(void);

#undef QMI8658_RESET_DELAY_MS
#undef QMI8658_PWR_UP_DELAY_MS
#undef QMI8658_ENABLE_DELAY_MS
#undef QMI8658_READY_TIMEOUT_MS
#define QMI8658_LEGACY_EXACT_TEST 0     /**< 旧版精确复测模式开关。 */
#define QMI8658_SOFT_RESET_ENABLE 0     /**< 初始化时是否执行软复位。 */
#define QMI8658_DIAG_ENABLE       0     /**< 初始化诊断日志开关。 */
#define QMI8658_RESET_DELAY_MS    2000  /**< 覆盖后的软复位等待时间，单位 ms。 */
#define QMI8658_PWR_UP_DELAY_MS   500   /**< 覆盖后的上电等待时间，单位 ms。 */
#define QMI8658_ENABLE_DELAY_MS   30    /**< 覆盖后的使能等待时间，单位 ms。 */
#define QMI8658_READY_TIMEOUT_MS  200   /**< 覆盖后的数据就绪超时时间，单位 ms。 */

#endif
