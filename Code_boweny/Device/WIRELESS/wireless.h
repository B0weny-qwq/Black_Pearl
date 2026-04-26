#ifndef __WIRELESS_H__
#define __WIRELESS_H__

#include "config.h"

#define WIRELESS_OK                SUCCESS
#define WIRELESS_ERR_PARAM         (-2)
#define WIRELESS_ERR_STATE         (-3)
#define WIRELESS_ERR_IO            (-4)
#define WIRELESS_ERR_TIMEOUT       (-5)
#define WIRELESS_ERR_EMPTY         (-6)
#define WIRELESS_ERR_OVERFLOW      (-7)
#define WIRELESS_ERR_VERIFY        (-8)

#define WIRELESS_TAG               "WL"

#define WIRELESS_ANT1              0U
#define WIRELESS_ANT2              1U

#define WIRELESS_MODE_IDLE         0U
#define WIRELESS_MODE_RX           1U
#define WIRELESS_MODE_TX           2U

#define WIRELESS_RX_QUEUE_DEPTH    4U
#define WIRELESS_SCAN_SAMPLE_COUNT 16U
#define WIRELESS_SCAN_SAMPLE_MS    2U
#define WIRELESS_SIGNAL_RSSI_MIN   12U
#define WIRELESS_SEARCH_POLL_DIV   32U
#define WIRELESS_TX_TIMEOUT_LOOPS  2000U

typedef struct
{
    u8 initialized;
    u8 ready;
    u8 mode;
    u8 antenna;
    u8 scan_has_signal;
    s8 last_error;

    u16 tx_ok_count;
    u16 rx_ok_count;
    u16 crc_error_count;
    u16 queue_overflow_count;
    u16 rx_drop_count;

    u16 antenna_rssi_ant1;
    u16 antenna_rssi_ant2;
} Wireless_State_t;

s8 Wireless_Init(void);
s8 Wireless_Deinit(void);
s8 Wireless_Poll(void);
s8 Wireless_Send(const u8 *buf, u8 len);
s8 Wireless_Receive(u8 *buf, u8 buf_len, u8 *out_len);
s8 Wireless_SetAntenna(u8 ant_sel);
s8 Wireless_GetState(Wireless_State_t *state);
s8 Wireless_RescanAntenna(void);
s8 Wireless_SearchSignalPoll(void);
s8 Wireless_RunMinimalTest(void);

#endif
