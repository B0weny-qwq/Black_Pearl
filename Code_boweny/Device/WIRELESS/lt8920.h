#ifndef __LT8920_H__
#define __LT8920_H__

#include "config.h"

#define LT8920_MAX_PAYLOAD_LEN       60U

#define LT8920_DEFAULT_CHANNEL       0U
#define LT8920_DEFAULT_SYNC_WORD     0xE4E4E0E0UL

#define LT8920_STATUS_PKT_FLAG       0x2000U
#define LT8920_STATUS_CRC_ERROR      0x0800U

s8 LT8920_Init(u8 channel, u32 sync_word);
s8 LT8920_SetChannel(u8 channel);
s8 LT8920_SetSyncWord(u32 sync_word);
s8 LT8920_EnterIdle(void);
s8 LT8920_EnterRx(void);
s8 LT8920_EnterTx(void);
s8 LT8920_ReadStatus(u16 *status);
s8 LT8920_ReadRawRssi(u8 *rssi);
s8 LT8920_ClearTxFifo(void);
s8 LT8920_ClearRxFifo(void);
s8 LT8920_WritePacket(const u8 *buf, u8 len);
s8 LT8920_ReadPacket(u8 *buf, u8 buf_len, u8 *out_len);
s8 LT8920_ReadReg(u8 reg, u16 *value);

#endif
