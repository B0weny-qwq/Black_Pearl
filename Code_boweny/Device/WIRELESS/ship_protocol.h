/**
 * @file    ship_protocol.h
 * @brief   船端无线业务协议解析与调度接口。
 * @author  boweny
 * @date    2026-05-05
 * @version v1.0
 *
 * @details
 * 定义船端无线帧格式的帧头、帧尾、命令字和对外调度函数。
 * 协议层负责解析配对、油门、GPS 上报、返航和目标点等业务命令。
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
 * @return  none
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
 * @return  none
 *
 * @details
 * 用于执行配对、心跳或业务帧发送等需要周期调度的协议动作。
 */
void ShipProtocol_RunScheduler(void);

/**
 * @brief   查询船端协议是否已经完成配对。
 * @return  1=已配对，0=未配对。
 */
u8 ShipProtocol_IsPaired(void);

#endif
