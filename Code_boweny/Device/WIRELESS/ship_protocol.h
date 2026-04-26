#ifndef __SHIP_PROTOCOL_H__
#define __SHIP_PROTOCOL_H__

#include "config.h"

#define SHIP_PROTO_HEAD          0xAAU
#define SHIP_PROTO_TAIL          0xBBU
#define SHIP_PROTO_MAX_FRAME_LEN 64U

#define SHIP_CMD_PAIR            0x10U
#define SHIP_CMD_THROTTLE        0x11U
#define SHIP_CMD_GPS_REPORT      0x12U
#define SHIP_CMD_RETURN_HOME     0x13U
#define SHIP_CMD_GOTO_POINT      0x14U
#define SHIP_CMD_RETURN_SWITCH   0x15U

void ShipProtocol_Poll(void);
s8 ShipProtocol_ParseFrame(const u8 *frame, u8 frame_len);

#endif
