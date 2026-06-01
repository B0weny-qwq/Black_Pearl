/**
 * @file    Log.h
 * @brief   UART1 轻量级日志输出接口。
 * @author  boweny
 * @date    2026-05-05
 * @version v1.0
 *
 * @details
 * 基于 UART1 提供带标签、分级别的格式化日志输出。支持 INFO、WARN、
 * ERROR、DEBUG 四个级别，以及无级别的原始 printf 风格输出。
 *
 * 当前关系边界：
 * - 本模块只负责 UART1 文本输出，不负责日志节流策略；
 * - 各模块的限频、viewer 兼容标签和诊断开关由上层各自维护；
 * - 上位机 `ship_log_viewer` 依赖现有 `[TAG] I/W/E/D:` 口径解析。
 *
 * @note    使用前必须先初始化 UART1，再调用 log_init() 使能日志输出。
 * @note    STC32G 工程中避免使用 %f，请将浮点量转换为整数后再输出。
 *
 * @see     Code_boweny/Function/Log/Log.c
 */

#ifndef __LOG_H__
#define __LOG_H__

/**
 * @note 当前职责边界：
 * - 本模块只负责 UART1 文本日志输出接口；
 * - 日志触发频率、业务标签含义和 viewer 兼容口径由各上层模块自行维护；
 * - 本模块不参与无线协议、控制策略或诊断判定逻辑。
 */

#include "..\..\..\Driver\inc\STC32G_UART.h"
#include <stdarg.h>

/* Compile-time log filtering. Lower levels are more verbose. */
#define LOG_LEVEL_DEBUG  0U
#define LOG_LEVEL_INFO   1U
#define LOG_LEVEL_WARN   2U
#define LOG_LEVEL_ERROR  3U
#define LOG_LEVEL_NONE   4U

#ifndef SERIAL_LOG_LEVEL
#define SERIAL_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#if (SERIAL_LOG_LEVEL <= LOG_LEVEL_INFO)
#define LOGI(tag, ...)   log_info(tag, __VA_ARGS__)   /**< 输出 INFO 级别日志。 */
#else
#define LOGI(tag, ...)
#endif

#if (SERIAL_LOG_LEVEL <= LOG_LEVEL_WARN)
#define LOGW(tag, ...)   log_warn(tag, __VA_ARGS__)   /**< 输出 WARN 级别日志。 */
#else
#define LOGW(tag, ...)
#endif

#if (SERIAL_LOG_LEVEL <= LOG_LEVEL_ERROR)
#define LOGE(tag, ...)   log_error(tag, __VA_ARGS__)  /**< 输出 ERROR 级别日志。 */
#else
#define LOGE(tag, ...)
#endif

#if (SERIAL_LOG_LEVEL <= LOG_LEVEL_DEBUG)
#define LOGD(tag, ...)   log_debug(tag, __VA_ARGS__)  /**< 输出 DEBUG 级别日志。 */
#else
#define LOGD(tag, ...)
#endif

/**
 * @brief   初始化日志系统。
 * @return  none
 *
 * @details
 * 置位内部日志就绪标志，使 LOGI/LOGW/LOGE/LOGD 和 log_printf 输出生效。
 */
void log_init(void);

/**
 * @brief      输出带标签的 INFO 级别日志。
 * @param[in]  tag  日志标签字符串，例如 "IMU"。
 * @param[in]  fmt  printf 风格格式化字符串。
 * @param[in]  ...  可变参数。
 * @return     none
 */
void log_info(u8 *tag, u8 *fmt, ...);

/**
 * @brief      输出带标签的 WARN 级别日志。
 * @param[in]  tag  日志标签字符串。
 * @param[in]  fmt  printf 风格格式化字符串。
 * @param[in]  ...  可变参数。
 * @return     none
 */
void log_warn(u8 *tag, u8 *fmt, ...);

/**
 * @brief      输出带标签的 ERROR 级别日志。
 * @param[in]  tag  日志标签字符串。
 * @param[in]  fmt  printf 风格格式化字符串。
 * @param[in]  ...  可变参数。
 * @return     none
 */
void log_error(u8 *tag, u8 *fmt, ...);

/**
 * @brief      输出带标签的 DEBUG 级别日志。
 * @param[in]  tag  日志标签字符串。
 * @param[in]  fmt  printf 风格格式化字符串。
 * @param[in]  ...  可变参数。
 * @return     none
 */
void log_debug(u8 *tag, u8 *fmt, ...);

/**
 * @brief      输出无标签、无级别的原始日志。
 * @param[in]  fmt  printf 风格格式化字符串。
 * @param[in]  ...  可变参数。
 * @return     none
 */
void log_printf(u8 *fmt, ...);

#endif
