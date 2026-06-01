/**
 * @file    autodrive_cfg.c
 * @brief   AutoDrive 返航配置的轻量级 RAM 存储实现。
 *
 * @details
 * 当前版本使用 RAM 中的结构体缓存模拟配置持久层，供 AutoDrive 读取和保存
 * 自动返航开关与返航点参数。
 *
 * @note 当前职责边界：
 * - 本文件只负责配置载入、默认值和保存接口。
 * - 真正的自动导航状态机不在本文件内实现。
 * - 若后续接入 EEPROM/Flash，可优先在本文件扩展而不破坏上层接口。
 */
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
