# LOG 日志系统文档

本文档记录项目日志系统的设计和使用说明，供后续维护参考。

---

## 1. 系统概述

LOG 系统是基于 UART1 的轻量级日志输出模块，提供带标签、分级别的日志功能。

**输出硬件**：UART1（P3.1=TXD, P3.0=RXD）

---

## 2. API 接口

### 2.1 日志级别

| 函数 | 宏定义 | 级别 | 输出格式 |
|------|--------|------|---------|
| `log_info(tag, fmt, ...)` | `LOGI(tag, fmt, ...)` | INFO | `[tag] I: message` |
| `log_warn(tag, fmt, ...)` | `LOGW(tag, fmt, ...)` | WARN | `[tag] W: message` |
| `log_error(tag, fmt, ...)` | `LOGE(tag, fmt, ...)` | ERROR | `[tag] E: message` |
| `log_debug(tag, fmt, ...)` | `LOGD(tag, fmt, ...)` | DEBUG | `[tag] D: message` |
| `log_printf(fmt, ...)` | - | RAW | 直接输出，无标签无级别 |

### 2.2 初始化

```c
void log_init(void);
```

**注意**：必须在 UART1 初始化完成后调用 `log_init()` 才能输出日志。调用后 `log_ready` 标志置 1，所有日志函数才会实际输出。

### 2.3 使用示例

```c
// 初始化（通常在 main 中）
log_init();

// 带标签的日志
LOGI("IMU", "init begin");
LOGI("IMU", "acc range=%d gyro range=%d", acc_range, gyro_range);
LOGW("I2C", "bus not idle scl=%u sda=%u", P15, P14);
LOGE("I2C", "write failed status=%u", status);
LOGD("LOOP", "data=%d", value);

// 原始输出（无标签）
log_printf("raw output: %d %d %d\r\n", a, b, c);
```

---

## 3. 输出格式

### 3.1 带标签日志

```
[tag] level: message\r\n
```

**示例**：
```
[IMU] I: init begin
[IMU] I: acc range=2 gyro range=4
[I2C] W: bus not idle scl=1 sda=1
[IMU] E: read failed status=2
```

### 3.2 原始日志

```
message\r\n
```

---

## 4. 内部实现

### 4.1 核心函数

| 函数 | 说明 |
|------|------|
| `log_vprint()` | 内部函数，处理 raw printf |
| `log_vtagged()` | 内部函数，添加 [tag] level: 前缀 |

### 4.2 内部变量

| 变量 | 类型 | 说明 |
|------|------|------|
| `log_ready` | static u8 | 日志就绪标志，0=禁用，1=启用 |

### 4.3 缓冲区

日志系统使用 `vsprintf()` 格式化字符串，**最大支持 127 字符**（不含末尾的 `\r\n`）。

**注意**：如果格式化后的字符串超过 127 字符，会发生缓冲区溢出。

---

## 5. 依赖关系

| 依赖 | 说明 |
|------|------|
| `STC32G_UART.h` | UART1 输出，使用 `PrintString1()` |
| `stdarg.h` | 可变参数支持 |
| `config.h` | 类型定义（u8, u16 等） |

---

## 6. 注意事项

### 6.1 调用顺序

```
UART1_Init() → log_init() → LOG*()
```

日志必须在 UART1 初始化后才能使用。

### 6.2 缓冲区大小

当前 `buf[128]` 限制：
- 支持最长 127 字符的格式化消息
- 超出部分会被截断或溢出
- 如需输出长字符串，使用多次 `log_printf()`

### 6.3 printf 支持

LOG 系统使用 `vsprintf()`，**不支持浮点数格式化**（STC32G 不链接浮点库）。

```c
// 不支持的用法
LOGI("TEST", "value=%f", 3.14);  // 错误

// 正确用法：自行转换
int whole = (int)3.14;
int frac = (int)((3.14 - whole) * 1000);
LOGI("TEST", "value=%d.%d", whole, frac);  // 输出 3.140
```

---

## 7. 相关文件

| 文件 | 说明 |
|------|------|
| `Device/inc/log.h` | 头文件，API 声明 |
| `Device/src/log.c` | 实现文件 |
| `User/System_init.c` | UART1 初始化 |

---

## 8. 优化建议

### 8.1 当前问题

| 问题 | 说明 |
|------|------|
| 缓冲区固定 128 字节 | 长日志会被截断 |
| 无时间戳 | 无法知道日志发生的时间 |
| 无日志级别过滤 | DEBUG 日志无法在运行时关闭 |
| 无日志输出开关 | 无法动态关闭日志以节省 UART 带宽 |
| 无环形缓冲区 | 如果 UART 阻塞，日志会丢失 |

### 8.2 优化方向

#### 方案 A：增加日志级别过滤

```c
#define LOG_LEVEL  LOG_LEVEL_INFO   // 编译时选择

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOGD(...)  log_debug(__VA_ARGS__)
#else
#define LOGD(...)  ((void)0)        // DEBUG 日志禁用
#endif
```

#### 方案 B：增加动态开关

```c
static u8 log_enable_mask = LOG_MASK_INFO | LOG_MASK_WARN | LOG_MASK_ERROR;

void log_set_mask(u8 mask) {
    log_enable_mask = mask;
}

void log_disable_all(void) {
    log_enable_mask = 0;
}
```

#### 方案 C：增加时间戳

```c
static void log_vtagged(char level, char *tag, char *fmt, va_list args)
{
    u16 tick = Timer2_get_ms();  // 需要实现
    sprintf(buf, "[%05u][%s] %c: ", tick, tag, level);
    // ...
}
```

#### 方案 D：增加环形缓冲区（生产环境推荐）

```c
#define LOG_BUF_SIZE  256

static char log_ring[LOG_BUF_SIZE];
static u16 log_head = 0;
static u16 log_tail = 0;

void log_push(char *msg) {
    // 写入环形缓冲区
    // UART 中断在后台消费
}
```

#### 方案 E：支持浮点数

如果需要浮点数支持，可以：

1. 使用自定义的 `fixed_to_str()` 函数
2. 或者链接完整的浮点库（增加代码体积）

---

## 9. 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-04-22 | v1.0 | 初版文档 |
