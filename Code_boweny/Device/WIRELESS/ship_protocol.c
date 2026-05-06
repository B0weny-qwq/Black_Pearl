/**
 * @file    ship_protocol.c
 * @brief   船端旧遥控器无线业务协议移植实现。
 * @author  boweny
 * @date    2026-05-06
 * @version v1.1
 *
 * @details
 * 本文件对齐 `Wireless_other/wirelessProtocal.c` 的配对、收包解析、
 * 固定 `0x12` 回传和遥控器 `0x11` 数据处理逻辑。当前默认测试目标是：
 * 配对成功后打印进入工作通道，收到遥控器油门/按键后只打印数据。
 *
 * @note
 * 真实 PWM 油门输出由 `SHIP_THROTTLE_PWM_ENABLE` 控制，默认关闭，避免
 * 遥控器联调阶段误驱动电机。
 */
#include "ship_protocol.h"

#include "wireless.h"
#include "..\..\Device\GPS\GPS.h"
#include "..\..\Device\Motor\Motor.h"
#include "..\..\Function\Log\Log.h"
#include "..\..\..\User\Task.h"
#include "..\..\..\Driver\inc\STC32G_ADC.h"

#if defined(SHIP_PAIR_SEED_USE_CHIPID) && (SHIP_PAIR_SEED_USE_CHIPID != 0)
#include "..\..\..\User\STC32G.h"
#endif

#define SHIP_TAG "SHIP"
#define SHIP_PAIR_FIX_REV "pairbiz-r3"

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

#ifndef SHIP_PAIR_RSP_EXPIRE_LOG_MS
#define SHIP_PAIR_RSP_EXPIRE_LOG_MS    5000UL
#endif

#ifndef SHIP_RX_IDLE_WARN_MS
#define SHIP_RX_IDLE_WARN_MS           2000UL
#endif

#ifndef SHIP_THROTTLE_TIMEOUT_MS
#define SHIP_THROTTLE_TIMEOUT_MS       1500UL
#endif

#ifndef SHIP_THROTTLE_PWM_ENABLE
#define SHIP_THROTTLE_PWM_ENABLE       0
#endif

#ifndef SHIP_WORK_RX_REOPEN_TICKS
#define SHIP_WORK_RX_REOPEN_TICKS      10U
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

#ifndef SHIP_ADC_REF_MV
#define SHIP_ADC_REF_MV                3300UL
#endif

#ifndef SHIP_BAT_DIV_NUM
#define SHIP_BAT_DIV_NUM               1UL
#endif

#ifndef SHIP_BAT_DIV_DEN
#define SHIP_BAT_DIV_DEN               1UL
#endif

#ifndef SHIP_ADC_LOG_ENABLE
#define SHIP_ADC_LOG_ENABLE            1
#endif

#define SHIP_LEGACY_PROTO_MAX_LEN      30U

typedef enum
{
    SHIP_STATE_BOOT_WAIT = 0,
    SHIP_STATE_PAIR_SEND,
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
    u16 wait_ticks;
    u16 pair_left;
    u16 pair_retry_count;
    u16 work_rx_reopen_ticks;
    u16 work_rx_reopen_total;
    u32 pair_wait_start_ms;
    u32 last_proto_rx_ms;
    u32 last_throttle_rx_ms;
    u8 pair_rsp_timeout_logged;
    u8 rx_idle_warned;
    u8 throttle_online;
#if SHIP_THROTTLE_PWM_ENABLE
    u8 motor_initialized;
#endif
} ShipRuntime_t;

typedef struct
{
    u16 raw;
    u16 adc_mv;
    u32 bat_mv;
    u8 report;
    u8 valid;
} ShipPowerSample_t;

static ShipRuntime_t g_ship_rt;

static s8 ShipProtocol_ApplyWorkSyncIdle(u8 log_rxdbg);
static s8 ShipProtocol_ApplyWorkRx(u8 log_rxdbg);
static void ShipProtocol_LogRxDebug(const u8 *stage);
static void ShipProtocol_LogPayloadBrief(const u8 *stage, u8 cmd, const u8 *payload, u8 payload_len);

#if SHIP_THROTTLE_PWM_ENABLE
/**
 * @brief      将遥控器 `ud` 油门字节转换为电机速度命令。
 * @param[in]  throttle  遥控器油门字节，旧版中心值按 100 处理。
 * @return     电机速度命令，范围约为 `-1000~1000`。
 *
 * @details
 * 旧业务 `frontBack` 中心值为 100，前进大于 100，后退小于 100。
 * 这里只在显式打开 `SHIP_THROTTLE_PWM_ENABLE` 后使用。
 */
static int16 ShipProtocol_ThrottleToMotorSpeed(u8 throttle)
{
    int16 delta;
    int16 speed;

    delta = (int16)throttle - 100;
    if ((delta > -10) && (delta < 10)) {
        return 0;
    }

    speed = (int16)(delta * 10);
    if (speed > MOTOR_SPEED_MAX) {
        return MOTOR_SPEED_MAX;
    }
    if (speed < -MOTOR_SPEED_MAX) {
        return -MOTOR_SPEED_MAX;
    }
    return speed;
}

/**
 * @brief      在宏允许时把遥控油门输出到真实电机 PWM。
 * @param[in]  throttle  遥控器油门字节。
 * @return     无。
 */
static void ShipProtocol_ApplyThrottlePwm(u8 throttle)
{
    int16 speed;

    if (g_ship_rt.motor_initialized == 0U) {
        Motor_Init();
        g_ship_rt.motor_initialized = 1U;
        LOGW(SHIP_TAG, "throttle pwm enabled, real motor output active");
    }

    speed = ShipProtocol_ThrottleToMotorSpeed(throttle);
    Motor_SetBothSpeed(speed, speed);
    LOGI(SHIP_TAG, "pwm throttle_out left=%d right=%d", speed, speed);
}
#endif

/**
 * @brief      将 ADC 原始值换算为芯片 ADC 输入端电压。
 * @param[in]  adc_raw  12 位 ADC 原始采样值。
 * @return     ADC 输入端电压，单位 mV。
 *
 * @details
 * STC32G ADC 当前按右对齐 12 位读取。这里仅用于串口打印，不改变
 * 老版无线 `0x12` 数据格式。
 */
static u16 ShipProtocol_AdcRawToMv(u16 adc_raw)
{
    return (u16)(((u32)adc_raw * (u32)SHIP_ADC_REF_MV) / 4095UL);
}

/**
 * @brief      按分压比例还原电池端电压。
 * @param[in]  adc_mv  ADC 输入端电压，单位 mV。
 * @return     估算电池电压，单位 mV。
 *
 * @details
 * 若硬件前端没有分压，`SHIP_BAT_DIV_NUM/SHIP_BAT_DIV_DEN` 保持 `1/1`。
 * 若电池经电阻分压接入 P0.0，应在 `User/Config.h` 中按实际比例配置。
 */
static u32 ShipProtocol_AdcMvToBatteryMv(u16 adc_mv)
{
    if (SHIP_BAT_DIV_DEN == 0UL) {
        return (u32)adc_mv;
    }
    return (((u32)adc_mv * (u32)SHIP_BAT_DIV_NUM) / (u32)SHIP_BAT_DIV_DEN);
}

/**
 * @brief      读取 P0.0 ADC，并生成旧版 `0x12` power 字节。
 * @param[out] sample  输出 ADC 原始值、电压估算值和 1 字节 power 字段。
 * @return     无。
 *
 * @details
 * 老版 `0x12` 帧中 power 字段只有 1 字节。本函数不扩展无线数据帧，
 * 仅把 12 位 ADC 原始值压缩为 1 字节放入原位置，同时通过串口打印
 * `raw/adc_mv/bat_mv/power`，便于现场对照老版 `Power_ADC_Get_Level()`。
 */
static void ShipProtocol_ReadPowerSample(ShipPowerSample_t *sample)
{
    u16 adc_raw;

    if (sample == 0) {
        return;
    }

    adc_raw = Get_ADCResult(ADC_CH8);
    if (adc_raw > 4095U) {
        sample->raw = adc_raw;
        sample->adc_mv = 0U;
        sample->bat_mv = 0UL;
        sample->report = 0xFFU;
        sample->valid = 0U;
        return;
    }

    sample->raw = adc_raw;
    sample->adc_mv = ShipProtocol_AdcRawToMv(adc_raw);
    sample->bat_mv = ShipProtocol_AdcMvToBatteryMv(sample->adc_mv);
    sample->report = (u8)(adc_raw >> 4);
    sample->valid = 1U;
}

/**
 * @brief      打印本次电量 ADC 采样结果。
 * @param[in]  sample  ADC 采样结果。
 * @return     无。
 */
static void ShipProtocol_LogPowerSample(const ShipPowerSample_t *sample)
{
#if SHIP_ADC_LOG_ENABLE
    if (sample == 0) {
        return;
    }
    if (sample->valid == 0U) {
        LOGW(SHIP_TAG, "adc p0.0 read fail raw=%u power=0x%02X",
             (u16)sample->raw,
             (u16)sample->report);
        return;
    }

    LOGI(SHIP_TAG, "adc p0.0 raw=%u adc_mv=%u bat_mv=%lu power=0x%02X",
         (u16)sample->raw,
         (u16)sample->adc_mv,
         (u32)sample->bat_mv,
         (u16)sample->report);
#else
    (void)sample;
#endif
}

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
 * 配对 seed 是 cmd=0x10 携带的 4 字节 payload：
 *   AA | 06 | 10 | seed0 | seed1 | seed2 | seed3 | xor | BB
 *
 * 在船端移植实现里，这个 seed 不是单纯记录值，而是派生后续工作信道和
 * 同步 key 的实时输入。遥控器期待的 seed 不一致时，即使 RF TX 正常也会配不上。
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

    return Wireless_SendOnChannel(channel, frame, idx);
}

static void ShipProtocol_ApplyDefaultRf(void)
{
    u8 seed[4];
    u8 channel;

    /* 工作收发信道和同步 key 都由同一个 4 字节 seed 派生。
     * 这里必须保持旧遥控器公式，不能按新协议理解为普通随机数。
     */
    ShipProtocol_GetPairSeed(seed);

    g_ship_rt.rf_send_key[0] =
        (u8)(((u8)((u8)(seed[0] << 4) >> 4)) + ((u8)(seed[3] >> 2) + (u8)(seed[3] % 0x03U)));
    g_ship_rt.rf_send_key[1] =
        (u8)(((u8)((u8)(seed[1] << 4) >> 4)) + ((u8)(seed[2] >> 3) + (u8)(seed[0] % 0x06U)));

    channel = (u8)(((u8)(((seed[3] + 0x06U) % 0x40U) +
                         ((seed[2] >> 3) * 0x08U) +
                         (((seed[1] | seed[0]) % 0x08U) / 2U))) % 0x40U);
    g_ship_rt.rf_channel[0] = channel;
    g_ship_rt.rf_channel[1] = channel;
    g_ship_rt.rf_channel[2] = (u8)(channel + 0x40U);
}

static s8 ShipProtocol_ApplyWorkSyncIdle(u8 log_rxdbg)
{
    u16 reg36;
    u16 reg39;
    s8 rc;

    reg36 = (u16)(((u16)g_ship_rt.rf_send_key[0] << 8) | g_ship_rt.rf_send_key[0]);
    reg39 = (u16)(((u16)g_ship_rt.rf_send_key[1] << 8) | g_ship_rt.rf_send_key[1]);

    rc = Wireless_SetSyncRegsIdle(reg36, reg39);
    if (rc != SUCCESS) {
        return rc;
    }

    g_ship_rt.work_rx_configured = 0U;
    g_ship_rt.work_rx_reopen_ticks = 0U;
    if (log_rxdbg != 0U) {
        ShipProtocol_LogRxDebug("pair-sync-idle");
    }

    return SUCCESS;
}

static s8 ShipProtocol_ApplyWorkRx(u8 log_rxdbg)
{
    s8 rc;

    rc = Wireless_SetChannel(g_ship_rt.rf_channel[0]);
    if (rc == SUCCESS) {
        g_ship_rt.work_rx_configured = 1U;
        g_ship_rt.work_rx_reopen_ticks = 0U;
        if (log_rxdbg != 0U) {
            ShipProtocol_LogRxDebug("work-rx");
        }
    }
    return rc;
}

static void ShipProtocol_LogRxDebug(const u8 *stage)
{
    Wireless_RxDebug_t dbg;
    s8 rc;

    rc = Wireless_GetRxDebug(&dbg);
    if (rc != SUCCESS) {
        LOGW(SHIP_TAG, "rxdbg %s read fail rc=%d", stage, rc);
        return;
    }

    LOGI(SHIP_TAG,
         "rxdbg %s ch=%u reg7=0x%04X reg8=0x%04X reg36=0x%04X reg37=0x%04X reg38=0x%04X reg39=0x%04X reg48=0x%04X reg52=0x%04X rssi=%u rxbit=%u mode=%u rxen=%u txen=%u",
         stage,
         (u16)dbg.channel,
         dbg.reg7,
         dbg.reg8,
         dbg.reg36,
         dbg.reg37,
         dbg.reg38,
         dbg.reg39,
         dbg.reg48,
         dbg.reg52,
         (u16)dbg.rssi,
         (u16)dbg.rx_mode_bit,
         (u16)dbg.mode,
         (u16)dbg.rx_en,
         (u16)dbg.tx_en);
}

static void ShipProtocol_LogPayloadBrief(const u8 *stage, u8 cmd, const u8 *payload, u8 payload_len)
{
    LOGI(SHIP_TAG,
         "%s cmd=0x%02X len=%u data=%02X %02X %02X %02X %02X %02X",
         stage,
         (u16)cmd,
         (u16)payload_len,
         (u16)((payload_len > 0U) ? payload[0] : 0U),
         (u16)((payload_len > 1U) ? payload[1] : 0U),
         (u16)((payload_len > 2U) ? payload[2] : 0U),
         (u16)((payload_len > 3U) ? payload[3] : 0U),
         (u16)((payload_len > 4U) ? payload[4] : 0U),
         (u16)((payload_len > 5U) ? payload[5] : 0U));
}

static s8 ShipProtocol_TryPairSend(u16 left_after_send)
{
    u8 pair_data[4];
    u8 pair_xor;
    s8 rc;

    /* cmd=0x10 的 payload 固定为当前 4 字节配对 seed。 */
    ShipProtocol_GetPairSeed(pair_data);
    pair_xor = (u8)(0x06U ^ SHIP_CMD_PAIR ^ pair_data[0] ^
                    pair_data[1] ^ pair_data[2] ^ pair_data[3]);

    rc = ShipProtocol_SendFrame(SHIP_PAIR_CHANNEL_DEFAULT, SHIP_CMD_PAIR, pair_data, 4U);
    if (rc == SUCCESS) {
        LOGI(SHIP_TAG,
             "pair req sent seq=%u/%u retry=%u ch=0x%02X",
             (u16)(SHIP_PAIR_SEND_TIMES - left_after_send),
             (u16)SHIP_PAIR_SEND_TIMES,
             (u16)g_ship_rt.pair_retry_count,
             (u16)SHIP_PAIR_CHANNEL_DEFAULT);
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
            LOGI(SHIP_TAG,
                 "pair req frame=AA 06 10 %02X %02X %02X %02X %02X BB",
                 (u16)pair_data[0],
                 (u16)pair_data[1],
                 (u16)pair_data[2],
                 (u16)pair_data[3],
                 (u16)pair_xor);
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
    ShipPowerSample_t power;
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

    ShipProtocol_ReadPowerSample(&power);
    payload[idx++] = power.report;
    payload[idx++] = 0U; /* autodrive placeholder */

    if (idx != 15U) {
        LOGE(SHIP_TAG, "gps payload len bad=%u", (u16)idx);
        return;
    }

    ShipProtocol_LogPowerSample(&power);
    LOGI(SHIP_TAG,
         "tx cmd=0x12 ch=%u payload_len=%u sat=%u angle=%u power=0x%02X auto=0x%02X",
         (u16)g_ship_rt.rf_channel[0],
         (u16)idx,
         (u16)payload[0],
         (u16)(((u16)payload[2] << 8) | payload[1]),
         (u16)payload[13],
         (u16)payload[14]);
    ShipProtocol_LogPayloadBrief("tx frame", SHIP_CMD_GPS_REPORT, payload, idx);

    rc = ShipProtocol_SendFrame(g_ship_rt.rf_channel[0], SHIP_CMD_GPS_REPORT, payload, idx);
    if (rc != SUCCESS) {
        LOGE(SHIP_TAG, "gps tx fail rc=%d ch=0x%02X", rc, (u16)g_ship_rt.rf_channel[0]);
    }
    g_ship_rt.work_rx_configured = 0U;
}

static void ShipProtocol_HandlePairRsp(const u8 *payload, u8 payload_len)
{
    if (g_ship_rt.pair_wait_rsp_time == 0U) {
        return;
    }

    ShipProtocol_LogPayloadBrief("pair rsp rx", SHIP_CMD_PAIR_RSP, payload, payload_len);
    g_ship_rt.pair_wait_rsp_time = 0U;
    g_ship_rt.paired = 1U;
    LOGI(SHIP_TAG, "pair ok, enter work channel rx_ch=%u tx_ch=%u",
         (u16)g_ship_rt.rf_channel[0],
         (u16)g_ship_rt.rf_channel[0]);

    if ((payload != 0) && (payload_len == 4U)) {
        LOGI(SHIP_TAG,
             "pair success paired=1 work_rx=%u work_tx=%u key=%u/%u rsp=%02X%02X%02X%02X",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)payload[0], (u16)payload[1],
             (u16)payload[2], (u16)payload[3]);
    } else {
        LOGI(SHIP_TAG,
             "pair success paired=1 work_rx=%u work_tx=%u key=%u/%u rsp_len=%u",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_send_key[0],
             (u16)g_ship_rt.rf_send_key[1],
             (u16)payload_len);
    }
}

static void ShipProtocol_HandleThrottle(const u8 *payload, u8 payload_len)
{
    u32 now_ms;

    if (payload_len < 3U) {
        LOGW(SHIP_TAG, "throttle short len=%u", (u16)payload_len);
        return;
    }

    g_ship_rt.lr = payload[0];
    g_ship_rt.ud = payload[1];
    g_ship_rt.key = payload[2];
    g_ship_rt.valid = 1U;
    now_ms = Task_GetTickMs();
    g_ship_rt.last_throttle_rx_ms = now_ms;
    g_ship_rt.last_proto_rx_ms = now_ms;
    g_ship_rt.rx_idle_warned = 0U;
    if (g_ship_rt.throttle_online == 0U) {
        g_ship_rt.throttle_online = 1U;
        LOGI(SHIP_TAG, "remote link online by cmd=0x11");
    }

    LOGI(SHIP_TAG, "rc lr=%u ud=%u key=0x%02X paired=%u",
         (u16)g_ship_rt.lr,
         (u16)g_ship_rt.ud,
         (u16)g_ship_rt.key,
         (u16)g_ship_rt.paired);
    LOGI(SHIP_TAG, "throttle=%u steering=%u key=0x%02X",
         (u16)g_ship_rt.ud,
         (u16)g_ship_rt.lr,
         (u16)g_ship_rt.key);

#if SHIP_THROTTLE_PWM_ENABLE
    ShipProtocol_ApplyThrottlePwm(g_ship_rt.ud);
#else
    LOGI(SHIP_TAG, "pwm disabled by SHIP_THROTTLE_PWM_ENABLE=0");
#endif
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
        break;
    }

    ShipProtocol_SendGpsOnce();
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
    g_ship_rt.last_proto_rx_ms = Task_GetTickMs();
    g_ship_rt.rx_idle_warned = 0U;
    ShipProtocol_LogPayloadBrief("rx frame ok", cmd, &frame[3], data_len);
    ShipProtocol_Dispatch(cmd, &frame[3], data_len);
    return SUCCESS;
}

static void ShipProtocol_ReceiveHandle(const u8 *rx_buf, u8 len)
{
    u8 i;
    u8 frame[SHIP_LEGACY_PROTO_MAX_LEN];
    u8 frame_index;
    u8 frame_left;
    u8 frame_finish;
    u8 check_ok;
    u8 check_sum;

    if (rx_buf == 0) {
        return;
    }

    if (len > SHIP_LEGACY_PROTO_MAX_LEN) {
        len = 10U;
    }

    frame_index = 0U;
    frame_left = 0U;
    frame_finish = 0U;
    for (i = 0U; i < len; i++) {
        check_ok = 1U;
        switch (frame_index) {
        case 0U:
            if (rx_buf[i] != SHIP_PROTO_HEAD) {
                frame_index = 0U;
                frame_left = 0U;
                check_ok = 0U;
            }
            break;
        case 1U:
            frame_left = (u8)(rx_buf[i] + 1U);
            if (frame_left > (SHIP_LEGACY_PROTO_MAX_LEN - 2U)) {
                frame_index = 0U;
                frame_left = 0U;
                check_ok = 0U;
            }
            break;
        default:
            if (frame_left > 0U) {
                frame_left--;
                if (frame_left == 0U) {
                    frame_finish = 1U;
                }
            }
            break;
        }

        if (check_ok == 0U) {
            continue;
        }

        if (frame_index < SHIP_LEGACY_PROTO_MAX_LEN) {
            frame[frame_index++] = rx_buf[i];
        } else {
            frame_index = 0U;
            frame_left = 0U;
            frame_finish = 0U;
            continue;
        }

        if (frame_finish != 0U) {
            frame_finish = 0U;
            check_sum = ShipProtocol_Xor(&frame[1], (u8)(frame_index - 3U));
            if ((check_sum == frame[frame_index - 2U]) &&
                (frame[frame_index - 1U] == SHIP_PROTO_TAIL)) {
                (void)ShipProtocol_ParseFrame(frame, frame_index);
            }
            frame_index = 0U;
            frame_left = 0U;
        }
    }
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
            ShipProtocol_ReceiveHandle(frame, frame_len);
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
            ShipProtocol_ReceiveHandle(frame, frame_len);
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
    g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
    g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
    g_ship_rt.pair_retry_count = 0U;
    g_ship_rt.work_rx_reopen_ticks = 0U;
    g_ship_rt.work_rx_reopen_total = 0U;
    g_ship_rt.pair_wait_start_ms = 0UL;
    g_ship_rt.last_proto_rx_ms = 0UL;
    g_ship_rt.last_throttle_rx_ms = 0UL;
    g_ship_rt.pair_rsp_timeout_logged = 0U;
    g_ship_rt.rx_idle_warned = 0U;
    g_ship_rt.throttle_online = 0U;
#if SHIP_THROTTLE_PWM_ENABLE
    g_ship_rt.motor_initialized = 0U;
#endif

    LOGI(SHIP_TAG,
         "scheduler init rev=%s wait=%u pair_send=%u pair_ch=0x%02X seed=%02X%02X%02X%02X",
         SHIP_PAIR_FIX_REV,
         (u16)g_ship_rt.wait_ticks,
         (u16)g_ship_rt.pair_left,
         (u16)SHIP_PAIR_CHANNEL_DEFAULT,
         (u16)seed[0], (u16)seed[1], (u16)seed[2], (u16)seed[3]);
}

static void ShipProtocol_StepPairSend(void)
{
    s8 rc;
    u16 left_after_send;

    if (g_ship_rt.pair_left > 0U) {
        g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
        left_after_send = (u16)(g_ship_rt.pair_left - 1U);
        rc = ShipProtocol_TryPairSend(left_after_send);
        if (rc != SUCCESS) {
            return;
        }
        g_ship_rt.pair_left = left_after_send;

        if (g_ship_rt.pair_left == 0U) {
            rc = ShipProtocol_ApplyWorkSyncIdle(1U);
            if (rc != SUCCESS) {
                LOGE(SHIP_TAG, "pair sync idle fail rc=%d", rc);
                g_ship_rt.pair_retry_count++;
                g_ship_rt.pair_left = SHIP_PAIR_SEND_TIMES;
                g_ship_rt.wait_ticks = SHIP_WAIT_TICKS_DEFAULT;
                return;
            }
            g_ship_rt.pair_wait_rsp_time = SHIP_PAIR_WAIT_RSP_TICKS;
            g_ship_rt.pair_wait_start_ms = Task_GetTickMs();
            g_ship_rt.last_proto_rx_ms = g_ship_rt.pair_wait_start_ms;
            g_ship_rt.pair_rsp_timeout_logged = 0U;
            g_ship_rt.state = SHIP_STATE_WORK_RX;
            LOGI(SHIP_TAG, "pair sync idle done, wait %u ticks then work-rx",
                 (u16)g_ship_rt.wait_ticks);
        }
    }
}

static void ShipProtocol_StepWorkRx(void)
{
    s8 rc;

    if (g_ship_rt.work_rx_configured == 0U) {
        rc = ShipProtocol_ApplyWorkRx(1U);
        if (rc != SUCCESS) {
            LOGE(SHIP_TAG, "enter work rx fail rc=%d", rc);
            return;
        }
        if (g_ship_rt.last_proto_rx_ms == 0UL) {
            g_ship_rt.last_proto_rx_ms = Task_GetTickMs();
        }
    }

    if (g_ship_rt.work_state_logged == 0U) {
        g_ship_rt.work_state_logged = 1U;
        LOGI(SHIP_TAG, "enter work-state rx_ch=%u tx_ch=%u",
             (u16)g_ship_rt.rf_channel[0],
             (u16)g_ship_rt.rf_channel[0]);
    } else if (g_ship_rt.work_rx_configured == 0U) {
        rc = ShipProtocol_ApplyWorkRx(1U);
        if (rc != SUCCESS) {
            LOGE(SHIP_TAG, "restore work rx fail rc=%d", rc);
        }
    } else {
        g_ship_rt.work_rx_reopen_ticks++;
        if (g_ship_rt.work_rx_reopen_ticks > SHIP_WORK_RX_REOPEN_TICKS) {
            rc = ShipProtocol_ApplyWorkRx(0U);
            if (rc != SUCCESS) {
                LOGE(SHIP_TAG, "periodic work rx reopen fail rc=%d", rc);
            } else {
                g_ship_rt.work_rx_reopen_total++;
                if ((g_ship_rt.work_rx_reopen_total <= 3U) ||
                    ((g_ship_rt.work_rx_reopen_total % 50U) == 0U)) {
                    LOGI(SHIP_TAG, "work-rx reopen cnt=%u ch=%u",
                         (u16)g_ship_rt.work_rx_reopen_total,
                         (u16)g_ship_rt.rf_channel[0]);
                }
            }
        }
    }
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

    if (g_ship_rt.pair_wait_rsp_time > 0U) {
        g_ship_rt.pair_wait_rsp_time--;
    } else if ((g_ship_rt.pair_wait_start_ms != 0UL) &&
               (g_ship_rt.pair_rsp_timeout_logged == 0U) &&
               ((now_ms - g_ship_rt.pair_wait_start_ms) >= SHIP_PAIR_RSP_EXPIRE_LOG_MS) &&
               (g_ship_rt.paired == 0U)) {
        g_ship_rt.pair_rsp_timeout_logged = 1U;
        LOGW(SHIP_TAG, "pair rsp window expired, no rsp");
        ShipProtocol_LogRxDebug("pair-rsp-expired");
    }

    if (g_ship_rt.wait_ticks > 0U) {
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
    case SHIP_STATE_WORK_RX:
        ShipProtocol_StepWorkRx();
        break;
    default:
        g_ship_rt.state = SHIP_STATE_PAIR_SEND;
        break;
    }

    now_ms = Task_GetTickMs();

    if (g_ship_rt.state == SHIP_STATE_WORK_RX) {
        if ((g_ship_rt.last_proto_rx_ms != 0UL) &&
            (g_ship_rt.rx_idle_warned == 0U) &&
            ((now_ms - g_ship_rt.last_proto_rx_ms) >= SHIP_RX_IDLE_WARN_MS)) {
            g_ship_rt.rx_idle_warned = 1U;
            LOGW(SHIP_TAG, "work-rx idle %lums, no valid frame", (u32)(now_ms - g_ship_rt.last_proto_rx_ms));
            ShipProtocol_LogRxDebug("rx-idle");
        }

        if ((g_ship_rt.throttle_online != 0U) &&
            (g_ship_rt.last_throttle_rx_ms != 0UL) &&
            ((now_ms - g_ship_rt.last_throttle_rx_ms) >= SHIP_THROTTLE_TIMEOUT_MS)) {
            g_ship_rt.throttle_online = 0U;
            LOGW(SHIP_TAG, "remote link timeout by cmd=0x11, dt=%lums",
                 (u32)(now_ms - g_ship_rt.last_throttle_rx_ms));
            ShipProtocol_LogRxDebug("throttle-timeout");
        }
    }
}

u8 ShipProtocol_IsPaired(void)
{
    return g_ship_rt.paired;
}
