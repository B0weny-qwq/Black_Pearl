#include "..\..\..\User\Config.h"
#include "..\..\..\User\Task.h"
#include "..\..\..\Driver\inc\STC32G_I2C.h"
#include "QMI8658.h"
#include "QMI8658_port.h"
#include "Filter.h"

#if !QMI8658_DIAG_ENABLE
#undef LOGD
#define LOGD(tag, ...)
#endif

typedef struct
{
    QMI8658_State_t state;
    u8 data_ready;
    u8 init_retry;
    u8 selected_id;
    u8 last_status0;
    u8 last_statusint;
    u32 due_ms;
    u32 ready_deadline_ms;
} QMI8658_Context_t;

u8 QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;

static QMI8658_Context_t g_qmi8658_ctx = { QMI8658_STATE_IDLE, 0U, 0U, 0xFFU, 0U, 0U, 0UL, 0UL };
static u8 qmi8658_last_i2c_error = QMI8658_I2C_OK;

static u8 QMI8658_ReadReg(u8 reg_addr);
static u8 QMI8658_ReadRegAtAddr(u8 addr, u8 reg_addr, u8 *ok);
static u8 QMI8658_ReadNByte(u8 start_reg, u8 *buf, u8 len);
static u8 QMI8658_ReadNByteAtAddr(u8 addr, u8 start_reg, u8 *buf, u8 len);
static u8 QMI8658_WriteReg(u8 reg_addr, u8 reg_val);
static void QMI8658_SetState(QMI8658_State_t state, u16 delay_ms);
static u8 QMI8658_IsDue(u32 now_ms);
static char *QMI8658_StateName(QMI8658_State_t state);
static char *QMI8658_I2cErrName(u8 err);
static u8 QMI8658_SelectAddrByWhoAmI(u8 *selected_id);
static u8 QMI8658_ProbeAddr(u8 addr);
static u8 QMI8658_CheckReadyFlag(void);
static s8 QMI8658_ClearDataPath(void);
static u8 QMI8658_ConfigReadbackOk(void);
static void QMI8658_LogDataPath(char *phase);
static u8 QMI8658_LogDataWindow(void);
static s8 QMI8658_EnterRetryOrFail(char *reason);

static u8 QMI8658_ReadReg(u8 reg_addr)
{
    u8 value;

    value = 0xFFU;
    if (QMI8658_ReadNByte(reg_addr, &value, 1U) != 0U) {
        LOGE("IMU", "rd fail err=%s addr=0x%02X reg=0x%02X",
             QMI8658_I2cErrName(qmi8658_last_i2c_error),
             QMI8658_I2C_Addr,
             reg_addr);
        return 0xFFU;
    }
    return value;
}

static u8 QMI8658_ReadRegAtAddr(u8 addr, u8 reg_addr, u8 *ok)
{
    u8 value;

    value = 0xFFU;
    if (QMI8658_ReadNByteAtAddr(addr, reg_addr, &value, 1U) != 0U) {
        if (ok != NULL) {
            *ok = 0U;
        }
        return 0xFFU;
    }
    if (ok != NULL) {
        *ok = 1U;
    }
    return value;
}

static u8 QMI8658_ReadNByte(u8 start_reg, u8 *buf, u8 len)
{
    return QMI8658_ReadNByteAtAddr(QMI8658_I2C_Addr, start_reg, buf, len);
}

static u8 QMI8658_ReadNByteAtAddr(u8 addr, u8 start_reg, u8 *buf, u8 len)
{
    if (QMI8658Port_ReadN(addr, start_reg, buf, len, &qmi8658_last_i2c_error) != 0U) {
        return 1U;
    }
    qmi8658_last_i2c_error = QMI8658_I2C_OK;
    return 0U;
}

static u8 QMI8658_WriteReg(u8 reg_addr, u8 reg_val)
{
    if (QMI8658Port_WriteReg(QMI8658_I2C_Addr, reg_addr, reg_val, &qmi8658_last_i2c_error) != 0U) {
        return 1U;
    }
    qmi8658_last_i2c_error = QMI8658_I2C_OK;
    return 0U;
}

static void QMI8658_SetState(QMI8658_State_t state, u16 delay_ms)
{
    g_qmi8658_ctx.state = state;
    g_qmi8658_ctx.due_ms = Task_GetTickMs() + (u32)delay_ms;
    LOGD("IMU", "state -> %s delay=%u", QMI8658_StateName(state), (u16)delay_ms);
}

static u8 QMI8658_IsDue(u32 now_ms)
{
    return ((int32)(now_ms - g_qmi8658_ctx.due_ms) >= 0) ? 1U : 0U;
}

static char *QMI8658_StateName(QMI8658_State_t state)
{
    switch (state) {
    case QMI8658_STATE_IDLE:
        return "IDLE";
    case QMI8658_STATE_BUS_PREPARE:
        return "BUS_PREPARE";
    case QMI8658_STATE_PWR_WAIT:
        return "PWR_WAIT";
    case QMI8658_STATE_ID_PROBE:
        return "ID_PROBE";
    case QMI8658_STATE_QUIESCE:
        return "QUIESCE";
    case QMI8658_STATE_SOFT_RESET:
        return "SOFT_RESET";
    case QMI8658_STATE_RESET_WAIT:
        return "RESET_WAIT";
    case QMI8658_STATE_CLEAR_PATH:
        return "CLEAR_PATH";
    case QMI8658_STATE_CONFIG_WRITE:
        return "CONFIG_WRITE";
    case QMI8658_STATE_CONFIG_VERIFY:
        return "CONFIG_VERIFY";
    case QMI8658_STATE_ENABLE:
        return "ENABLE";
    case QMI8658_STATE_READY_WAIT:
        return "READY_WAIT";
    case QMI8658_STATE_READY:
        return "READY";
    default:
        return "FAILED";
    }
}

static char *QMI8658_I2cErrName(u8 err)
{
    switch (err) {
    case QMI8658_I2C_OK:
        return "OK";
    case QMI8658_I2C_ERR_BUSY:
        return "BUSY";
    case QMI8658_I2C_ERR_DEVW_NACK:
        return "DEVW_NACK";
    case QMI8658_I2C_ERR_REG_NACK:
        return "REG_NACK";
    case QMI8658_I2C_ERR_DEVR_NACK:
        return "DEVR_NACK";
    case QMI8658_I2C_ERR_DATA_NACK:
        return "DATA_NACK";
    default:
        return "PARAM";
    }
}

static u8 QMI8658_SelectAddrByWhoAmI(u8 *selected_id)
{
    u8 ok;
    u8 id;

    ok = 0U;
    id = QMI8658_ReadRegAtAddr(QMI8658_I2C_ADDR_PRIMARY, QMI8658_REG_WHO_AM_I, &ok);
    if ((ok != 0U) && (id == QMI8658_CHIP_ID_VALUE)) {
        QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;
        if (selected_id != NULL) {
            *selected_id = id;
        }
        LOGI("IMU", "id probe primary ok id=0x%02X addr=0x%02X", id, QMI8658_I2C_Addr);
        return 0U;
    }

    ok = 0U;
    id = QMI8658_ReadRegAtAddr(QMI8658_I2C_ADDR_ALT, QMI8658_REG_WHO_AM_I, &ok);
    if ((ok != 0U) && (id == QMI8658_CHIP_ID_VALUE)) {
        QMI8658_I2C_Addr = QMI8658_I2C_ADDR_ALT;
        if (selected_id != NULL) {
            *selected_id = id;
        }
        LOGI("IMU", "id probe alt ok id=0x%02X addr=0x%02X", id, QMI8658_I2C_Addr);
        return 0U;
    }

    LOGW("IMU", "id probe fail err=%s", QMI8658_I2cErrName(qmi8658_last_i2c_error));
    return 1U;
}

static u8 QMI8658_ProbeAddr(u8 addr)
{
    u8 dummy;

    dummy = QMI8658_REG_WHO_AM_I;
    qmi8658_last_i2c_error = QMI8658_I2C_OK;
    I2C_WriteNbyte(QMI8658_I2C_WRITE(addr), QMI8658_REG_WHO_AM_I, &dummy, 0U);
    if (Get_MSBusy_Status() != 0U) {
        qmi8658_last_i2c_error = QMI8658_I2C_ERR_BUSY;
        return 1U;
    }
    return 0U;
}

static u8 QMI8658_CheckReadyFlag(void)
{
    u8 status0;
#if QMI8658_READY_MODE_STATUSINT
    u8 statusint;
#endif

    status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);
    if (qmi8658_last_i2c_error != QMI8658_I2C_OK) {
        return 0U;
    }
    g_qmi8658_ctx.last_status0 = status0;

#if QMI8658_READY_MODE_STATUSINT
    statusint = QMI8658_ReadReg(QMI8658_REG_STATUSINT);
    if (qmi8658_last_i2c_error != QMI8658_I2C_OK) {
        return 0U;
    }
    g_qmi8658_ctx.last_statusint = statusint;
    return ((statusint & QMI8658_STATUSINT_AVAIL) != 0U) ? 1U : 0U;
#else
    g_qmi8658_ctx.last_statusint = 0U;
#if !QMI8658_INIT_NONBLOCKING
    return ((status0 & QMI8658_STATUS0_A_DA) != 0U) ? 1U : 0U;
#else
    return (((status0 & (QMI8658_STATUS0_A_DA | QMI8658_STATUS0_G_DA)) ==
             (QMI8658_STATUS0_A_DA | QMI8658_STATUS0_G_DA)) ? 1U : 0U);
#endif
#endif
}

static s8 QMI8658_ClearDataPath(void)
{
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00U) != 0U) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_CTRL6, QMI8658_CTRL6_INIT) != 0U) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_CTRL8, QMI8658_CTRL8_INIT) != 0U) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_FIFO_WTM, QMI8658_FIFO_WTM_INIT) != 0U) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_FIFO_CTRL, QMI8658_FIFO_CTRL_BYPASS) != 0U) {
        return -1;
    }
    if (QMI8658_WriteReg(QMI8658_REG_CTRL9, QMI8658_CTRL9_CMD_RST_FIFO) != 0U) {
        return -1;
    }
    QMI8658Port_DelayMs(2U);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL9, QMI8658_CTRL9_CMD_ACK) != 0U) {
        return -1;
    }
    return 0;
}

static u8 QMI8658_ConfigReadbackOk(void)
{
    u8 ctrl1;
    u8 ctrl2;
    u8 ctrl3;
    u8 ctrl5;
    u8 ctrl7;

    ctrl1 = QMI8658_ReadReg(QMI8658_REG_CTRL1);
    ctrl2 = QMI8658_ReadReg(QMI8658_REG_CTRL2);
    ctrl3 = QMI8658_ReadReg(QMI8658_REG_CTRL3);
    ctrl5 = QMI8658_ReadReg(QMI8658_REG_CTRL5);
    ctrl7 = QMI8658_ReadReg(QMI8658_REG_CTRL7);
    LOGI("IMU", "cfg readback c1=%02X c2=%02X c3=%02X c5=%02X c7=%02X",
         ctrl1, ctrl2, ctrl3, ctrl5, ctrl7);

    if (qmi8658_last_i2c_error != QMI8658_I2C_OK) {
        return 0U;
    }
    if (ctrl1 != QMI8658_CTRL1_INIT) {
        return 0U;
    }
    if (ctrl2 != QMI8658_CTRL2_INIT) {
        return 0U;
    }
    if (ctrl3 != QMI8658_CTRL3_INIT) {
        return 0U;
    }
    if (ctrl5 != QMI8658_CTRL5_INIT) {
        return 0U;
    }
    if (ctrl7 != QMI8658_CTRL7_INIT) {
        return 0U;
    }
    return 1U;
}

static void QMI8658_LogDataPath(char *phase)
{
    u8 ctrl1;
    u8 ctrl2;
    u8 ctrl3;
    u8 ctrl5;
    u8 ctrl7;
    u8 status0;
    u8 statusint;
    u8 ts_raw[3];
    u8 temp_raw[2];
    u8 raw[12];
    u32 ts;
    int16 temp;
    int16 ax;
    int16 ay;
    int16 az;
    int16 gx;
    int16 gy;
    int16 gz;

    ctrl1 = QMI8658_ReadReg(QMI8658_REG_CTRL1);
    ctrl2 = QMI8658_ReadReg(QMI8658_REG_CTRL2);
    ctrl3 = QMI8658_ReadReg(QMI8658_REG_CTRL3);
    ctrl5 = QMI8658_ReadReg(QMI8658_REG_CTRL5);
    ctrl7 = QMI8658_ReadReg(QMI8658_REG_CTRL7);
    statusint = QMI8658_ReadReg(QMI8658_REG_STATUSINT);
    status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);
    LOGI("IMU", "%s regs c1=%02X c2=%02X c3=%02X c5=%02X c7=%02X",
         phase, ctrl1, ctrl2, ctrl3, ctrl5, ctrl7);
    LOGI("IMU", "%s status int=%02X s0=%02X", phase, statusint, status0);

    if (QMI8658_ReadNByte(QMI8658_REG_TIMESTAMP_L, ts_raw, 3U) == 0U) {
        ts = ((u32)ts_raw[2] << 16) | ((u32)ts_raw[1] << 8) | ts_raw[0];
        LOGI("IMU", "%s ts=%lu", phase, ts);
    }
    if (QMI8658_ReadNByte(QMI8658_REG_TEMP_L, temp_raw, 2U) == 0U) {
        temp = (int16)((u16)temp_raw[1] << 8 | temp_raw[0]);
        LOGI("IMU", "%s temp=%d", phase, temp);
    }
    if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 12U) == 0U) {
        ax = (int16)((u16)raw[1] << 8 | raw[0]);
        ay = (int16)((u16)raw[3] << 8 | raw[2]);
        az = (int16)((u16)raw[5] << 8 | raw[4]);
        gx = (int16)((u16)raw[7] << 8 | raw[6]);
        gy = (int16)((u16)raw[9] << 8 | raw[8]);
        gz = (int16)((u16)raw[11] << 8 | raw[10]);
        LOGI("IMU", "%s raw a=%d %d %d g=%d %d %d", phase, ax, ay, az, gx, gy, gz);
    }
}

static u8 QMI8658_LogDataWindow(void)
{
    u8 i;
    u8 status0;
    u8 statusint;
    u8 ts_raw[3];
    u8 temp_raw[2];
    u8 raw[12];
    u32 ts;
    int16 temp;
    int16 ax;
    int16 ay;
    int16 az;
    int16 gx;
    int16 gy;
    int16 gz;
    u8 saw_data;

    saw_data = 0U;
    LOGI("IMU", "data window start");

    for (i = 0U; i < 10U; i++) {
        statusint = QMI8658_ReadReg(QMI8658_REG_STATUSINT);
        status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);

        ts = 0UL;
        if (QMI8658_ReadNByte(QMI8658_REG_TIMESTAMP_L, ts_raw, 3U) == 0U) {
            ts = ((u32)ts_raw[2] << 16) | ((u32)ts_raw[1] << 8) | ts_raw[0];
        }

        temp = 0;
        if (QMI8658_ReadNByte(QMI8658_REG_TEMP_L, temp_raw, 2U) == 0U) {
            temp = (int16)((u16)temp_raw[1] << 8 | temp_raw[0]);
        }

        ax = 0;
        ay = 0;
        az = 0;
        gx = 0;
        gy = 0;
        gz = 0;
        if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 12U) == 0U) {
            ax = (int16)((u16)raw[1] << 8 | raw[0]);
            ay = (int16)((u16)raw[3] << 8 | raw[2]);
            az = (int16)((u16)raw[5] << 8 | raw[4]);
            gx = (int16)((u16)raw[7] << 8 | raw[6]);
            gy = (int16)((u16)raw[9] << 8 | raw[8]);
            gz = (int16)((u16)raw[11] << 8 | raw[10]);
        }

        LOGI("IMU", "poll%u si=%02X s0=%02X ts=%lu t=%d a=%d %d %d",
             (u16)i, statusint, status0, ts, temp, ax, ay, az);
        LOGI("IMU", "poll%u g=%d %d %d", (u16)i, gx, gy, gz);

        if ((status0 != 0U) || (ts != 0UL) || (temp != 0) ||
            (ax != 0) || (ay != 0) || (az != 0) ||
            (gx != 0) || (gy != 0) || (gz != 0)) {
            saw_data = 1U;
            break;
        }
        QMI8658Port_DelayMs(100U);
    }

    LOGI("IMU", "data window result=%s", (saw_data != 0U) ? "DATA" : "ALL_ZERO");
    return saw_data;
}

static s8 QMI8658_EnterRetryOrFail(char *reason)
{
    if (g_qmi8658_ctx.init_retry < QMI8658_INIT_RETRY_MAX) {
        g_qmi8658_ctx.init_retry++;
        LOGW("IMU", "%s -> retry %u/%u", reason,
             (u16)g_qmi8658_ctx.init_retry,
             (u16)QMI8658_INIT_RETRY_MAX);
        QMI8658Port_BusRecover();
        QMI8658_SetState(QMI8658_STATE_BUS_PREPARE, QMI8658_INIT_RETRY_DELAY_MS);
        return 0;
    }

    LOGE("IMU", "%s -> FAILED err=%s", reason, QMI8658_I2cErrName(qmi8658_last_i2c_error));
    g_qmi8658_ctx.data_ready = 0U;
    QMI8658_SetState(QMI8658_STATE_FAILED, 0U);
    return -1;
}

s8 QMI8658_Init(void)
{
#if QMI8658_INIT_NONBLOCKING
    QMI8658_RequestReinit();
    return 0;
#else
    u8 id;
    u8 retry;
    u8 reset_state;
    u8 ctrl1_rb;
    u8 ctrl2_rb;
    u8 ctrl3_rb;
    u8 ctrl5_rb;
    u8 ctrl7_rb;
    u8 saw_data;

    id = 0xFFU;
    saw_data = 0U;
    g_qmi8658_ctx.data_ready = 0U;
    g_qmi8658_ctx.init_retry = 0U;
    g_qmi8658_ctx.selected_id = 0xFFU;
    g_qmi8658_ctx.last_status0 = 0U;
    g_qmi8658_ctx.last_statusint = 0U;
    g_qmi8658_ctx.state = QMI8658_STATE_IDLE;
    qmi8658_last_i2c_error = QMI8658_I2C_OK;

    LOGI("IMU", "========== QMI8658 Init Start ==========");
    LOGI("IMU", "legacy bring-up: soft_reset=%u diag=%u bus=%s",
         (u16)QMI8658_SOFT_RESET_ENABLE,
         (u16)QMI8658_DIAG_ENABLE,
         QMI8658Port_BackendName());
    LOGD("IMU", "primary addr=0x%02X alt=0x%02X",
         QMI8658_I2C_WRITE(QMI8658_I2C_ADDR_PRIMARY),
         QMI8658_I2C_WRITE(QMI8658_I2C_ADDR_ALT));

    if (QMI8658Port_Init() != SUCCESS) {
        LOGE("IMU", "port init fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    for (retry = 0U; retry < 3U; retry++) {
        QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;
        LOGD("IMU", "probe primary 0x%02X try=%u",
             QMI8658_I2C_WRITE(QMI8658_I2C_Addr), (u16)(retry + 1U));
        if (QMI8658_ProbeAddr(QMI8658_I2C_ADDR_PRIMARY) == 0U) {
            LOGD("IMU", "found device at primary addr=0x%02X",
                 QMI8658_I2C_WRITE(QMI8658_I2C_Addr));
            break;
        }

        LOGW("IMU", "primary addr no ACK, try alt");
        QMI8658_I2C_Addr = QMI8658_I2C_ADDR_ALT;
        LOGD("IMU", "probe alt 0x%02X", QMI8658_I2C_WRITE(QMI8658_I2C_Addr));
        if (QMI8658_ProbeAddr(QMI8658_I2C_ADDR_ALT) == 0U) {
            LOGD("IMU", "found device at alt addr=0x%02X",
                 QMI8658_I2C_WRITE(QMI8658_I2C_Addr));
            break;
        }

        QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;
        LOGW("IMU", "addr probe fail try=%u, wait 200ms retry", (u16)(retry + 1U));
        QMI8658Port_DelayMs(200U);
    }

    if (retry >= 3U) {
        LOGE("IMU", "no device found after 3 tries");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    LOGD("IMU", "power-up wait %ums", (u16)QMI8658_PWR_UP_DELAY_MS);
    QMI8658Port_DelayMs(QMI8658_PWR_UP_DELAY_MS);

    id = QMI8658_ReadID();
    if (id != QMI8658_CHIP_ID_VALUE) {
        LOGE("IMU", "WHO_AM_I error got=0x%02X exp=0x%02X", id, QMI8658_CHIP_ID_VALUE);
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }
    g_qmi8658_ctx.selected_id = id;
    LOGD("IMU", "WHO_AM_I=0x%02X OK addr=0x%02X", id, QMI8658_I2C_Addr);

    reset_state = QMI8658_ReadReg(QMI8658_REG_RESET_STATE);
    LOGI("IMU", "reset_state before=0x%02X", reset_state);

    if (QMI8658_SOFT_RESET_ENABLE != 0) {
        if (reset_state != QMI8658_RESET_STATE_READY) {
            if (QMI8658_WriteReg(QMI8658_REG_RESET, 0xB0U) != 0U) {
                LOGE("IMU", "soft reset WR fail");
                g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
                return -1;
            }
            QMI8658Port_DelayMs(QMI8658_RESET_DELAY_MS);
            reset_state = QMI8658_ReadReg(QMI8658_REG_RESET_STATE);
            LOGI("IMU", "reset_state after=0x%02X", reset_state);
        } else {
            LOGI("IMU", "reset_state ready, skip reset");
        }
    } else {
        LOGI("IMU", "soft reset disabled state=0x%02X", reset_state);
    }

    if (reset_state == 0xFFU) {
        LOGE("IMU", "reset_state read fail after reset");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    id = QMI8658_ReadID();
    if (id != QMI8658_CHIP_ID_VALUE) {
        LOGE("IMU", "WHO_AM_I lost after reset got=0x%02X", id);
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    if (QMI8658_CLEAR_DATAPATH_ENABLE != 0) {
        if (QMI8658_ClearDataPath() != 0) {
            LOGE("IMU", "clear data path fail");
            g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
            return -1;
        }
        QMI8658_LogDataPath("after_clear");
    } else {
        LOGI("IMU", "skip ctrl6/8/fifo/ctrl9 clear");
    }

    LOGD("IMU", "step1 CTRL7=0x00 (disable sensors)");
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00U) != 0U) {
        LOGE("IMU", "step1 WR fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    LOGD("IMU", "step2 CTRL1=0x%02X", QMI8658_CTRL1_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL1, QMI8658_CTRL1_INIT) != 0U) {
        LOGE("IMU", "step2 WR fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    LOGD("IMU", "step3 CTRL2=0x%02X (ACC range+ODR)", QMI8658_CTRL2_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL2, QMI8658_CTRL2_INIT) != 0U) {
        LOGE("IMU", "step3 WR fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    LOGD("IMU", "step4 CTRL3=0x%02X (GYRO range+ODR)", QMI8658_CTRL3_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL3, QMI8658_CTRL3_INIT) != 0U) {
        LOGE("IMU", "step4 WR fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    LOGD("IMU", "step5 CTRL5=0x%02X", QMI8658_CTRL5_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL5, QMI8658_CTRL5_INIT) != 0U) {
        LOGE("IMU", "step5 WR fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }

    LOGD("IMU", "step6 CTRL7=0x%02X (enable sensors)", QMI8658_CTRL7_INIT);
    if (QMI8658_WriteReg(QMI8658_REG_CTRL7, QMI8658_CTRL7_INIT) != 0U) {
        LOGE("IMU", "step6 WR fail");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        return -1;
    }
    QMI8658Port_DelayMs(QMI8658_ENABLE_DELAY_MS);

    ctrl1_rb = QMI8658_ReadReg(QMI8658_REG_CTRL1);
    ctrl2_rb = QMI8658_ReadReg(QMI8658_REG_CTRL2);
    ctrl3_rb = QMI8658_ReadReg(QMI8658_REG_CTRL3);
    ctrl5_rb = QMI8658_ReadReg(QMI8658_REG_CTRL5);
    ctrl7_rb = QMI8658_ReadReg(QMI8658_REG_CTRL7);
    LOGI("IMU", "readback CTRL1=0x%02X CTRL2=0x%02X CTRL3=0x%02X CTRL5=0x%02X CTRL7=0x%02X",
         ctrl1_rb, ctrl2_rb, ctrl3_rb, ctrl5_rb, ctrl7_rb);

    if (ctrl1_rb != QMI8658_CTRL1_INIT) {
        LOGW("IMU", "CTRL1 mismatch read=0x%02X exp=0x%02X", ctrl1_rb, QMI8658_CTRL1_INIT);
    }
    if (ctrl2_rb != QMI8658_CTRL2_INIT) {
        LOGW("IMU", "CTRL2 mismatch read=0x%02X exp=0x%02X", ctrl2_rb, QMI8658_CTRL2_INIT);
    }
    if (ctrl3_rb != QMI8658_CTRL3_INIT) {
        LOGW("IMU", "CTRL3 mismatch read=0x%02X exp=0x%02X", ctrl3_rb, QMI8658_CTRL3_INIT);
    }
    if (ctrl5_rb != QMI8658_CTRL5_INIT) {
        LOGW("IMU", "CTRL5 mismatch read=0x%02X exp=0x%02X", ctrl5_rb, QMI8658_CTRL5_INIT);
    }
    if (ctrl7_rb != QMI8658_CTRL7_INIT) {
        LOGW("IMU", "CTRL7 mismatch read=0x%02X exp=0x%02X", ctrl7_rb, QMI8658_CTRL7_INIT);
    }

#if QMI8658_DIAG_ENABLE
    QMI8658_LogDataPath("after_enable");
#endif

    LOGD("IMU", "wait sensor data ready...");
    if (QMI8658_Wait_AccReady(QMI8658_READY_TIMEOUT_MS) != 0) {
        LOGW("IMU", "acc not ready in %ums, continue legacy data window",
             (u16)QMI8658_READY_TIMEOUT_MS);
#if QMI8658_DIAG_ENABLE
        QMI8658_LogDataPath("not_ready");
        saw_data = QMI8658_LogDataWindow();
#endif
    } else {
        saw_data = 1U;
#if QMI8658_DIAG_ENABLE
        QMI8658_LogDataPath("ready");
#endif
    }

    Filter_ResetGyroLowPass();
    LOGI("IMU", "========== QMI8658 Init Done ==========");
    LOGI("IMU", "config: CTRL2=0x%02X CTRL3=0x%02X CTRL5=0x%02X CTRL7=0x%02X",
         QMI8658_CTRL2_INIT, QMI8658_CTRL3_INIT, QMI8658_CTRL5_INIT, QMI8658_CTRL7_INIT);

    if (saw_data == 0U) {
        LOGE("IMU", "legacy data window all zero");
        g_qmi8658_ctx.state = QMI8658_STATE_FAILED;
        g_qmi8658_ctx.data_ready = 0U;
        return -1;
    }

    g_qmi8658_ctx.last_status0 = QMI8658_ReadReg(QMI8658_REG_STATUS0);
    g_qmi8658_ctx.state = QMI8658_STATE_READY;
    g_qmi8658_ctx.data_ready = 1U;
    return 0;
#endif
}

s8 QMI8658_Service(void)
{
    u8 id;
    u8 reset_state;
    u32 now_ms;

    now_ms = Task_GetTickMs();

#if !QMI8658_INIT_NONBLOCKING
    if (g_qmi8658_ctx.state == QMI8658_STATE_READY) {
        (void)QMI8658_PollDataReady();
        return 0;
    }
    if (g_qmi8658_ctx.state == QMI8658_STATE_FAILED) {
        return -1;
    }
    return 0;
#endif

    if (g_qmi8658_ctx.state == QMI8658_STATE_IDLE) {
        return 0;
    }

    if (g_qmi8658_ctx.state == QMI8658_STATE_READY) {
        (void)QMI8658_PollDataReady();
        return 0;
    }

    if (g_qmi8658_ctx.state == QMI8658_STATE_FAILED) {
        return -1;
    }

    if (QMI8658_IsDue(now_ms) == 0U) {
        return 0;
    }

    switch (g_qmi8658_ctx.state) {
    case QMI8658_STATE_BUS_PREPARE:
        LOGI("IMU", "========== QMI8658 Init Start ==========");
        LOGI("IMU", "bring-up: soft_reset=%u diag=%u bus=%s",
             (u16)QMI8658_SOFT_RESET_ENABLE,
             (u16)QMI8658_DIAG_ENABLE,
             QMI8658Port_BackendName());
        if (QMI8658Port_Init() != SUCCESS) {
            return QMI8658_EnterRetryOrFail("port init fail");
        }
        if (QMI8658Port_BusNeedsRecover() != 0U) {
            QMI8658Port_BusRecover();
        }
        QMI8658_SetState(QMI8658_STATE_PWR_WAIT, QMI8658_PWR_UP_DELAY_MS);
        return 0;

    case QMI8658_STATE_PWR_WAIT:
        QMI8658_SetState(QMI8658_STATE_ID_PROBE, 0U);
        return 0;

    case QMI8658_STATE_ID_PROBE:
        if (QMI8658_SelectAddrByWhoAmI(&id) != 0U) {
            return QMI8658_EnterRetryOrFail("who_am_i fail");
        }
        g_qmi8658_ctx.selected_id = id;
        LOGI("IMU", "WHO_AM_I=0x%02X i2c_addr=0x%02X", id, QMI8658_I2C_Addr);
        QMI8658_SetState(QMI8658_STATE_QUIESCE, 0U);
        return 0;

    case QMI8658_STATE_QUIESCE:
        if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00U) != 0U) {
            return QMI8658_EnterRetryOrFail("quiesce fail");
        }
        if (QMI8658_CLEAR_DATAPATH_ENABLE != 0) {
            if (QMI8658_ClearDataPath() != 0) {
                return QMI8658_EnterRetryOrFail("pre-clear fail");
            }
        }
        QMI8658_SetState(QMI8658_STATE_SOFT_RESET, 0U);
        return 0;

    case QMI8658_STATE_SOFT_RESET:
        if (QMI8658_SOFT_RESET_ENABLE != 0) {
            if (QMI8658_WriteReg(QMI8658_REG_RESET, 0xB0U) != 0U) {
                return QMI8658_EnterRetryOrFail("soft reset fail");
            }
            QMI8658_SetState(QMI8658_STATE_RESET_WAIT, QMI8658_RESET_DELAY_MS);
            return 0;
        }
        QMI8658_SetState(QMI8658_STATE_CLEAR_PATH, 0U);
        return 0;

    case QMI8658_STATE_RESET_WAIT:
        reset_state = QMI8658_ReadReg(QMI8658_REG_RESET_STATE);
        LOGI("IMU", "reset_state=0x%02X", reset_state);
        if (qmi8658_last_i2c_error != QMI8658_I2C_OK) {
            return QMI8658_EnterRetryOrFail("reset_state read fail");
        }
        QMI8658_SetState(QMI8658_STATE_CLEAR_PATH, 0U);
        return 0;

    case QMI8658_STATE_CLEAR_PATH:
        if (QMI8658_CLEAR_DATAPATH_ENABLE != 0) {
            if (QMI8658_ClearDataPath() != 0) {
                return QMI8658_EnterRetryOrFail("clear path fail");
            }
        }
#if QMI8658_DIAG_ENABLE
        QMI8658_LogDataPath("after_clear");
#endif
        QMI8658_SetState(QMI8658_STATE_CONFIG_WRITE, 0U);
        return 0;

    case QMI8658_STATE_CONFIG_WRITE:
        if (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00U) != 0U) {
            return QMI8658_EnterRetryOrFail("cfg ctrl7 disable fail");
        }
        if (QMI8658_WriteReg(QMI8658_REG_CTRL1, QMI8658_CTRL1_INIT) != 0U) {
            return QMI8658_EnterRetryOrFail("cfg ctrl1 fail");
        }
        if (QMI8658_WriteReg(QMI8658_REG_CTRL2, QMI8658_CTRL2_INIT) != 0U) {
            return QMI8658_EnterRetryOrFail("cfg ctrl2 fail");
        }
        if (QMI8658_WriteReg(QMI8658_REG_CTRL3, QMI8658_CTRL3_INIT) != 0U) {
            return QMI8658_EnterRetryOrFail("cfg ctrl3 fail");
        }
        if (QMI8658_WriteReg(QMI8658_REG_CTRL5, QMI8658_CTRL5_INIT) != 0U) {
            return QMI8658_EnterRetryOrFail("cfg ctrl5 fail");
        }
        QMI8658_SetState(QMI8658_STATE_ENABLE, 0U);
        return 0;

    case QMI8658_STATE_ENABLE:
        if (QMI8658_WriteReg(QMI8658_REG_CTRL7, QMI8658_CTRL7_INIT) != 0U) {
            return QMI8658_EnterRetryOrFail("enable fail");
        }
#if QMI8658_DIAG_ENABLE
        QMI8658_LogDataPath("after_enable");
#endif
        g_qmi8658_ctx.ready_deadline_ms = now_ms + (u32)QMI8658_READY_TIMEOUT_MS;
        QMI8658_SetState(QMI8658_STATE_CONFIG_VERIFY, QMI8658_ENABLE_DELAY_MS);
        return 0;

    case QMI8658_STATE_CONFIG_VERIFY:
        if (QMI8658_ConfigReadbackOk() == 0U) {
            return QMI8658_EnterRetryOrFail("cfg verify fail");
        }
        QMI8658_SetState(QMI8658_STATE_READY_WAIT, 0U);
        return 0;

    case QMI8658_STATE_READY_WAIT:
        if (QMI8658_CheckReadyFlag() != 0U) {
            g_qmi8658_ctx.data_ready = 1U;
            Filter_ResetGyroLowPass();
            LOGI("IMU", "ready addr=0x%02X id=0x%02X status0=0x%02X",
                 QMI8658_I2C_Addr,
                 g_qmi8658_ctx.selected_id,
                 g_qmi8658_ctx.last_status0);
            QMI8658_SetState(QMI8658_STATE_READY, 0U);
            return 0;
        }
        if ((int32)(now_ms - g_qmi8658_ctx.ready_deadline_ms) >= 0) {
#if QMI8658_DIAG_ENABLE
            QMI8658_LogDataPath("ready_timeout");
#endif
            return QMI8658_EnterRetryOrFail("ready timeout");
        }
        g_qmi8658_ctx.due_ms = now_ms + 5U;
        return 0;

    default:
        QMI8658_SetState(QMI8658_STATE_FAILED, 0U);
        return -1;
    }
}

void QMI8658_RequestReinit(void)
{
    g_qmi8658_ctx.data_ready = 0U;
    g_qmi8658_ctx.init_retry = 0U;
    g_qmi8658_ctx.selected_id = 0xFFU;
    g_qmi8658_ctx.last_status0 = 0U;
    g_qmi8658_ctx.last_statusint = 0U;
    qmi8658_last_i2c_error = QMI8658_I2C_OK;
    QMI8658_I2C_Addr = QMI8658_I2C_ADDR_PRIMARY;
    QMI8658_SetState(QMI8658_STATE_BUS_PREPARE, 0U);
}

u8 QMI8658_IsReady(void)
{
    return (g_qmi8658_ctx.state == QMI8658_STATE_READY) ? 1U : 0U;
}

QMI8658_State_t QMI8658_GetState(void)
{
    return g_qmi8658_ctx.state;
}

u8 QMI8658_HasDataReady(void)
{
    return g_qmi8658_ctx.data_ready;
}

void QMI8658_ClearDataReady(void)
{
    g_qmi8658_ctx.data_ready = 0U;
}

u8 QMI8658_PollDataReady(void)
{
    if (g_qmi8658_ctx.state != QMI8658_STATE_READY) {
        return 0U;
    }

    if (QMI8658_CheckReadyFlag() != 0U) {
        g_qmi8658_ctx.data_ready = 1U;
    }
    return g_qmi8658_ctx.data_ready;
}

u8 QMI8658_ReadID(void)
{
    return QMI8658_ReadReg(QMI8658_REG_WHO_AM_I);
}

s8 QMI8658_ReadAcc(int16 *x, int16 *y, int16 *z)
{
    u8 raw[6];
    int16 ax;
    int16 ay;
    int16 az;

    if ((x == NULL) || (y == NULL) || (z == NULL)) {
        qmi8658_last_i2c_error = QMI8658_I2C_ERR_PARAM;
        return -1;
    }
    if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 6U) != 0U) {
        return -1;
    }

    ax = (int16)((u16)raw[1] << 8 | raw[0]);
    ay = (int16)((u16)raw[3] << 8 | raw[2]);
    az = (int16)((u16)raw[5] << 8 | raw[4]);
    if (QMI8658_ACC_IS_ZERO(ax, ay, az) || QMI8658_DATA_IS_INVALID(ax, ay, az)) {
        return -1;
    }

    *x = ax;
    *y = ay;
    *z = az;
    return 0;
}

s8 QMI8658_ReadGyro(int16 *x, int16 *y, int16 *z)
{
    u8 raw[6];
    int16 gx;
    int16 gy;
    int16 gz;

    if ((x == NULL) || (y == NULL) || (z == NULL)) {
        qmi8658_last_i2c_error = QMI8658_I2C_ERR_PARAM;
        return -1;
    }
    if (QMI8658_ReadNByte(QMI8658_REG_GX_L, raw, 6U) != 0U) {
        return -1;
    }

    gx = (int16)((u16)raw[1] << 8 | raw[0]);
    gy = (int16)((u16)raw[3] << 8 | raw[2]);
    gz = (int16)((u16)raw[5] << 8 | raw[4]);
    if (QMI8658_GYRO_IS_ZERO(gx, gy, gz) || QMI8658_DATA_IS_INVALID(gx, gy, gz)) {
        return -1;
    }

    *x = gx;
    *y = gy;
    *z = gz;
    return 0;
}

s8 QMI8658_ReadGyroFiltered(int16 *x, int16 *y, int16 *z)
{
    int16 gx;
    int16 gy;
    int16 gz;

    if ((x == NULL) || (y == NULL) || (z == NULL)) {
        qmi8658_last_i2c_error = QMI8658_I2C_ERR_PARAM;
        return -1;
    }
    if (QMI8658_ReadGyro(&gx, &gy, &gz) != 0) {
        return -1;
    }

    return Filter_GyroLowPass(gx, gy, gz, x, y, z);
}

s8 QMI8658_ReadTemp(int16 *temp)
{
    u8 raw[2];

    if (temp == NULL) {
        qmi8658_last_i2c_error = QMI8658_I2C_ERR_PARAM;
        return -1;
    }
    if (QMI8658_ReadNByte(QMI8658_REG_TEMP_L, raw, 2U) != 0U) {
        return -1;
    }

    *temp = (int16)((u16)raw[1] << 8 | raw[0]);
    return 0;
}

s8 QMI8658_ReadAll(int16 *ax, int16 *ay, int16 *az,
                   int16 *gx, int16 *gy, int16 *gz)
{
    u8 raw[12];
    int16 aax;
    int16 aay;
    int16 aaz;
    int16 ggx;
    int16 ggy;
    int16 ggz;

    if ((ax == NULL) || (ay == NULL) || (az == NULL) ||
        (gx == NULL) || (gy == NULL) || (gz == NULL)) {
        qmi8658_last_i2c_error = QMI8658_I2C_ERR_PARAM;
        return -1;
    }
    if (QMI8658_ReadNByte(QMI8658_REG_AX_L, raw, 12U) != 0U) {
        return -1;
    }

    aax = (int16)((u16)raw[1] << 8 | raw[0]);
    aay = (int16)((u16)raw[3] << 8 | raw[2]);
    aaz = (int16)((u16)raw[5] << 8 | raw[4]);
    ggx = (int16)((u16)raw[7] << 8 | raw[6]);
    ggy = (int16)((u16)raw[9] << 8 | raw[8]);
    ggz = (int16)((u16)raw[11] << 8 | raw[10]);

    if ((QMI8658_ACC_IS_ZERO(aax, aay, aaz) || QMI8658_DATA_IS_INVALID(aax, aay, aaz)) &&
        (QMI8658_GYRO_IS_ZERO(ggx, ggy, ggz) || QMI8658_DATA_IS_INVALID(ggx, ggy, ggz))) {
        return -1;
    }

    *ax = aax;
    *ay = aay;
    *az = aaz;
    *gx = ggx;
    *gy = ggy;
    *gz = ggz;
    g_qmi8658_ctx.data_ready = 0U;
    return 0;
}

s8 QMI8658_Wait_AccReady(u16 timeout_ms)
{
    u16 i;

    for (i = 0U; i < timeout_ms; i += 5U) {
        if ((QMI8658_ReadReg(QMI8658_REG_STATUS0) & QMI8658_STATUS0_A_DA) != 0U) {
            return 0;
        }
        QMI8658Port_DelayMs(5U);
    }
    return -1;
}

s8 QMI8658_Wait_GyroReady(u16 timeout_ms)
{
    u16 i;

    for (i = 0U; i < timeout_ms; i += 5U) {
        if ((QMI8658_ReadReg(QMI8658_REG_STATUS0) & QMI8658_STATUS0_G_DA) != 0U) {
            return 0;
        }
        QMI8658Port_DelayMs(5U);
    }
    return -1;
}

void QMI8658_BusRecover(void)
{
    QMI8658Port_BusRecover();
}

s8 QMI8658_Enable(void)
{
    return (QMI8658_WriteReg(QMI8658_REG_CTRL7, QMI8658_CTRL7_INIT) == 0U) ? 0 : -1;
}

s8 QMI8658_Disable(void)
{
    return (QMI8658_WriteReg(QMI8658_REG_CTRL7, 0x00U) == 0U) ? 0 : -1;
}

void QMI8658_DumpRawRegs(void)
{
    QMI8658_LogDataPath("dump");
}

u8 QMI8658_GetLastI2cError(void)
{
    return qmi8658_last_i2c_error;
}

char *QMI8658_GetLastI2cErrorName(void)
{
    return QMI8658_I2cErrName(qmi8658_last_i2c_error);
}
