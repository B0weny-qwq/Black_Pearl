#ifndef __AUTODRIVE_H__
#define __AUTODRIVE_H__

#include "config.h"

typedef enum
{
    AUTO_DRIVE_IDLE = 0,
    AUTO_DRIVE_START,
    AUTO_DRIVE_GET_DIRECTION,
    AUTO_DRIVE_RUNING
} AutoDrive_State_t;

typedef enum
{
    AUTO_DRIVE_CLOSE = 0,
    AUTO_DRIVE_GO_FISISH_POSITION,
    AUTO_DRIVE_GO_HOME_POSITION
} AutoDrive_Mode_t;

typedef enum
{
    POSITION_NORTH = 1,
    POSITION_EAST_NORTH = 2,
    POSITION_EAST = 3,
    POSITION_EAST_SOUTH = 4,
    POSITION_SOUTH = 5,
    POSITION_WEST_SOUTH = 6,
    POSITION_WEST = 7,
    POSITION_WEST_NORTH = 8
} AutoDrive_Direction_t;

typedef struct
{
    u8 lon_ew;
    u16 lon_whole;
    u16 lon_frac;
    u8 lat_ns;
    u16 lat_whole;
    u16 lat_frac;
} AutoDrive_PointRaw_t;

#define AUTODRIVE_LEGACY_POINT_WIRE_LEN 10U

typedef struct
{
    u8 auto_ret_onoff;
    AutoDrive_PointRaw_t ret_point;
} AutoDrive_ReturnConfig_t;

void AutoDrive_Init(void);
void AutoDrive_Poll(void);
void AutoDrive_Stop(void);
void AutoDrive_StopMotion(void);
void AutoDrive_TriggerReturn(void);
void AutoDrive_WorkOvertimeFail(void);

void AutoDrive_SetMode(u8 mode);
u8 AutoDrive_GetMode(void);
u8 AutoDrive_InActive(void);
u8 AutoDrive_IsBusy(void);
u8 AutoDrive_IsCanActive(const AutoDrive_PointRaw_t *point);

void AutoDrive_SetReturnPositionRaw(const u8 *data_m);
void AutoDrive_SetFishPositionRaw(const u8 *data_m);
void AutoDrive_SetSwitchRaw(const u8 *data_m, u8 len);
void AutoDrive_GetStoredConfig(AutoDrive_ReturnConfig_t *cfg);
void AutoDrive_GetCurrentPointRaw(AutoDrive_PointRaw_t *point);

u16 AutoDrive_GetDistanceNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData);
u16 AutoDrive_GetAngelNowToDestination(const u8 *nowpositionData,
                                       const u8 *despositionData);
u16 AutoDrive_GetNorthAngel(u8 direction, u8 angel);
u8 AutoDrive_GetDirectionNowToDestination(const u8 *nowpositionData,
                                          const u8 *despositionData);

void AutoDrive_LinkAliveTick(void);
void AutoDrive_LinkAliveKick(void);

#endif
