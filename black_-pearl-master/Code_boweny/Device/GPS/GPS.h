/**
 * @file    GPS.h
 * @brief   GPS NMEA0183 解析模块 — UART2 接收与状态读取接口
 * @author  boweny
 * @date    2026-04-24
 * @version v1.0
 *
 * @details
 * - 基于 UART2 接收 GPS / 北斗 / 多模 GNSS 的 NMEA0183 语句
 * - 使用 Driver 层 UART2 中断缓冲区作为输入源，在模块内完成 FIFO 缓冲与逐字符状态机解析
 * - 支持 GGA / RMC / GSA / GSV / VTG 五类标准语句
 * - 所有位置、速度、航向和精度字段均使用定点整数保存，禁止浮点运算
 *
 * @hardware
 *   - UART2: RX=P1.0 / TX=P1.1
 *   - 波特率发生器: Timer2（UART2 固定占用）
 *   - 系统时钟: Fosc = 24MHz
 *
 * @note    本模块不修改 Driver 层 ISR，接收链路为 RX2_Buffer -> GPS FIFO -> NMEA Parser
 * @note    调用顺序: GPS_Init() -> 在主循环中高频调用 GPS_Poll() -> GPS_GetState()
 *
 * @see     Code_boweny/Device/GPS/GPS.c
 */

#ifndef __GPS_H__
#define __GPS_H__

#include "config.h"

/*---------------------------------- 编译期配置 ----------------------------------*/

#define GPS_BAUDRATE               115200UL
#define GPS_RAW_ECHO_ENABLE        0U
#define GPS_UART_FIFO_SIZE         256U
#define GPS_SENTENCE_BUFFER_SIZE   96U
#define GPS_MAX_FIELDS             24U

/*---------------------------------- Talker 定义 ---------------------------------*/

#define GPS_TALKER_UNKNOWN         0U
#define GPS_TALKER_GP              1U
#define GPS_TALKER_BD              2U
#define GPS_TALKER_GN              3U

/*---------------------------------- 状态结构 ------------------------------------*/

typedef struct
{
    char  talker[3];
    u8    talker_id;
    u8    fix_valid;
    u8    fix_quality;
    u8    rmc_status;
    u8    fix_mode;

    u8    utc_hour;
    u8    utc_minute;
    u8    utc_second;
    u16   utc_millisecond;

    u8    date_day;
    u8    date_month;
    u8    date_year;

    int32 lat_deg1e7;
    int32 lon_deg1e7;

    u32   speed_knots_x100;
    u32   speed_kmh_x100;
    u16   course_deg_x100;

    u8    satellites_used;
    u8    satellites_view;
    u16   hdop_x100;
    u16   pdop_x100;
    u16   vdop_x100;
    int32 altitude_cm;
    u8    max_snr;

    u32   update_sequence;

    u16   sentence_ok_count;
    u16   checksum_error_count;
    u16   parse_error_count;
    u16   uart_overflow_count;
    u16   fifo_overflow_count;
    u16   sentence_overflow_count;
} GPS_State_t;

/*---------------------------------- 对外接口 ------------------------------------*/

/**
 * @brief   初始化 GPS 模块
 * @return  0=成功，-1=失败
 *
 * @details
 * - 将 UART2 路由切换到 P1.0/P1.1
 * - 初始化 UART2 为 8N1 接收模式，波特率默认使用 GPS_BAUDRATE
 * - 清空模块内部 FIFO、解析状态和 GPS_State_t
 */
s8 GPS_Init(void);

/**
 * @brief   复位 GPS 模块运行状态
 * @return  none
 *
 * @details
 * 清空 FIFO、语句缓存、解析器状态与 GPS_State_t，
 * 不重新初始化 UART2 外设。
 */
void GPS_Reset(void);

/**
 * @brief   轮询处理 GPS 接收数据
 * @return  none
 *
 * @details
 * 从 Driver 层 RX2_Buffer 拉取增量字节，写入模块 FIFO，
 * 再逐字节驱动 NMEA 状态机解析。
 */
void GPS_Poll(void);

/**
 * @brief      获取当前 GPS 状态快照
 * @return     指向只读状态结构体的指针
 *
 * @details
 * 返回模块内部维护的最新状态结构体，
 * 调用者不得修改该结构体内容。
 */
const GPS_State_t *GPS_GetState(void);

#endif
