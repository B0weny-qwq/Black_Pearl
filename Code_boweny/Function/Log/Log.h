/**
 * @file    Log.h
 * @brief   轻量级 UART1 日志输出模块 — 带标签、分级别的日志功能
 * @author  boweny
 * @date    2026-04-22
 * @version v1.0
 *
 * @details
 * - 基于 UART1 (P3.1=TXD, P3.0=RXD) 的轻量级日志输出
 * - 提供 4 级日志: INFO / WARN / ERROR / DEBUG
 * - 最大支持 127 字符格式化消息 (不含 \r\n)
 * - 使用前必须先初始化 UART1，再调用 log_init()
 *
 * @note    禁止使用浮点数 (%f) 格式化，请先将浮点值转为整数
 * @note    log_init() 必须在 UART1_Init() 之后调用
 *
 * @see     Code_boweny/Function/Log/Log.c
 */

#ifndef __LOG_H__
#define __LOG_H__

#include "..\..\..\Driver\inc\STC32G_UART.h"
#include <stdarg.h>

/*---------------------------------- 日志级别宏 ----------------------------------*/

#define LOGI(tag, ...)   log_info(tag, __VA_ARGS__)
#define LOGW(tag, ...)   log_warn(tag, __VA_ARGS__)
#define LOGE(tag, ...)   log_error(tag, __VA_ARGS__)
#define LOGD(tag, ...)   log_debug(tag, __VA_ARGS__)

/*---------------------------------- 函数声明 ----------------------------------*/

/**
 * @brief   初始化日志系统
 * @return  none
 *
 * @details 将 log_ready 标志置 1，使所有 LOG* 函数生效
 * @note    必须在 UART1_Init() 之后调用
 */
void log_init(void);

/**
 * @brief      带标签的 INFO 级别日志
 * @param[in]  tag   日志标签 (如 "IMU", "I2C")
 * @param[in]  fmt   格式化字符串
 * @param[in]  ...   可变参数
 * @return     none
 */
void log_info(u8 *tag, u8 *fmt, ...);

/**
 * @brief      带标签的 WARN 级别日志
 * @param[in]  tag   日志标签
 * @param[in]  fmt   格式化字符串
 * @param[in]  ...   可变参数
 * @return     none
 */
void log_warn(u8 *tag, u8 *fmt, ...);

/**
 * @brief      带标签的 ERROR 级别日志
 * @param[in]  tag   日志标签
 * @param[in]  fmt   格式化字符串
 * @param[in]  ...   可变参数
 * @return     none
 */
void log_error(u8 *tag, u8 *fmt, ...);

/**
 * @brief      带标签的 DEBUG 级别日志
 * @param[in]  tag   日志标签
 * @param[in]  fmt   格式化字符串
 * @param[in]  ...   可变参数
 * @return     none
 */
void log_debug(u8 *tag, u8 *fmt, ...);

/**
 * @brief      原始日志输出 (无标签、无级别)
 * @param[in]  fmt   格式化字符串
 * @param[in]  ...   可变参数
 * @return     none
 */
void log_printf(u8 *fmt, ...);

#endif
