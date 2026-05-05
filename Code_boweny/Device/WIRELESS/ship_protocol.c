#include "ship_protocol.h"

#include "wireless.h"
#include "..\..\Device\GPS\GPS.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\..\User\Task.h"

#if defined(SHIP_PAIR_SEED_USE_CHIPID) && (SHIP_PAIR_SEED_USE_CHIPID != 0)
#include "..\..\..\User\STC32G.h"
#endif

#define SHIP_TAG "SHIP"
#define SHIP_PAIR_FIX_REV "pairbiz-r2"

#ifndef SHIP_PAIR_SYNC_WORD
#define SHIP_PAIR_SYNC_WORD            0x03800380UL
#endif

#ifndef SHIP_PAIR_CHANNEL_DEFAULT
#ifdef PAIR_CHANNEL
#define SHIP_PAIR_CHANNEL_DEFAULT      ((u8)(PAIR_CHANNEL))
#else
#define SHIP_PAIR_CHANNEL_DEFAULT      0x7FU
#endif
#endif

#ifndef SHIP_PAIR_SEND_TIMES
#define SHIP_PAIR_SEND_TIMES           10U
#endif

#ifndef SHIP_WAIT_TICKS_DEFAULT
#define SHIP_WAIT_TICKS_DEFAULT        30U
#endif

#ifndef SHIP_PAIR_WAIT_RSP_TICKS
#define SHIP_PAIR_WAIT_RSP_TICKS       500U
#endif

#ifndef SHIP_WORK_TX_DIV_THRESHOLD
#define SHIP_WORK_TX_DIV_THRESHOLD     80U
#endif

#ifndef SHIP_PAIR_SEED0
#define SHIP_PAIR_SEED0                0x65U
#endif
#ifndef SHIP_PAIR_SEED1
#define SHIP_PAIR_SEED1                0x65U
#endif
#ifndef SHIP_PAIR_SEED2
#define SHIP_PAIR_SEED2                0xA0U
#endif
#ifndef SHIP_PAIR_SEED3
#define SHIP_PAIR_SEED3                0x65U
#endif

#ifndef WIRELESS_MINIMAL_TEST_ONLY
#define WIRELESS_MINIMAL_TEST_ONLY     0
#endif

typedef enum
{
    SHIP_STATE_BOOT_WAIT = 0,
    SHIP_STATE_PAIR_SEND,
    SHIP_STATE_PAIR_WAIT_RSP,
    SHIP_STATE_WORK_RX
} ShipState_t;

typedef struct
{
    u8 lr;
    u8 ud;
    u8 key;
    u8 valid;
    u8 paired;
    u8 work_rx_configured;
    u8 work_state_logged;
    ShipState_t state;
    u8 rf_channel[3];
    u8 rf_send_key[2];
    u16 pair_wait_rsp_time;
    u16 accel_timeout_ticks;
    u16 accel_close_ticks;
    u16 wait_ticks;
    u16 pair_left;
    u16 pair_retry_count;
    u8 work_div;
} ShipRuntime_t;

static ShipRuntime_t g_ship_rt;
static u8 g_ship_sync_is_tx = 0U;

static s8 ShipProtocol_ApplyPairSync(void);
static s8 ShipProtocol_ApplyWorkRx(void);

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

/*
 * Pair seed is the 4-byte payload carried by cmd=0x10:
 *   AA | 06 | 10 | seed0 | seed1 | seed2 | seed3 | xor | BB
 *
 * In the current ship-side implementation this seed is not only a "record"
 * value. It is the live pairing input used to derive the follow-up work
 * channel and sync/key bytes. If the peer expects a different seed, pairing
 * can fail even when RF TX itself is healthy.
 */
static void ShipProtocol_GetPairSeed(u8 *seed)
{
    if (seed == 0) {
        return;
    }

#if defined(SHIP_PAIR_SEED_USE_CHIPID) && (SHIP_PAIR_SEED_USE_CHIPID != 0)
    seed[0] = CHIPID20;
    seed[1] = CHIPID21;
    seed[2] = CHIPID22;
    seed[3] = CHIPID23;
#else
    seed[0] = SHIP_PAIR_SEED0;
    seed[1] = SHIP_PAIR_SEED1;
    seed[2] = SHIP_PAIR_SEED2;
    seed[3] = SHIP_PAIR_SEED3;
#endif
}

static u16 ShipProtocol_ReadU16BE(const u8 *buf)
{
    return (u16)(((u16)buf[0] << 8) | buf[1]);
}

static u16 ShipProtocol_ReadU16LE(const u8 *buf)
{
    return (u16)(((u16)buf[1] << 8) | buf[0]);
}

static u16 ShipProtocol_ToCoord1(u32 abs_deg1e7)
{
    return (u16)(abs_deg1e7 / 10000000UL);
}

static u16 ShipProtocol_ToCoord2(u32 abs_deg1e7)
{
    return (u16)(((abs_deg1e7 % 10000000UL) * 10000UL) / 10000000UL);
}

static void ShipProtocol_WriteU16LE(u8 *dst, u16 value)
{
    dst[0] = (u8)(value & 0xFFU);
    dst[1] = (u8)(value >> 8);
}

static void ShipProtocol_LogCoordBE(const u8 *buf, u8 len)
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

static s8 ShipProtocol_SendFrame(u8 channel, u8 cmd, const u8 *payload, u8 payload_len)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 idx;
    u8 body_len;
    u8 i;
    s8 rc;

    if (payload_len > (SHIP_PROTO_MAX_FRAME_LEN - 5U)) {
        return WIRELESS_ERR_PARAM;
    }

    body_len = (u8)(2U + payload_len);
    idx = 0U;
    frame[idx++] = SHIP_PROTO_HEAD;
    frame[idx++] = body_len;
    frame[idx++] = cmd;

    for (i = 0U; i < payload_len; i++) {
        frame[idx++] = payload[i];
    }

    frame[idx++] = ShipProtocol_Xor(&frame[1], body_len);
    frame[idx++] = SHIP_PROTO_TAIL;

    rc = Wireless_SetChannel(channel);
    if (rc != SUCCESS) {
        return rc;
    }
    return Wireless_Send(frame, idx);
}

static void ShipProtocol_ApplyDefaultRf(void)
{
    u8 seed[4];
    u8 channel;

    /* The work RX/TX channels and sync bytes are derived from the same
     * 4-byte pair seed. Keep this in sync with the peer's legacy formula.
     */
    ShipProtocol_GetPairSeed(seed);

    g_ship_rt.rf_send_key[0] =
        (u8)(((u8)((seed[0] << 4) >> 4)) + ((u8)(seed[3] >> 2) + (u8)(seed[3] % 0x03U)));
    g_ship_rt.rf_send_key[1] =
        (u8)(((u8)((seed[1] << 4) >> 4)) + ((u8)(seed[2] >> 3) + (u8)(seed[0] % 0x06U)));

    channel = (u8)(((u8)(((seed[3] + 0x06U) % 0x40U) +
                         ((seed[2] >> 3) * 0x08U) +
                         (((seed[1] | seed[0]) % 0x08U) / 2U))) % 0x40U);
    g_ship_rt.rf_channel[0] = channel;
    g_ship_rt.rf_channel[1] = channel;
    g_ship_rt.rf_channel[2] = (u8)(channel + 0x40U);
}

static s8 ShipProtocol_ApplyWorkRx(void)
{
    u16 rx36;
    u16 rx39;
    s8 rc;

    rx36 = (u16)(((u16)g_ship_rt.rf_send_key[0] << 8) | g_ship_rt.rf_send_key[0]);
    rx39 = (u16)(((u16)g_ship_rt.rf_send_key[1] << 8) | g_ship_rt.rf_send_key[1]);

    rc = Wireless_SetSyncRegs(rx36, rx39);
    if (rc != SUCCESS) {
        return rc;
    }

    g_ship_sync_is_tx = 0U;
    rc = Wireless_SetChannel(g_ship_rt.rf_channel[0]);
    if (rc == SUCCESS) {
        g_ship_rt.work_rx_configured = 1U;
    }
    return rc;
}

static s8 ShipProtocol_ApplyPairSync(void)
{
    s8 rc;

    rc = Wireless_SetSyncWord(SHIP_PAIR_SYNC_WORD);
    if (rc != SUCCESS) {
        return rc;
    }

    g_ship_sync_is_tx = 0U;
    g_ship_rt.work_rx_configured = 0U;
    return SUCCESS;
}

static s8 ShipProtocol_EnsureSyncTx(void)
{
    u16 tx36;
    u16 tx39;

    if (g_ship_sync_is_tx != 0U) {
        return SUCCESS;
    }

    tx36 = (u16)(((u16)g_ship_rt.rf_send_key[1] << 8) | g_ship_rt.rf_send_key[1]);
    tx39 = (u16)(((u16)g_ship_rt.rf_send_key[0] << 8) | g_ship_rt.rf_send_key[0]);
    if (Wireless_SetSyncRegs(tx36, tx39) != SUCCESS) {
        return WIRELESS_ERR_IO;
    }

    g_ship_sync_is_tx = 1U;
    g_ship_rt.work_rx_configured = 0U;
    return SUCCESS;
}

static s8 ShipProtocol_EnsureSyncRx(void)
{
    u16 rx36;
    u16 rx39;

    if (g_ship_sync_is_tx == 0U) {
        return SUCCESS;
    }

    rx36 = (u16)(((u16)g_ship_rt.rf_send_key[0] << 8) | g_ship_rt.rf_send_key[0]);
    rx39 = (u16)(((u16)g_ship_rt.rf_send_key[1] << 8) | g_ship_rt.rf_send_key[1]);
    if (Wireless_SetSyncRegs(rx36, rx39) != SUCCESS) {
        return WIRELESS_ERR_IO;
    }

    g_ship_sync_is_tx = 0U;
    g_ship_rt.work_rx_configured = 0U;
    return SUCCESS;
}

static s8 ShipProtocol_TryPairSend(u16 left_after_send)
{
    u8 pair_data[4];
    s8 rc;

    /* cmd=0x10 payload is always the current 4-byte pair seed. */
    ShipProtocol_GetPairSeed(pair_data);

    rc = ShipProtocol_ApplyPairSync();
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "apply pair sync fail rc=%d", rc);
        return rc;
    }

    rc = ShipProtocol_SendFrame(SHIP_PAIR_CHANNEL_DEFAULT, SHIP_CMD_PAIR, pair_data, 4U);
    if (rc == SUCCESS) {
        if (left_after_send == (SHIP_PAIR_SEND_TIMES - 1U)) {
            LOGI(SHIP_TAG,
                 "pair req start retry=%u pair_ch=0x%02X seed=%02X%02X%02X%02X work_rx=%u key=%u/%u",
                 (u16)g_ship_rt.pair_retry_count,
                 (u16)SHIP_PAIR_CHANNEL_DEFAULT,
                 (u16)pair_data[0], (u16)pair_data[1],
                 (u16)pair_data[2], (u16)pair_data[3],
                 (u16)g_ship_rt.rf_channel[0],
                 (u16)g_ship_rt.rf_send_key[0],
                 (u16)g_ship_rt.rf_send_key[1]);
        } else if (left_after_send == 0U) {
            LOGI(SHIP_TAG, "pair req burst done, wait rsp");
        }
    } else {
        LOGE(SHIP_TAG, "pair req tx fail rc=%d", rc);
    }

    return rc;
}

static void ShipProtocol_SendGpsOnce(void)
{
    u8 payload[15];
    const GPS_State_t *gps;
    u8 idx;
    u16 angle;
    u32 abs_lon;
    u32 abs_lat;
    u16 coord1;
    u16 coord2;
    s8 rc;

    gps = GPS_GetState();
    idx = 0U;

    payload[idx++] = gps->satellites_used;

    angle = gps->course_deg_x100;
    payload[idx++] = (u8)(angle & 0xFFU);
    payload[idx++] = (u8)(angle >> 8);

    if (gps->lon_deg1e7 < 0) {
        payload[idx++] = 'W';
        abs_lon = (u32)(-gps->lon_deg1e7);
    } else {
        payload[idx++] = 'E';
        abs_lon = (u32)gps->lon_deg1e7;
    }
    coord1 = ShipProtocol_ToCoord1(abs_lon);
    coord2 = ShipProtocol_ToCoord2(abs_lon);
    ShipProtocol_WriteU16LE(&payload[idx], coord1);
    idx += 2U;
    ShipProtocol_WriteU16LE(&payload[idx], coord2);
    idx += 2U;

    if (gps->lat_deg1e7 < 0) {
        payload[idx++] = 'S';
        abs_lat = (u32)(-gps->lat_deg1e7);
    } else {
        payload[idx++] = 'N';
        abs_lat = (u32)gps->lat_deg1e7;
    }
    coord1 = ShipProtocol_ToCoord1(abs_lat);
    coord2 = ShipProtocol_ToCoord2(abs_lat);
    ShipProtocol_WriteU16LE(&payload[idx], coord1);
    idx += 2U;
    ShipProtocol_WriteU16LE(&payload[idx], coord2);
    idx += 2U;

    payload[idx++] = 0U; /* power placeholder */
    payload[idx++] = 0U; /* autodrive placeholder */

    rc = ShipProtocol_EnsureSyncTx();
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "set tx sync fail rc=%d", rc);
        return;
    }

    rc = ShipProtocol_SendFrame(g_ship_rt.rf_channel[2], SHIP_CMD_GPS_REPORT, payload, idx);
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "gps tx fail rc=%d ch=0x%02X", rc, (u16)g_ship_rt.rf_channel[2]);
    }
    rc = ShipProtocol_EnsureSyncRx();
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "restore rx sync fail rc=%d", rc);
    }
}

static void ShipProtocol_HandlePairRsp(const u8 *payload, u8 payload_len)
{
    if (g_ship_rt.state != SHIP_STATE_PAIR_WAIT_RSP) {
        LOGW(SHIP_TAG, "pair rsp ignored(outside window)");
        return;
    }

    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.paired = 1U;
    g_ship_rt.state = SHIP_STATE_WORK_RX;

    if ((payload != 0) && (payload_len == 4U)) {
        LOGI(SHIP_TAG,
             "pair success paired=1 work_rx=%u work_tx=%u key=%u/%u rsp=%02X%02X%02X%02X",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[2],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)payload[0], (u16)payload[1],
             (u16)payload[2], (u16)payload[3]);
    } else {
        LOGI(SHIP_TAG,
             "pair success paired=1 work_rx=%u work_tx=%u key=%u/%u rsp_len=%u",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[2],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)payload_len);
    }
}

static void ShipProtocol_HandleThrottle(const u8 *payload, u8 payload_len)
{
    if (payload_len < 3U) {
        LOGW(SHIP_TAG, "throttle short len=%u", (u16)payload_len);
        return;
    }

    g_ship_rt.lr = payload[0];
    g_ship_rt.ud = payload[1];
    g_ship_rt.key = payload[2];
    g_ship_rt.valid = 1U;
    g_ship_rt.accel_timeout_ticks = 0U;
    g_ship_rt.accel_close_ticks = 300U;

    LOGI(SHIP_TAG, "rc lr=%u ud=%u key=0x%02X paired=%u",
         (u16)g_ship_rt.lr,
         (u16)g_ship_rt.ud,
         (u16)g_ship_rt.key,
         (u16)g_ship_rt.paired);
}

static void ShipProtocol_HandleGpsReport(const u8 *payload, u8 payload_len)
{
    if (payload_len < 11U) {
        LOGW(SHIP_TAG, "gps short len=%u", (u16)payload_len);
        return;
    }

    LOGI(SHIP_TAG, "cmd=0x12 gps=%u yaw=%u",
         (u16)payload[0], ShipProtocol_ReadU16LE(&payload[1]));
    ShipProtocol_LogCoordBE(&payload[3], (u8)(payload_len - 3U));
}

static void ShipProtocol_Dispatch(u8 cmd, const u8 *payload, u8 payload_len)
{
    switch (cmd) {
    case SHIP_CMD_PAIR_RSP:
        ShipProtocol_HandlePairRsp(payload, payload_len);
        break;
    case SHIP_CMD_PAIR:
        break;
    case SHIP_CMD_THROTTLE:
        ShipProtocol_HandleThrottle(payload, payload_len);
        break;
    case SHIP_CMD_GPS_REPORT:
        ShipProtocol_HandleGpsReport(payload, payload_len);
        break;
    case SHIP_CMD_RETURN_HOME:
        LOGI(SHIP_TAG, "cmd=0x13 return-home len=%u", (u16)payload_len);
        ShipProtocol_LogCoordBE(payload, payload_len);
        break;
    case SHIP_CMD_GOTO_POINT:
        LOGI(SHIP_TAG, "cmd=0x14 goto-point len=%u", (u16)payload_len);
        ShipProtocol_LogCoordBE(payload, payload_len);
        break;
    case SHIP_CMD_RETURN_SWITCH:
        if (payload_len < 1U) {
            LOGW(SHIP_TAG, "return-switch short len=%u", (u16)payload_len);
            break;
        }
        LOGI(SHIP_TAG, "cmd=0x15 switch=0x%02X len=%u",
             (u16)payload[0], (u16)payload_len);
        if (payload_len > 1U) {
            ShipProtocol_LogCoordBE(&payload[1], (u8)(payload_len - 1U));
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

static void ShipProtocol_PollRxFrames(void)
{
    u8 frame[SHIP_PROTO_MAX_FRAME_LEN];
    u8 frame_len;
    s8 rc;

    do {
        frame_len = 0U;
        rc = Wireless_Receive(frame, SHIP_PROTO_MAX_FRAME_LEN, &frame_len);
        if (rc == SUCCESS) {
            rc = ShipProtocol_ParseFrame(frame, frame_len);
            if (rc != SUCCESS) {
                LOGW(SHIP_TAG, "parse frame fail rc=%d len=%u", rc, (u16)frame_len);
            }
        }
    } while (rc == SUCCESS);
}

static void ShipProtocol_InitRuntime(void)
{
    u8 seed[4];

    ShipProtocol_ApplyDefaultRf();
    ShipProtocol_GetPairSeed(seed);

    g_ship_rt.lr = 0U;
    g_ship_rt.ud = 0U;
    g_ship_rt.key = 0U;
    g_ship_rt.valid = 0U;
    g_ship_rt.paired = 0U;
    g_ship_rt.work_rx_configured = 0U;
    g_ship_rt.work_state_logged = 0U;
    g_ship_rt.state = SHIP_STATE_BOOT_WAIT;
    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.accel_timeout_ticks = 0U;
    g_ship_rt.accel_close_ticks = 0U;
    g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
    g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
    g_ship_rt.pair_retry_count = 0U;
    g_ship_rt.work_div = 0U;
    g_ship_sync_is_tx = 0U;

    LOGI(SHIP_TAG,
         "scheduler init rev=%s wait=%u pair_send=%u pair_ch=0x%02X seed=%02X%02X%02X%02X",
         SHIP_PAIR_FIX_REV,
         (u16)g_ship_rt.wait_ticks,
         (u16)g_ship_rt.pair_left,
         (u16)SHIP_PAIR_CHANNEL_DEFAULT,
         (u16)seed[0], (u16)seed[1], (u16)seed[2], (u16)seed[3]);
}

static void ShipProtocol_StepPairWaitRsp(void)
{
    if (g_ship_rt.pair_wait_rsp_time > 0U) {
        g_ship_rt.pair_wait_rsp_time--;
        if ((g_ship_rt.pair_wait_rsp_time == 0U) && (g_ship_rt.paired == 0U)) {
            g_ship_rt.pair_retry_count++;
            LOGW(SHIP_TAG, "pair rsp window timeout, retry=%u",
                 (u16)g_ship_rt.pair_retry_count);
            g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
            g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
            g_ship_rt.state = SHIP_STATE_PAIR_SEND;
        }
    } else if (g_ship_rt.paired == 0U) {
        g_ship_rt.pair_retry_count++;
        g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
        g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
        g_ship_rt.state = SHIP_STATE_PAIR_SEND;
    }
}

static void ShipProtocol_StepPairSend(void)
{
    s8 rc;
    u16 left_after_send;

    if (g_ship_rt.wait_ticks > 0U) {
        g_ship_rt.wait_ticks--;
        return;
    }

    if (g_ship_rt.pair_left > 0U) {
        g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
        left_after_send = (u16)(g_ship_rt.pair_left - 1U);
        rc = ShipProtocol_TryPairSend(left_after_send);
        if (rc != SUCCESS) {
            return;
        }
        g_ship_rt.pair_left = left_after_send;

        if (g_ship_rt.pair_left == 0U) {
            rc = ShipProtocol_ApplyWorkRx();
            if (rc != SUCCESS) {
                LOGE(SHIP_TAG, "enter pair rsp listen fail rc=%d", rc);
                g_ship_rt.pair_retry_count++;
                g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
                g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
                return;
            }
            g_ship_rt.pair_wait_rsp_time = SHIP_PAIR_WAIT_RSP_TICKS;
            g_ship_rt.state = SHIP_STATE_PAIR_WAIT_RSP;
        }
    }
}

static void ShipProtocol_StepWorkRx(void)
{
    s8 rc;

    if (g_ship_rt.work_rx_configured == 0U) {
        rc = ShipProtocol_ApplyWorkRx();
        if (rc != SUCCESS) {
            LOGE(SHIP_TAG, "enter work rx fail rc=%d", rc);
            return;
        }
    }

    if (g_ship_rt.work_state_logged == 0U) {
        g_ship_rt.work_state_logged = 1U;
        LOGI(SHIP_TAG, "enter work-state rx_ch=%u tx_ch=%u tx_div>%u",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[2],
             (u16)SHIP_WORK_TX_DIV_THRESHOLD);
    }

#if !WIRELESS_MINIMAL_TEST_ONLY
    g_ship_rt.work_div++;
    if (g_ship_rt.work_div > SHIP_WORK_TX_DIV_THRESHOLD) {
        g_ship_rt.work_div = 0U;
        ShipProtocol_SendGpsOnce();
        g_ship_rt.work_rx_configured = 0U;
    } else if (g_ship_rt.work_rx_configured == 0U) {
        rc = ShipProtocol_ApplyWorkRx();
        if (rc != SUCCESS) {
            LOGE(SHIP_TAG, "restore work rx fail rc=%d", rc);
        }
    }
#endif
}

void ShipProtocol_RunScheduler(void)
{
    static u8 initialized = 0U;
    static u32 last_tick_ms = 0U;
    u32 now_ms;

    if (!initialized) {
        ShipProtocol_InitRuntime();
        initialized = 1U;
        last_tick_ms = Task_GetTickMs();
    }

    now_ms = Task_GetTickMs();
    if ((now_ms - last_tick_ms) < 10U) {
        ShipProtocol_PollRxFrames();
        return;
    }
    last_tick_ms += 10U;

    ShipProtocol_PollRxFrames();

    if (g_ship_rt.accel_timeout_ticks < (30U * 100U)) {
        g_ship_rt.accel_timeout_ticks++;
    }
    if (g_ship_rt.accel_close_ticks > 0U) {
        g_ship_rt.accel_close_ticks--;
    }

    if ((g_ship_rt.state == SHIP_STATE_BOOT_WAIT) && (g_ship_rt.wait_ticks > 0U)) {
        g_ship_rt.wait_ticks--;
        return;
    }
    if (g_ship_rt.state == SHIP_STATE_BOOT_WAIT) {
        g_ship_rt.state = SHIP_STATE_PAIR_SEND;
    }

    switch (g_ship_rt.state) {
    case SHIP_STATE_PAIR_SEND:
        ShipProtocol_StepPairSend();
        break;
    case SHIP_STATE_PAIR_WAIT_RSP:
        ShipProtocol_StepPairWaitRsp();
        break;
    case SHIP_STATE_WORK_RX:
        ShipProtocol_StepWorkRx();
        break;
    default:
        g_ship_rt.state = SHIP_STATE_PAIR_SEND;
        break;
    }
}

u8 ShipProtocol_IsPaired(void)
{
    return g_ship_rt.paired;
}
