#include "lt8920.h"

#include "wireless.h"
#include "wireless_port.h"

typedef struct
{
    u8 reg;
    u16 value;
} LT8920_RegInit_t;

static const LT8920_RegInit_t g_lt8920_default_regs[] =
{
    {0U,  0x6FE0U},
    {1U,  0x5681U},
    {2U,  0x6617U},
    {4U,  0x9CC9U},
    {5U,  0x6637U},
    {8U,  0x6C90U},
    {9U,  0x1840U},
    {10U, 0x7FFDU},
    {11U, 0x0008U},
    {12U, 0x0000U},
    {13U, 0x48BDU},
    {22U, 0x00FFU},
    {23U, 0x8005U},
    {24U, 0x0067U},
    {25U, 0x1659U},
    {26U, 0x19E0U},
    {27U, 0x1300U},
    {28U, 0x1800U},
    {32U, 0x4800U},
    {33U, 0x3FC7U},
    {34U, 0x2000U},
    {35U, 0x0300U},
    {36U, 0x0000U},
    {37U, 0x0000U},
    {38U, 0x0000U},
    {39U, 0x0000U},
    {40U, 0x2102U},
    {41U, 0xB000U},
    {42U, 0xFDB0U},
    {43U, 0x000FU},
    {44U, 0x1000U},
    {45U, 0x0552U}
};

static u8 g_lt8920_initialized = 0U;
static u8 g_lt8920_channel = LT8920_DEFAULT_CHANNEL;
static u32 g_lt8920_sync_word = LT8920_DEFAULT_SYNC_WORD;

static u8 LT8920_BuildCommand(u8 reg, u8 read)
{
    return (u8)((read ? 0x80U : 0x00U) | (reg & 0x7FU));
}

static s8 LT8920_WriteReg(u8 reg, u16 value)
{
    WirelessPort_SetCs(0U);
    (void)WirelessPort_SpiTransfer(LT8920_BuildCommand(reg, 0U));
    (void)WirelessPort_SpiTransfer((u8)(value >> 8));
    (void)WirelessPort_SpiTransfer((u8)(value & 0x00FFU));
    WirelessPort_SetCs(1U);
    return SUCCESS;
}

static s8 LT8920_WriteFifoByte(u8 value)
{
    WirelessPort_SetCs(0U);
    (void)WirelessPort_SpiTransfer(LT8920_BuildCommand(50U, 0U));
    (void)WirelessPort_SpiTransfer(value);
    WirelessPort_SetCs(1U);
    return SUCCESS;
}

static s8 LT8920_ReadFifoByte(u8 *value)
{
    if (value == 0) {
        return WIRELESS_ERR_PARAM;
    }

    WirelessPort_SetCs(0U);
    (void)WirelessPort_SpiTransfer(LT8920_BuildCommand(50U, 1U));
    *value = WirelessPort_SpiTransfer(0x00U);
    WirelessPort_SetCs(1U);
    return SUCCESS;
}

static s8 LT8920_ReadFifoCount(u8 *count)
{
    u16 reg52;
    s8 rc;

    if (count == 0) {
        return WIRELESS_ERR_PARAM;
    }

    rc = LT8920_ReadReg(52U, &reg52);
    if (rc != SUCCESS) {
        return rc;
    }

    *count = (u8)(reg52 & 0x003FU);
    return SUCCESS;
}

static s8 LT8920_UpdateModeRegister(u16 mode_flags)
{
    return LT8920_WriteReg(7U, (u16)(mode_flags | (u16)(g_lt8920_channel & 0x7FU)));
}

static void LT8920_DrainRxBytes(u8 count)
{
    u8 dummy;

    while (count > 0U) {
        (void)LT8920_ReadFifoByte(&dummy);
        count--;
    }
}

static s8 LT8920_LoadDefaultProfile(void)
{
    u8 i;
    s8 rc;

    for (i = 0U; i < (u8)(sizeof(g_lt8920_default_regs) / sizeof(g_lt8920_default_regs[0])); i++) {
        rc = LT8920_WriteReg(g_lt8920_default_regs[i].reg, g_lt8920_default_regs[i].value);
        if (rc != SUCCESS) {
            return rc;
        }
    }

    return SUCCESS;
}

s8 LT8920_ReadReg(u8 reg, u16 *value)
{
    u8 msb;
    u8 lsb;

    if (value == 0) {
        return WIRELESS_ERR_PARAM;
    }

    WirelessPort_SetCs(0U);
    (void)WirelessPort_SpiTransfer(LT8920_BuildCommand(reg, 1U));
    msb = WirelessPort_SpiTransfer(0x00U);
    lsb = WirelessPort_SpiTransfer(0x00U);
    WirelessPort_SetCs(1U);

    *value = (u16)(((u16)msb << 8) | lsb);
    return SUCCESS;
}

s8 LT8920_Init(u8 channel, u32 sync_word)
{
    u16 verify_reg;
    s8 rc;

    if (channel > 0x7FU) {
        return WIRELESS_ERR_PARAM;
    }

    rc = LT8920_LoadDefaultProfile();
    if (rc != SUCCESS) {
        return rc;
    }

    g_lt8920_channel = channel;
    g_lt8920_sync_word = sync_word;

    rc = LT8920_SetSyncWord(sync_word);
    if (rc != SUCCESS) {
        return rc;
    }

    rc = LT8920_SetChannel(channel);
    if (rc != SUCCESS) {
        return rc;
    }

    rc = LT8920_ClearTxFifo();
    if (rc != SUCCESS) {
        return rc;
    }

    rc = LT8920_ClearRxFifo();
    if (rc != SUCCESS) {
        return rc;
    }

    rc = LT8920_EnterIdle();
    if (rc != SUCCESS) {
        return rc;
    }

    rc = LT8920_ReadReg(11U, &verify_reg);
    if ((rc != SUCCESS) || (verify_reg != 0x0008U)) {
        return WIRELESS_ERR_VERIFY;
    }

    rc = LT8920_ReadReg(41U, &verify_reg);
    if ((rc != SUCCESS) || (verify_reg != 0xB000U)) {
        return WIRELESS_ERR_VERIFY;
    }

    g_lt8920_initialized = 1U;
    return SUCCESS;
}

s8 LT8920_SetChannel(u8 channel)
{
    g_lt8920_channel = (u8)(channel & 0x7FU);
    return LT8920_EnterIdle();
}

s8 LT8920_SetSyncWord(u32 sync_word)
{
    s8 rc;

    g_lt8920_sync_word = sync_word;

    rc = LT8920_WriteReg(36U, (u16)(sync_word & 0xFFFFUL));
    if (rc != SUCCESS) {
        return rc;
    }
    rc = LT8920_WriteReg(37U, 0x0000U);
    if (rc != SUCCESS) {
        return rc;
    }
    rc = LT8920_WriteReg(38U, 0x0000U);
    if (rc != SUCCESS) {
        return rc;
    }
    return LT8920_WriteReg(39U, (u16)(sync_word >> 16));
}

s8 LT8920_EnterIdle(void)
{
    return LT8920_UpdateModeRegister(0x0000U);
}

s8 LT8920_EnterRx(void)
{
    return LT8920_UpdateModeRegister(0x0080U);
}

s8 LT8920_EnterTx(void)
{
    return LT8920_UpdateModeRegister(0x0100U);
}

s8 LT8920_ReadStatus(u16 *status)
{
    return LT8920_ReadReg(48U, status);
}

s8 LT8920_ReadRawRssi(u8 *rssi)
{
    u16 reg6;
    s8 rc;

    if (rssi == 0) {
        return WIRELESS_ERR_PARAM;
    }

    rc = LT8920_ReadReg(6U, &reg6);
    if (rc != SUCCESS) {
        return rc;
    }

    *rssi = (u8)((reg6 >> 10) & 0x003FU);
    return SUCCESS;
}

s8 LT8920_ClearTxFifo(void)
{
    return LT8920_WriteReg(52U, 0x8000U);
}

s8 LT8920_ClearRxFifo(void)
{
    return LT8920_WriteReg(52U, 0x0080U);
}

s8 LT8920_WritePacket(const u8 *buf, u8 len)
{
    u8 i;
    s8 rc;

    if ((buf == 0) || (len == 0U) || (len > LT8920_MAX_PAYLOAD_LEN)) {
        return WIRELESS_ERR_PARAM;
    }
    if (!g_lt8920_initialized) {
        return WIRELESS_ERR_STATE;
    }

    rc = LT8920_ClearTxFifo();
    if (rc != SUCCESS) {
        return rc;
    }

    rc = LT8920_WriteFifoByte(len);
    if (rc != SUCCESS) {
        return rc;
    }

    for (i = 0U; i < len; i++) {
        rc = LT8920_WriteFifoByte(buf[i]);
        if (rc != SUCCESS) {
            return rc;
        }
    }

    return SUCCESS;
}

s8 LT8920_ReadPacket(u8 *buf, u8 buf_len, u8 *out_len)
{
    u8 fifo_count;
    u8 packet_len;
    u8 available_len;
    u8 i;
    s8 rc;

    if ((buf == 0) || (out_len == 0) || (buf_len == 0U)) {
        return WIRELESS_ERR_PARAM;
    }
    if (!g_lt8920_initialized) {
        return WIRELESS_ERR_STATE;
    }

    rc = LT8920_ReadFifoCount(&fifo_count);
    if (rc != SUCCESS) {
        return rc;
    }
    if (fifo_count == 0U) {
        return WIRELESS_ERR_EMPTY;
    }

    rc = LT8920_ReadFifoByte(&packet_len);
    if (rc != SUCCESS) {
        return rc;
    }
    available_len = (u8)(fifo_count - 1U);

    if (packet_len == 0U) {
        LT8920_DrainRxBytes(available_len);
        return WIRELESS_ERR_IO;
    }
    if (packet_len > LT8920_MAX_PAYLOAD_LEN) {
        LT8920_DrainRxBytes(available_len);
        return WIRELESS_ERR_OVERFLOW;
    }
    if (packet_len > buf_len) {
        LT8920_DrainRxBytes(available_len);
        return WIRELESS_ERR_OVERFLOW;
    }
    if (packet_len > available_len) {
        LT8920_DrainRxBytes(available_len);
        return WIRELESS_ERR_IO;
    }

    for (i = 0U; i < packet_len; i++) {
        rc = LT8920_ReadFifoByte(&buf[i]);
        if (rc != SUCCESS) {
            LT8920_DrainRxBytes((u8)(packet_len - i - 1U));
            return rc;
        }
    }

    if (packet_len < available_len) {
        LT8920_DrainRxBytes((u8)(available_len - packet_len));
    }

    *out_len = packet_len;
    return SUCCESS;
}
