/**
 * @file    ship_protocol.h
 * @brief   船端无线业务协议解析与调度接口。
 * @author  boweny
 * @date    2026-05-06
 * @version v1.1
 *
 * @details
 * 定义船端无线帧格式的帧头、帧尾、命令字和对外调度函数。
 * 协议层负责解析配对、遥控器油门/转向、GPS 上报、返航和目标点等
 * 业务命令。当前无线最小业务只启用配对和遥控值打印，不驱动电机。
 *
 * @note
 * 当前主路径应调用 ShipProtocol_RunScheduler()，由调度器统一消费无线接收
 * 队列并维护配对状态。ShipProtocol_Poll() 仅保留为兼容入口，不应与调度器
 * 在同一主循环中同时启用，以免重复消费无线接收队列。
 *
 * @see     Code_boweny/Device/WIRELESS/ship_protocol.c
 */

#ifndef __SHIP_PROTOCOL_H__
#define __SHIP_PROTOCOL_H__

#include "config.h"

#define SHIP_PROTO_HEAD          0xAAU  /**< 船端协议帧头字节。 */
#define SHIP_PROTO_TAIL          0xBBU  /**< 船端协议帧尾字节。 */
#define SHIP_PROTO_MAX_FRAME_LEN 64U    /**< 单帧最大长度，单位 byte。 */

#define SHIP_CMD_PAIR_RSP        0x0FU  /**< 配对响应命令。 */
#define SHIP_CMD_PAIR            0x10U  /**< 配对请求命令。 */
#define SHIP_CMD_THROTTLE        0x11U  /**< 油门/转向控制命令。 */
#define SHIP_CMD_GPS_REPORT      0x12U  /**< GPS 状态上报命令。 */
#define SHIP_CMD_RETURN_HOME     0x13U  /**< 一键返航命令。 */
#define SHIP_CMD_GOTO_POINT      0x14U  /**< 目标点导航命令。 */
#define SHIP_CMD_RETURN_SWITCH   0x15U  /**< 返航开关命令。 */

/**
 * @brief   轮询无线接收数据并尝试解析协议帧。
 * @return  无。
 *
 * @note
 * 兼容入口。当前最小业务模式下由 ShipProtocol_RunScheduler() 负责收包和调度。
 */
void ShipProtocol_Poll(void);

/**
 * @brief      解析一帧完整船端协议数据。
 * @param[in]  frame      指向协议帧缓冲区的指针。
 * @param[in]  frame_len  协议帧长度，单位 byte。
 * @return     SUCCESS=解析并处理成功，WIRELESS_ERR_* 表示参数、校验或业务处理失败。
 */
s8 ShipProtocol_ParseFrame(const u8 *frame, u8 frame_len);

/**
 * @brief   运行协议层周期任务。
 * @return  无。
 *
 * @details
 * 用于执行固定 seed 配对、配对响应窗口、工作信道监听和协议帧解析。
 * 当前最小业务配对成功后，每收到一帧 SHIP_CMD_THROTTLE(0x11) 都会打印
 * lr/ud/key，不调用电机控制。
 */
void ShipProtocol_RunScheduler(void);

/**
 * @brief   查询船端协议是否已经完成配对。
 * @return  1=已配对，0=未配对。
 */
u8 ShipProtocol_IsPaired(void);

#endif
