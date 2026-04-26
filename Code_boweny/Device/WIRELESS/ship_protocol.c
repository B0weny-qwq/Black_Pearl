#include "ship_protocol.h"

#include "wireless.h"
#include "..\..\Function\Log\Log.h"

#define SHIP_TAG "SHIP"

static u8 ShipProtocol_Xor(const u8 *buf, u8 len)
{
    u8 i;
    u8 val;

    val = 0U;
    for (i = 0U; i < len; i++) {
        val ^= buf[i];
    }
    return val;
}

static u16 ShipProtocol_ReadU16BE(const u8 *buf)
{
    return (u16)(((u16)buf[0] << 8) | buf[1]);
}

static void ShipProtocol_LogCoord(const u8 *buf, u8 len)
{
    if (len < 10U) {
        LOGW(SHIP_TAG, "coord short len=%u", (u16)len);
        return;
    }

    LOGI(SHIP_TAG,
         "coord lonEW=0x%02X lon=%u.%u latNS=0x%02X lat=%u.%u",
         (u16)buf[0],
         ShipProtocol_ReadU16BE(&buf[1]),
         ShipProtocol_ReadU16BE(&buf[3]),
         (u16)buf[5],
         ShipProtocol_ReadU16BE(&buf[6]),
         ShipProtocol_ReadU16BE(&buf[8]));
}

static void ShipProtocol_HandlePair(const u8 *payload, u8 payload_len)
{
    u8 key1;
    u8 key2;
    u8 channel;

    if (payload_len < 4U) {
        LOGW(SHIP_TAG, "pair short len=%u", (u16)payload_len);
        return;
    }

    key1 = (u8)((u8)((payload[0] << 4) >> 4) + ((u8)(payload[3] >> 2) + (u8)(payload[3] % 0x03U)));
    key2 = (u8)((u8)((payload[1] << 4) >> 4) + ((u8)(payload[2] >> 3) + (u8)(payload[0] % 0x06U)));
    if (payload_len >= 5U) {
        channel = (u8)(((u8)(((payload[4] + 0x06U) % 0x40U) +
                             ((payload[2] >> 3) * 0x08U) +
                             (((payload[1] | payload[0]) % 0x08U) / 2U))) % 0x40U);
        LOGI(SHIP_TAG,
             "cmd=0x10 pair chip=%02X %02X %02X %02X key=%u/%u ch=%u",
             (u16)payload[0], (u16)payload[1], (u16)payload[2], (u16)payload[3],
             (u16)key1, (u16)key2, (u16)channel);
    } else {
        LOGI(SHIP_TAG,
             "cmd=0x10 pair chip=%02X %02X %02X %02X key=%u/%u",
             (u16)payload[0], (u16)payload[1], (u16)payload[2], (u16)payload[3],
             (u16)key1, (u16)key2);
    }
}

static void ShipProtocol_HandleThrottle(const u8 *payload, u8 payload_len)
{
    if (payload_len < 3U) {
        LOGW(SHIP_TAG, "throttle short len=%u", (u16)payload_len);
        return;
    }

    LOGI(SHIP_TAG, "cmd=0x11 lr=%u ud=%u key=0x%02X",
         (u16)payload[0], (u16)payload[1], (u16)payload[2]);
}

static void ShipProtocol_HandleGpsReport(const u8 *payload, u8 payload_len)
{
    if (payload_len < 11U) {
        LOGW(SHIP_TAG, "gps short len=%u", (u16)payload_len);
        return;
    }

    LOGI(SHIP_TAG, "cmd=0x12 gps=%u yaw=%u",
         (u16)payload[0], ShipProtocol_ReadU16BE(&payload[1]));
    ShipProtocol_LogCoord(&payload[3], (u8)(payload_len - 3U));
}

static void ShipProtocol_Dispatch(u8 cmd, const u8 *payload, u8 payload_len)
{
    switch (cmd) {
    case SHIP_CMD_PAIR:
        ShipProtocol_HandlePair(payload, payload_len);
        break;
    case SHIP_CMD_THROTTLE:
        ShipProtocol_HandleThrottle(payload, payload_len);
        break;
    case SHIP_CMD_GPS_REPORT:
        ShipProtocol_HandleGpsReport(payload, payload_len);
        break;
    case SHIP_CMD_RETURN_HOME:
        LOGI(SHIP_TAG, "cmd=0x13 return-home len=%u", (u16)payload_len);
        ShipProtocol_LogCoord(payload, payload_len);
        break;
    case SHIP_CMD_GOTO_POINT:
        LOGI(SHIP_TAG, "cmd=0x14 goto-point len=%u", (u16)payload_len);
        ShipProtocol_LogCoord(payload, payload_len);
        break;
    case SHIP_CMD_RETURN_SWITCH:
        if (payload_len < 1U) {
            LOGW(SHIP_TAG, "return-switch short len=%u", (u16)payload_len);
            break;
        }
        LOGI(SHIP_TAG, "cmd=0x15 switch=0x%02X len=%u",
             (u16)payload[0], (u16)payload_len);
        if (payload_len > 1U) {
            ShipProtocol_LogCoord(&payload[1], (u8)(payload_len - 1U));
        }
        break;
    default:
        LOGW(SHIP_TAG, "unknown cmd=0x%02X len=%u", (u16)cmd, (u16)payload_len);
        break;
    }
}

s8 ShipProtocol_ParseFrame(const u8 *frame, u8 frame_len)
{
    u8 body_len;
    u8 cmd;
    u8 data_len;
    u8 xor_calc;
    u8 xor_recv;

    if ((frame == 0) || (frame_len < 5U)) {
        return WIRELESS_ERR_PARAM;
    }
    if ((frame[0] != SHIP_PROTO_HEAD) || (frame[frame_len - 1U] != SHIP_PROTO_TAIL)) {
        LOGW(SHIP_TAG, "bad frame edge h=0x%02X t=0x%02X len=%u",
             (u16)frame[0], (u16)frame[frame_len - 1U], (u16)frame_len);
        return WIRELESS_ERR_VERIFY;
    }

    body_len = frame[1];
    if ((body_len < 2U) || ((u8)(body_len + 3U) != frame_len)) {
        LOGW(SHIP_TAG, "bad len field=%u frame=%u", (u16)body_len, (u16)frame_len);
        return WIRELESS_ERR_VERIFY;
    }

    xor_recv = frame[frame_len - 2U];
    xor_calc = ShipProtocol_Xor(&frame[1], body_len);
    if (xor_recv != xor_calc) {
        LOGW(SHIP_TAG, "bad xor cmd=0x%02X calc=0x%02X recv=0x%02X",
             (u16)frame[2], (u16)xor_calc, (u16)xor_recv);
        return WIRELESS_ERR_VERIFY;
    }

    cmd = frame[2];
    data_len = (u8)(body_len - 2U);
    ShipProtocol_Dispatch(cmd, &frame[3], data_len);
    return SUCCESS;
}

void ShipProtocol_Poll(void)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 frame_len;
    s8 rc;

    do {
        frame_len = 0U;
        rc = Wireless_Receive(frame, SHIP_PROTO_MAX_FRAME_LEN, &frame_len);
        if (rc == SUCCESS) {
            (void)ShipProtocol_ParseFrame(frame, frame_len);
        }
    } while (rc == SUCCESS);
}
