/**
 * @file    autodrive_cfg.c
 * @brief   AutoDrive return-home origin EEPROM storage.
 *
 * @details
 * This module persists only the return-home/origin point. Runtime switch state
 * is kept in RAM, and 0x14 goto/fish points are never stored here.
 */
#include "autodrive_cfg.h"

#include "..\..\..\Driver\inc\STC32G_EEPROM.h"

#define AUTODRIVE_CFG_EEPROM_ADDR      0x000600UL
#define AUTODRIVE_CFG_RECORD_MAGIC     0x41445256UL /* "ADRV" */
#define AUTODRIVE_CFG_RECORD_VERSION   1U
#define AUTODRIVE_CFG_RECORD_SIZE      18U

typedef struct
{
    u32 magic;
    u8 version;
    AutoDrive_PointRaw_t ret_point;
    u16 checksum;
} AutoDriveCfg_Record_t;

static AutoDrive_ReturnConfig_t g_autodrive_ram_cfg;

static void AutoDriveCfg_WriteLe16(u8 *buf, u8 offset, u16 value)
{
    buf[offset] = (u8)value;
    buf[(u8)(offset + 1U)] = (u8)(value >> 8);
}

static u16 AutoDriveCfg_ReadLe16(const u8 *buf, u8 offset)
{
    return (u16)(((u16)buf[(u8)(offset + 1U)] << 8) | buf[offset]);
}

static void AutoDriveCfg_WriteLe32(u8 *buf, u8 offset, u32 value)
{
    buf[offset] = (u8)value;
    buf[(u8)(offset + 1U)] = (u8)(value >> 8);
    buf[(u8)(offset + 2U)] = (u8)(value >> 16);
    buf[(u8)(offset + 3U)] = (u8)(value >> 24);
}

static u32 AutoDriveCfg_ReadLe32(const u8 *buf, u8 offset)
{
    return ((u32)buf[offset]) |
           ((u32)buf[(u8)(offset + 1U)] << 8) |
           ((u32)buf[(u8)(offset + 2U)] << 16) |
           ((u32)buf[(u8)(offset + 3U)] << 24);
}

static u8 AutoDriveCfg_PointValid(const AutoDrive_PointRaw_t *point)
{
    if (point == 0) {
        return 0U;
    }
    if ((point->lon_ew != 'E') && (point->lon_ew != 'W')) {
        return 0U;
    }
    if ((point->lat_ns != 'N') && (point->lat_ns != 'S')) {
        return 0U;
    }
    if ((point->lon_whole == 0U) || (point->lat_whole == 0U)) {
        return 0U;
    }
    return 1U;
}

static u8 AutoDriveCfg_PointEqual(const AutoDrive_PointRaw_t *lhs,
                                  const AutoDrive_PointRaw_t *rhs)
{
    if ((lhs == 0) || (rhs == 0)) {
        return 0U;
    }
    if ((lhs->lon_ew == rhs->lon_ew) &&
        (lhs->lon_whole == rhs->lon_whole) &&
        (lhs->lon_frac == rhs->lon_frac) &&
        (lhs->lat_ns == rhs->lat_ns) &&
        (lhs->lat_whole == rhs->lat_whole) &&
        (lhs->lat_frac == rhs->lat_frac)) {
        return 1U;
    }
    return 0U;
}

static void AutoDriveCfg_RecordToBytes(const AutoDriveCfg_Record_t *record,
                                       u8 *buf)
{
    AutoDriveCfg_WriteLe32(buf, 0U, record->magic);
    buf[4] = record->version;
    buf[5] = record->ret_point.lon_ew;
    AutoDriveCfg_WriteLe16(buf, 6U, record->ret_point.lon_whole);
    AutoDriveCfg_WriteLe16(buf, 8U, record->ret_point.lon_frac);
    buf[10] = record->ret_point.lat_ns;
    AutoDriveCfg_WriteLe16(buf, 11U, record->ret_point.lat_whole);
    AutoDriveCfg_WriteLe16(buf, 13U, record->ret_point.lat_frac);
    buf[15] = 0U;
    AutoDriveCfg_WriteLe16(buf, 16U, record->checksum);
}

static void AutoDriveCfg_BytesToRecord(const u8 *buf,
                                       AutoDriveCfg_Record_t *record)
{
    record->magic = AutoDriveCfg_ReadLe32(buf, 0U);
    record->version = buf[4];
    record->ret_point.lon_ew = buf[5];
    record->ret_point.lon_whole = AutoDriveCfg_ReadLe16(buf, 6U);
    record->ret_point.lon_frac = AutoDriveCfg_ReadLe16(buf, 8U);
    record->ret_point.lat_ns = buf[10];
    record->ret_point.lat_whole = AutoDriveCfg_ReadLe16(buf, 11U);
    record->ret_point.lat_frac = AutoDriveCfg_ReadLe16(buf, 13U);
    record->checksum = AutoDriveCfg_ReadLe16(buf, 16U);
}

static u16 AutoDriveCfg_RecordChecksum(const AutoDriveCfg_Record_t *record)
{
    u8 buf[AUTODRIVE_CFG_RECORD_SIZE];
    u8 i;
    u16 sum;

    AutoDriveCfg_RecordToBytes(record, buf);
    buf[16] = 0U;
    buf[17] = 0U;
    sum = 0U;
    for (i = 0U; i < AUTODRIVE_CFG_RECORD_SIZE; i++) {
        sum = (u16)(sum + (u16)buf[i]);
    }
    return (u16)(sum ^ 0x5AA5U);
}

static u8 AutoDriveCfg_RecordValid(const AutoDriveCfg_Record_t *record)
{
    if (record == 0) {
        return 0U;
    }
    if ((record->magic != AUTODRIVE_CFG_RECORD_MAGIC) ||
        (record->version != AUTODRIVE_CFG_RECORD_VERSION) ||
        (record->checksum != AutoDriveCfg_RecordChecksum(record))) {
        return 0U;
    }
    return AutoDriveCfg_PointValid(&record->ret_point);
}

static void AutoDriveCfg_Default(AutoDrive_ReturnConfig_t *cfg)
{
    if (cfg == 0) {
        return;
    }

    cfg->auto_ret_onoff = 0x30U;
    cfg->ret_point.lon_ew = 0U;
    cfg->ret_point.lon_whole = 0U;
    cfg->ret_point.lon_frac = 0U;
    cfg->ret_point.lat_ns = 0U;
    cfg->ret_point.lat_whole = 0U;
    cfg->ret_point.lat_frac = 0U;
}

void AutoDriveCfg_Init(void)
{
    u8 buf[AUTODRIVE_CFG_RECORD_SIZE];
    AutoDriveCfg_Record_t record;

    AutoDriveCfg_Default(&g_autodrive_ram_cfg);
    EEPROM_read_n(AUTODRIVE_CFG_EEPROM_ADDR, buf, AUTODRIVE_CFG_RECORD_SIZE);
    AutoDriveCfg_BytesToRecord(buf, &record);
    if (AutoDriveCfg_RecordValid(&record) != 0U) {
        g_autodrive_ram_cfg.ret_point = record.ret_point;
    }
}

void AutoDriveCfg_Load(AutoDrive_ReturnConfig_t *cfg)
{
    if (cfg == 0) {
        return;
    }

    *cfg = g_autodrive_ram_cfg;
}

u8 AutoDriveCfg_Save(const AutoDrive_ReturnConfig_t *cfg)
{
    u8 buf[AUTODRIVE_CFG_RECORD_SIZE];
    AutoDriveCfg_Record_t record;
    AutoDriveCfg_Record_t verify;

    if (cfg == 0) {
        return 0U;
    }

    g_autodrive_ram_cfg = *cfg;
    if (AutoDriveCfg_PointValid(&cfg->ret_point) == 0U) {
        return 0U;
    }

    record.magic = AUTODRIVE_CFG_RECORD_MAGIC;
    record.version = AUTODRIVE_CFG_RECORD_VERSION;
    record.ret_point = cfg->ret_point;
    record.checksum = 0U;
    record.checksum = AutoDriveCfg_RecordChecksum(&record);

    AutoDriveCfg_RecordToBytes(&record, buf);
    EEPROM_SectorErase(AUTODRIVE_CFG_EEPROM_ADDR);
    EEPROM_write_n(AUTODRIVE_CFG_EEPROM_ADDR, buf, AUTODRIVE_CFG_RECORD_SIZE);
    EEPROM_read_n(AUTODRIVE_CFG_EEPROM_ADDR, buf, AUTODRIVE_CFG_RECORD_SIZE);
    AutoDriveCfg_BytesToRecord(buf, &verify);
    if ((AutoDriveCfg_RecordValid(&verify) == 0U) ||
        (AutoDriveCfg_PointEqual(&verify.ret_point, &cfg->ret_point) == 0U)) {
        return 0U;
    }
    return 1U;
}
