#ifndef __LT8920_H__
#define __LT8920_H__

#include "config.h"

#define LT8920_MAX_PAYLOAD_LEN       60U

#define LT8920_DEFAULT_CHANNEL       0x30U
#define LT8920_DEFAULT_SYNC_WORD     0x03800380UL

#define LT89xx_6dBm                  0x4800U
#define RF_Power                     LT89xx_6dBm
#define RadioFrequency_user          2450U
#define Test_Channel                 (RadioFrequency_user - 2402U)
#define Pair_Channel                 0x7FU
#define Packet_Length                12U
#define Tx_Interval_mS               10U
#define Work_Type                    0U
#define SyncPairWord                 0xE4E4E0E0UL
#define SyncTransferWord             0x6E6EFCFCUL

#define LT8920_STATUS_CRC_ERROR      0x8000U
#define LT8920_STATUS_SYNC_RECV      0x0080U
#define LT8920_STATUS_PKT_FLAG       0x0040U
#define LT8920_STATUS_FIFO_FLAG      0x0020U

s8 LT8920_Init(u8 channel, u32 sync_word);
s8 LT8920_SetChannel(u8 channel);
s8 LT8920_SetSyncWord(u32 sync_word);
s8 LT8920_SetSyncRegs(u16 reg36, u16 reg39);
s8 LT8920_EnterIdle(void);
s8 LT8920_EnterRx(void);
s8 LT8920_EnterTx(void);
s8 LT8920_EnterCarrierWave(void);
s8 LT8920_OpenRx(void);
s8 LT8920_StartTxPacket(const u8 *buf, u8 len);
s8 LT8920_ReadStatus(u16 *status);
s8 LT8920_ReadRawRssi(u8 *rssi);
s8 LT8920_ClearTxFifo(void);
s8 LT8920_ClearRxFifo(void);
s8 LT8920_WritePacket(const u8 *buf, u8 len);
s8 LT8920_ForceTxPacket(const u8 *buf, u8 len);
s8 LT8920_ReadPacket(u8 *buf, u8 buf_len, u8 *out_len);
s8 LT8920_ReadReg(u8 reg, u16 *value);
void LT8920_GetVerifyFailure(u8 *reg, u16 *expected, u16 *actual);
u8 LT8920_GetLastTxFifoCount(void);

#endif
