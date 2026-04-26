# LOG 轻量级日志模块

## 概述

基于 UART1 的轻量级日志输出模块，提供带标签、分级别的日志功能。

| 项目 | 说明 |
|------|------|
| 输出硬件 | UART1 (P3.1=TXD, P3.0=RXD) |
| 波特率 | 115200, 8N1 |
| 最大消息长度 | 127 字符 (不含 `\r\n`) |
| 缓冲区大小 | 128 字节 |

---

## 文件结构

```
Code_boweny/Function/Log/
├── Log.h       # API声明、宏定义、DOxygen注释
├── Log.c       # 模块实现
└── README.md   # 本文档
```

---

## 级别与输出格式

| 函数 | 宏 | 级别 | 格式 |
|------|-----|------|------|
| `log_info(tag, fmt, ...)` | `LOGI(tag, fmt, ...)` | INFO | `[tag] I: message\r\n` |
| `log_warn(tag, fmt, ...)` | `LOGW(tag, fmt, ...)` | WARN | `[tag] W: message\r\n` |
| `log_error(tag, fmt, ...)` | `LOGE(tag, fmt, ...)` | ERROR | `[tag] E: message\r\n` |
| `log_debug(tag, fmt, ...)` | `LOGD(tag, fmt, ...)` | DEBUG | `[tag] D: message\r\n` |
| `log_printf(fmt, ...)` | — | RAW | `message\r\n` |

**输出示例**:
```
[IMU] I: init begin
[IMU] I: acc range=2 gyro range=4
[I2C] W: bus not idle scl=1 sda=1
[IMU] E: read failed status=2
[LOOP] D: data=123
```

---

## 快速使用

### 1. 初始化 (System_init.c)

日志系统已在 `SYS_Init()` 中自动初始化，调用顺序:

```c
void SYS_Init(void)
{
    Timer_config();   // 系统时钟基准
    UART_config();    // 波特率 115200
    Switch_config();  // 引脚切换
    EA = 1;
    APP_config();
    log_init();       // 启用日志输出
}
```

> **注意**: `log_init()` 必须在 `UART_config()` 之后调用，否则日志函数不生效。

### 2. 调用示例

```c
#include "Log.h"

void MyTask(void)
{
    LOGI("MYTK", "task start");
    LOGI("MYTK", "count=%d", 42);

    LOGW("MYTK", "threshold exceeded val=%u", 200);

    if (FAIL == some_operation())
    {
        LOGE("MYTK", "operation failed");
        LOGE("MYTK", "error code=%u", error_code);
    }

    LOGD("MYTK", "loop tick");

    log_printf("raw: %d %d %d\r\n", a, b, c);
}
```

---

## 依赖关系

```
Log.h
├── STC32G_UART.h    (PrintString1, TX1_write2buff)
├── stdarg.h         (va_list, va_start, va_end)
└── config.h         (u8, SUCCESS, FAIL, MAIN_Fosc)
```

---

## 限制与注意事项

### 禁止浮点数格式化

STC32G 没有 FPU，`vsprintf()` 不支持 `%f`。如需输出小数，请先手动转换:

```c
// 错误
LOGI("TEST", "value=%f", 3.14);

// 正确: 手动分离整数和小数部分
int whole = (int)3.14;
int frac  = (int)((3.14 - whole) * 1000);
LOGI("TEST", "value=%d.%03d", whole, frac);  // 输出 3.140
```

### 缓冲区长度限制

- 单条日志消息最大 127 字符 (不含 `\r\n`)
- 超出部分被截断，不会内存越界
- 长消息建议拆分为多次 `log_printf()`

### 调用顺序

```
UART_config() → log_init() → LOG*()
```

`log_init()` 将内部 `log_ready` 标志置 1。在 UART1 初始化完成前调用日志函数，函数直接返回，不输出也不崩溃。

---

## API 参考

### log_init

```c
void log_init(void);
```

初始化日志系统，将 `log_ready` 置 1。

### log_info / log_warn / log_error / log_debug

```c
void log_info (u8 *tag, u8 *fmt, ...);
void log_warn (u8 *tag, u8 *fmt, ...);
void log_error(u8 *tag, u8 *fmt, ...);
void log_debug(u8 *tag, u8 *fmt, ...);
```

- `tag`: 日志标签 (如 `"IMU"`, `"I2C"`)，建议简短
- `fmt`: 格式化字符串，支持 `%d` `%u` `%x` `%c` `%s` 等
- `...`: 可变参数

### log_printf

```c
void log_printf(u8 *fmt, ...);
```

原始输出，无标签、无级别，直接打印格式化消息。

---

## 相关文档

| 文档 | 路径 |
|------|------|
| LOG 系统设计文档 | `doc/device_doc/LOG.md` |
| 工程总览 | `doc/project_doc/total.md` |
| 开发日志 | `doc/project_doc/date.md` |

---

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-22 | v1.0 | 初版实现 |
