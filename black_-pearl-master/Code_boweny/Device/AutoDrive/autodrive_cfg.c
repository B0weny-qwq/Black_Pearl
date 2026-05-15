#include "autodrive_cfg.h"

#include "..\..\..\Driver\inc\STC32G_EEPROM.h"
#include "..\..\Function\Log\Log.h"

#define AUTODRIVE_CFG_TAG        "ADRVCFG"
#define AUTODRIVE_CFG_MAGIC      0x41554432UL
#define AUTODRIVE_CFG_VERSION    0x0002U
#define AUTODRIVE_CFG_FLASH_ADDR 0x0001F800UL

typedef struct
{
    u32 magic;
    u16 version;
    u8 length;
    u8 checksum;
    AutoDrive_ReturnConfig_t cfg;
} AutoDrive_ConfigImage_t;

static u8 AutoDriveCfg_Checksum(const u8 *buf, u8 len)
{
    u8 i;
    u8 sum;

    sum = 0U;
    for (i = 0U; i < len; i++) {
        sum = (u8)(sum + buf[i]);
    }
    return sum;
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
    DisableEEPROM();
}

void AutoDriveCfg_Load(AutoDrive_ReturnConfig_t *cfg)
{
    AutoDrive_ConfigImage_t image;
    u8 *raw;
    u8 expect;
    u16 i;

    if (cfg == 0) {
        return;
    }

    raw = (u8 *)&image;
    for (i = 0U; i < (u16)sizeof(image); i++) {
        raw[i] = 0U;
    }
    EEPROM_read_n(AUTODRIVE_CFG_FLASH_ADDR, raw, (u16)sizeof(image));

    if ((image.magic != AUTODRIVE_CFG_MAGIC) ||
        (image.version != AUTODRIVE_CFG_VERSION) ||
        (image.length != (u8)sizeof(AutoDrive_ReturnConfig_t))) {
        AutoDriveCfg_Default(cfg);
        LOGW(AUTODRIVE_CFG_TAG, "cfg empty use default");
        return;
    }

    expect = AutoDriveCfg_Checksum((const u8 *)&image.cfg, image.length);
    if (expect != image.checksum) {
        AutoDriveCfg_Default(cfg);
        LOGW(AUTODRIVE_CFG_TAG, "cfg checksum mismatch");
        return;
    }

    *cfg = image.cfg;
    if (cfg->auto_ret_onoff == 0xFFU) {
        AutoDriveCfg_Default(cfg);
    }
}

u8 AutoDriveCfg_Save(const AutoDrive_ReturnConfig_t *cfg)
{
    AutoDrive_ConfigImage_t image;
    u8 *raw;
    u16 i;

    if (cfg == 0) {
        return 0U;
    }

    raw = (u8 *)&image;
    for (i = 0U; i < (u16)sizeof(image); i++) {
        raw[i] = 0U;
    }

    image.magic = AUTODRIVE_CFG_MAGIC;
    image.version = AUTODRIVE_CFG_VERSION;
    image.length = (u8)sizeof(AutoDrive_ReturnConfig_t);
    image.cfg = *cfg;
    image.checksum = AutoDriveCfg_Checksum((const u8 *)&image.cfg, image.length);

    EEPROM_SectorErase(AUTODRIVE_CFG_FLASH_ADDR);
    EEPROM_write_n(AUTODRIVE_CFG_FLASH_ADDR, raw, (u16)sizeof(image));
    return 1U;
}
