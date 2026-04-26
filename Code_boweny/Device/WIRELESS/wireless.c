#include "wireless.h"

#include "lt8920.h"
#include "wireless_port.h"
#include "..\..\Function\Log\Log.h"

static Wireless_State_t g_wireless_state;
static u8 g_wireless_rx_len[WIRELESS_RX_QUEUE_DEPTH];
static u8 g_wireless_rx_data[WIRELESS_RX_QUEUE_DEPTH][LT8920_MAX_PAYLOAD_LEN];
static u8 g_wireless_rx_head = 0U;
static u8 g_wireless_rx_tail = 0U;
static u8 g_wireless_rx_count = 0U;

static void Wireless_ResetState(void)
{
    u16 i;
    u8 *raw;

    raw = (u8 *)&g_wireless_state;
    for (i = 0U; i < (u16)sizeof(Wireless_State_t); i++) {
        raw[i] = 0U;
    }

    g_wireless_state.antenna = WIRELESS_ANT1;
    g_wireless_state.mode = WIRELESS_MODE_IDLE;
}

static void Wireless_ResetQueue(void)
{
    u8 i;
    u8 j;

    g_wireless_rx_head = 0U;
    g_wireless_rx_tail = 0U;
    g_wireless_rx_count = 0U;

    for (i = 0U; i < WIRELESS_RX_QUEUE_DEPTH; i++) {
        g_wireless_rx_len[i] = 0U;
        for (j = 0U; j < LT8920_MAX_PAYLOAD_LEN; j++) {
            g_wireless_rx_data[i][j] = 0U;
        }
    }
}

static s8 Wireless_QueuePush(const u8 *buf, u8 len)
{
    u8 i;

    if ((buf == 0) || (len == 0U) || (len > LT8920_MAX_PAYLOAD_LEN)) {
        return WIRELESS_ERR_PARAM;
    }
    if (g_wireless_rx_count >= WIRELESS_RX_QUEUE_DEPTH) {
        g_wireless_state.queue_overflow_count++;
        return WIRELESS_ERR_OVERFLOW;
    }

    g_wireless_rx_len[g_wireless_rx_head] = len;
    for (i = 0U; i < len; i++) {
        g_wireless_rx_data[g_wireless_rx_head][i] = buf[i];
    }
    g_wireless_rx_head++;
    if (g_wireless_rx_head >= WIRELESS_RX_QUEUE_DEPTH) {
        g_wireless_rx_head = 0U;
    }
    g_wireless_rx_count++;
    return SUCCESS;
}

static s8 Wireless_QueuePop(u8 *buf, u8 buf_len, u8 *out_len)
{
    u8 i;
    u8 len;

    if ((buf == 0) || (out_len == 0)) {
        return WIRELESS_ERR_PARAM;
    }
    if (g_wireless_rx_count == 0U) {
        return WIRELESS_ERR_EMPTY;
    }

    len = g_wireless_rx_len[g_wireless_rx_tail];
    if (len > buf_len) {
        return WIRELESS_ERR_OVERFLOW;
    }

    for (i = 0U; i < len; i++) {
        buf[i] = g_wireless_rx_data[g_wireless_rx_tail][i];
    }
    *out_len = len;

    g_wireless_rx_tail++;
    if (g_wireless_rx_tail >= WIRELESS_RX_QUEUE_DEPTH) {
        g_wireless_rx_tail = 0U;
    }
    g_wireless_rx_count--;
    return SUCCESS;
}

static s8 Wireless_SetIdleMode(void)
{
    s8 rc;

    WirelessPort_SetTxEn(0U);
    WirelessPort_SetRxEn(0U);
    rc = LT8920_EnterIdle();
    if (rc == SUCCESS) {
        g_wireless_state.mode = WIRELESS_MODE_IDLE;
    }
    return rc;
}

static s8 Wireless_SetRxMode(void)
{
    s8 rc;

    WirelessPort_SetTxEn(0U);
    WirelessPort_SetRxEn(1U);
    WirelessPort_DelayUs(5U);

    rc = LT8920_ClearRxFifo();
    if (rc != SUCCESS) {
        return rc;
    }
    rc = LT8920_EnterRx();
    if (rc == SUCCESS) {
        g_wireless_state.mode = WIRELESS_MODE_RX;
    }
    return rc;
}

static s8 Wireless_SetTxMode(void)
{
    s8 rc;

    WirelessPort_SetRxEn(0U);
    WirelessPort_SetTxEn(1U);
    WirelessPort_DelayUs(5U);

    rc = LT8920_EnterTx();
    if (rc == SUCCESS) {
        g_wireless_state.mode = WIRELESS_MODE_TX;
    }
    return rc;
}

static s8 Wireless_ProbeAntenna(u8 antenna, u16 *avg_rssi, u16 *pkt_ok, u16 *crc_err)
{
    u8 sample_idx;
    u8 rssi;
    u8 packet_len;
    u8 packet_buf[LT8920_MAX_PAYLOAD_LEN];
    u16 status;
    u32 sum;
    u16 ok_cnt;
    u16 crc_cnt;
    s8 rc;

    if ((avg_rssi == 0) || (pkt_ok == 0) || (crc_err == 0)) {
        return WIRELESS_ERR_PARAM;
    }

    rc = Wireless_SetAntenna(antenna);
    if (rc != SUCCESS) {
        return rc;
    }

    rc = Wireless_SetRxMode();
    if (rc != SUCCESS) {
        return rc;
    }

    WirelessPort_DelayMs(2U);

    sum = 0UL;
    ok_cnt = 0U;
    crc_cnt = 0U;

    for (sample_idx = 0U; sample_idx < WIRELESS_SCAN_SAMPLE_COUNT; sample_idx++) {
        rc = LT8920_ReadRawRssi(&rssi);
        if (rc == SUCCESS) {
            sum += rssi;
        }

        rc = LT8920_ReadStatus(&status);
        if ((rc == SUCCESS) && ((status & LT8920_STATUS_PKT_FLAG) != 0U)) {
            if ((status & LT8920_STATUS_CRC_ERROR) != 0U) {
                crc_cnt++;
            } else {
                if (LT8920_ReadPacket(packet_buf, LT8920_MAX_PAYLOAD_LEN, &packet_len) == SUCCESS) {
                    ok_cnt++;
                }
            }
            (void)Wireless_SetRxMode();
        }

        WirelessPort_DelayMs(WIRELESS_SCAN_SAMPLE_MS);
    }

    *avg_rssi = (u16)(sum / WIRELESS_SCAN_SAMPLE_COUNT);
    *pkt_ok = ok_cnt;
    *crc_err = crc_cnt;
    return SUCCESS;
}

s8 Wireless_Init(void)
{
    s8 rc;
    u16 reg11;
    u16 reg41;
    u16 reg52;

    LOGI(WIRELESS_TAG, "init start");
    Wireless_ResetState();
    Wireless_ResetQueue();

    rc = WirelessPort_Init();
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        LOGE(WIRELESS_TAG, "port init fail rc=%d", rc);
        return rc;
    }

    WirelessPort_SetRst(0U);
    WirelessPort_DelayMs(1U);
    WirelessPort_SetRst(1U);
    WirelessPort_DelayMs(5U);

    rc = LT8920_Init(LT8920_DEFAULT_CHANNEL, LT8920_DEFAULT_SYNC_WORD);
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        LOGE(WIRELESS_TAG, "lt8920 init fail rc=%d", rc);
        if (LT8920_ReadReg(11U, &reg11) == SUCCESS) {
            LOGE(WIRELESS_TAG, "verify reg11=0x%04X expect=0x0008", reg11);
        }
        if (LT8920_ReadReg(41U, &reg41) == SUCCESS) {
            LOGE(WIRELESS_TAG, "verify reg41=0x%04X expect=0xB000", reg41);
        }
        if (LT8920_ReadReg(52U, &reg52) == SUCCESS) {
            LOGE(WIRELESS_TAG, "debug reg52=0x%04X", reg52);
        }
        return rc;
    }

    g_wireless_state.initialized = 1U;
    rc = Wireless_RescanAntenna();
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        g_wireless_state.initialized = 0U;
        g_wireless_state.ready = 0U;
        (void)Wireless_SetIdleMode();
        LOGE(WIRELESS_TAG, "antenna scan fail rc=%d", rc);
        return rc;
    }

    rc = Wireless_SetRxMode();
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        g_wireless_state.initialized = 0U;
        g_wireless_state.ready = 0U;
        (void)Wireless_SetIdleMode();
        LOGE(WIRELESS_TAG, "enter rx fail rc=%d", rc);
        return rc;
    }

    g_wireless_state.ready = 1U;
    g_wireless_state.last_error = SUCCESS;
    LOGI(WIRELESS_TAG, "init ok ch=%u ant=%u rssi=%u/%u",
         (u16)LT8920_DEFAULT_CHANNEL,
         (u16)g_wireless_state.antenna,
         g_wireless_state.antenna_rssi_ant1,
         g_wireless_state.antenna_rssi_ant2);
    return SUCCESS;
}

s8 Wireless_Deinit(void)
{
    if (!g_wireless_state.initialized) {
        return SUCCESS;
    }

    (void)Wireless_SetIdleMode();
    (void)WirelessPort_Deinit();
    Wireless_ResetQueue();
    Wireless_ResetState();
    LOGI(WIRELESS_TAG, "deinit ok");
    return SUCCESS;
}

s8 Wireless_Poll(void)
{
    u16 status;
    u8 packet_len;
    u8 packet_buf[LT8920_MAX_PAYLOAD_LEN];
    s8 rc;

    if (!g_wireless_state.initialized) {
        return WIRELESS_ERR_STATE;
    }
    if (!g_wireless_state.ready) {
        return SUCCESS;
    }

    rc = LT8920_ReadStatus(&status);
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        return rc;
    }
    if ((status & LT8920_STATUS_PKT_FLAG) == 0U) {
        return SUCCESS;
    }

    if (g_wireless_state.mode == WIRELESS_MODE_TX) {
        g_wireless_state.tx_ok_count++;
        rc = Wireless_SetRxMode();
        if (rc != SUCCESS) {
            g_wireless_state.last_error = rc;
            LOGE(WIRELESS_TAG, "tx->rx fail rc=%d", rc);
            return rc;
        }
        return SUCCESS;
    }

    if (g_wireless_state.mode != WIRELESS_MODE_RX) {
        return SUCCESS;
    }

    if ((status & LT8920_STATUS_CRC_ERROR) != 0U) {
        g_wireless_state.crc_error_count++;
        (void)Wireless_SetRxMode();
        return SUCCESS;
    }

    rc = LT8920_ReadPacket(packet_buf, LT8920_MAX_PAYLOAD_LEN, &packet_len);
    if (rc == SUCCESS) {
        g_wireless_state.rx_ok_count++;
        rc = Wireless_QueuePush(packet_buf, packet_len);
        if (rc != SUCCESS) {
            g_wireless_state.rx_drop_count++;
        }
    } else {
        g_wireless_state.rx_drop_count++;
        g_wireless_state.last_error = rc;
    }

    (void)Wireless_SetRxMode();
    return SUCCESS;
}

s8 Wireless_Send(const u8 *buf, u8 len)
{
    u16 status;
    u16 timeout_cnt;
    s8 rc;

    if ((buf == 0) || (len == 0U) || (len > LT8920_MAX_PAYLOAD_LEN)) {
        return WIRELESS_ERR_PARAM;
    }
    if ((!g_wireless_state.initialized) || (!g_wireless_state.ready)) {
        return WIRELESS_ERR_STATE;
    }

    rc = LT8920_WritePacket(buf, len);
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        LOGE(WIRELESS_TAG, "load tx fifo fail rc=%d", rc);
        return rc;
    }

    rc = Wireless_SetTxMode();
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        LOGE(WIRELESS_TAG, "enter tx fail rc=%d", rc);
        return rc;
    }

    for (timeout_cnt = 0U; timeout_cnt < WIRELESS_TX_TIMEOUT_LOOPS; timeout_cnt++) {
        rc = LT8920_ReadStatus(&status);
        if (rc != SUCCESS) {
            break;
        }
        if ((status & LT8920_STATUS_PKT_FLAG) != 0U) {
            g_wireless_state.tx_ok_count++;
            (void)Wireless_SetRxMode();
            return SUCCESS;
        }
        WirelessPort_DelayUs(10U);
    }

    (void)Wireless_SetRxMode();
    g_wireless_state.last_error = WIRELESS_ERR_TIMEOUT;
    LOGE(WIRELESS_TAG, "tx timeout len=%u", (u16)len);
    return WIRELESS_ERR_TIMEOUT;
}

s8 Wireless_Receive(u8 *buf, u8 buf_len, u8 *out_len)
{
    return Wireless_QueuePop(buf, buf_len, out_len);
}

s8 Wireless_SetAntenna(u8 ant_sel)
{
    if ((ant_sel != WIRELESS_ANT1) && (ant_sel != WIRELESS_ANT2)) {
        return WIRELESS_ERR_PARAM;
    }
    if (!g_wireless_state.initialized) {
        return WIRELESS_ERR_STATE;
    }

    WirelessPort_SetAntSel((ant_sel == WIRELESS_ANT2) ? WIRELESS_PORT_ANT2 : WIRELESS_PORT_ANT1);
    g_wireless_state.antenna = ant_sel;
    return SUCCESS;
}

s8 Wireless_GetState(Wireless_State_t *state)
{
    u16 i;
    u8 *dst;
    const u8 *src;

    if (state == 0) {
        return WIRELESS_ERR_PARAM;
    }

    dst = (u8 *)state;
    src = (const u8 *)&g_wireless_state;
    for (i = 0U; i < (u16)sizeof(Wireless_State_t); i++) {
        dst[i] = src[i];
    }
    return SUCCESS;
}

s8 Wireless_RescanAntenna(void)
{
    u16 ant1_rssi;
    u16 ant2_rssi;
    u16 ant1_ok;
    u16 ant2_ok;
    u16 ant1_crc;
    u16 ant2_crc;
    s8 rc;
    u8 final_ant;

    if (!g_wireless_state.initialized) {
        return WIRELESS_ERR_STATE;
    }

    g_wireless_state.ready = 0U;
    rc = Wireless_SetIdleMode();
    if (rc != SUCCESS) {
        return rc;
    }

    rc = Wireless_ProbeAntenna(WIRELESS_ANT1, &ant1_rssi, &ant1_ok, &ant1_crc);
    if (rc != SUCCESS) {
        return rc;
    }
    rc = Wireless_ProbeAntenna(WIRELESS_ANT2, &ant2_rssi, &ant2_ok, &ant2_crc);
    if (rc != SUCCESS) {
        return rc;
    }

    g_wireless_state.antenna_rssi_ant1 = ant1_rssi;
    g_wireless_state.antenna_rssi_ant2 = ant2_rssi;

    final_ant = WIRELESS_ANT1;
    if (ant2_rssi > ant1_rssi) {
        final_ant = WIRELESS_ANT2;
    } else if ((ant2_rssi == ant1_rssi) && (ant2_ok > ant1_ok)) {
        final_ant = WIRELESS_ANT2;
    } else if ((ant2_rssi == ant1_rssi) && (ant2_ok == ant1_ok) && (ant2_crc < ant1_crc)) {
        final_ant = WIRELESS_ANT2;
    }

    if ((ant1_ok == 0U) && (ant2_ok == 0U) &&
        (ant1_rssi < WIRELESS_SIGNAL_RSSI_MIN) &&
        (ant2_rssi < WIRELESS_SIGNAL_RSSI_MIN)) {
        g_wireless_state.scan_has_signal = 0U;
        final_ant = WIRELESS_ANT1;
        LOGW(WIRELESS_TAG, "scan no signal, fallback ANT1");
    } else {
        g_wireless_state.scan_has_signal = 1U;
    }

    rc = Wireless_SetAntenna(final_ant);
    if (rc != SUCCESS) {
        return rc;
    }
    rc = Wireless_SetRxMode();
    if (rc != SUCCESS) {
        return rc;
    }

    g_wireless_state.ready = 1U;
    LOGI(WIRELESS_TAG, "scan ant1 rssi=%u ok=%u crc=%u ant2 rssi=%u ok=%u crc=%u final=%u",
         ant1_rssi, ant1_ok, ant1_crc,
         ant2_rssi, ant2_ok, ant2_crc,
         (u16)final_ant);
    return SUCCESS;
}

s8 Wireless_SearchSignalPoll(void)
{
    static u16 poll_div = 0U;
    s8 rc;

    if (!g_wireless_state.initialized) {
        return WIRELESS_ERR_STATE;
    }
    if (g_wireless_state.scan_has_signal) {
        return SUCCESS;
    }

    poll_div++;
    if (poll_div < WIRELESS_SEARCH_POLL_DIV) {
        return SUCCESS;
    }
    poll_div = 0U;

    rc = Wireless_RescanAntenna();
    if (rc != SUCCESS) {
        g_wireless_state.last_error = rc;
        LOGE(WIRELESS_TAG, "search rescan fail rc=%d", rc);
        return rc;
    }
    if (g_wireless_state.scan_has_signal) {
        LOGI(WIRELESS_TAG, "signal detected ant=%u rssi=%u/%u",
             (u16)g_wireless_state.antenna,
             g_wireless_state.antenna_rssi_ant1,
             g_wireless_state.antenna_rssi_ant2);
    }
    return SUCCESS;
}

s8 Wireless_RunMinimalTest(void)
{
    u16 reg3;
    u16 reg6;
    u16 reg11;
    u16 reg41;
    s8 rc;

    if ((!g_wireless_state.initialized) || (!g_wireless_state.ready)) {
        return WIRELESS_ERR_STATE;
    }

    rc = LT8920_ReadReg(3U, &reg3);
    if (rc != SUCCESS) {
        LOGE(WIRELESS_TAG, "test read reg3 fail rc=%d", rc);
        return rc;
    }

    rc = LT8920_ReadReg(6U, &reg6);
    if (rc != SUCCESS) {
        LOGE(WIRELESS_TAG, "test read reg6 fail rc=%d", rc);
        return rc;
    }

    rc = LT8920_ReadReg(11U, &reg11);
    if (rc != SUCCESS) {
        LOGE(WIRELESS_TAG, "test read reg11 fail rc=%d", rc);
        return rc;
    }

    rc = LT8920_ReadReg(41U, &reg41);
    if (rc != SUCCESS) {
        LOGE(WIRELESS_TAG, "test read reg41 fail rc=%d", rc);
        return rc;
    }

    if (reg11 != 0x0008U) {
        LOGE(WIRELESS_TAG, "test verify reg11 fail val=0x%04X", reg11);
        return WIRELESS_ERR_VERIFY;
    }
    if (reg41 != 0xB000U) {
        LOGE(WIRELESS_TAG, "test verify reg41 fail val=0x%04X", reg41);
        return WIRELESS_ERR_VERIFY;
    }

    LOGI(WIRELESS_TAG,
         "test ok no-whoami reg3=0x%04X reg6=0x%04X reg11=0x%04X reg41=0x%04X",
         reg3, reg6, reg11, reg41);
    return SUCCESS;
}
