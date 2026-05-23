#include "autodrive_cfg.h"

static AutoDrive_ReturnConfig_t g_autodrive_ram_cfg;

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
    AutoDriveCfg_Default(&g_autodrive_ram_cfg);
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
    if (cfg == 0) {
        return 0U;
    }

    g_autodrive_ram_cfg = *cfg;
    return 1U;
}
