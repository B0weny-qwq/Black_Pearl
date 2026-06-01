# FIFO 模块使用指南

## 概述

FIFO 是基于环形缓冲区实现的数据队列模块，适用于 ISR 中断上下文和主循环之间传递数据，例如 UART 接收、传感器数据缓存等。

## 核心特性

| 特性 | 说明 |
|------|------|
| 中断安全 | 通过关闭/恢复全局中断 `EA` 保护访问 |
| 原子操作 | API 在 `FIFO_LOCK` 保护下更新读写索引 |
| 零拷贝 | 读写直接在外部传入缓冲区上进行 |
| 批量操作 | 支持单字节和批量读写 |
| 调试支持 | 可通过 `FIFO_DEBUG_ENABLE` 查询读写索引 |

## 硬件和内存要求

- 缓冲区由用户在外部提供，FIFO 不管理内存生命周期。
- 对于 `xdata` 区域的大缓冲区，例如 GPS 512 字节缓存，需要确认链接脚本中 `xdata` 空间足够。

## 配置选项

```c
#define FIFO_USE_MALLOC    DISABLE
#define FIFO_DEFAULT_SIZE  128
#define FIFO_LOCK_ENABLE   ENABLE
#define FIFO_DEBUG_ENABLE  DISABLE
```

建议始终保持 `FIFO_LOCK_ENABLE = ENABLE`，除非能确认不存在 ISR 与主循环并发访问。

## 数据结构

```c
typedef struct {
    u8  *buffer;    /* 用户提供的外部缓冲区 */
    u16  size;      /* 缓冲区总大小，单位 byte */
    u16  r_index;   /* 读索引 */
    u16  w_index;   /* 写索引 */
    u16  count;     /* 当前有效数据字节数 */
} fifo_t;
```

## API

### 初始化

```c
void fifo_init(fifo_t *fifo, u8 *buf, u16 size);
```

使用任意 FIFO API 前必须先调用此函数，将用户缓冲区与 FIFO 控制块绑定。

示例：

```c
static fifo_t uart_fifo;
static u8 xdata uart_rx_buffer[256];

fifo_init(&uart_fifo, uart_rx_buffer, sizeof(uart_rx_buffer));
```

### 状态查询

```c
u8 fifo_is_empty(fifo_t *fifo);
u8 fifo_is_full(fifo_t *fifo);
u16 fifo_get_count(fifo_t *fifo);
u16 fifo_get_free(fifo_t *fifo);
```

### 清空与丢弃

```c
void fifo_clear(fifo_t *fifo);
u16 fifo_discard_all(fifo_t *fifo);
u16 fifo_skip(fifo_t *fifo, u16 len);
```

注意：这些函数只移动读写指针，不清除缓冲区内的旧字节。

### 单字节操作

```c
u8 fifo_push(fifo_t *fifo, u8 dat);
u8 fifo_pop(fifo_t *fifo, u8 *dat);
u8 fifo_peek(fifo_t *fifo, u8 *dat);
```

返回 `1` 表示成功，返回 `0` 表示失败。

### 批量操作

```c
u16 fifo_write(fifo_t *fifo, u8 *dat, u16 len);
u16 fifo_read(fifo_t *fifo, u8 *dat, u16 len);
u16 fifo_drain(fifo_t *fifo, u8 *dat, u16 max_len);
```

`fifo_read` 与 `fifo_drain` 行为等价，`fifo_drain` 仅用于强调“读取并消费”的语义。

## 典型使用模式

### ISR 生产，主循环消费

```c
static fifo_t g_uart_fifo;
static u8 xdata g_uart_rx_buf[256];

void init(void)
{
    fifo_init(&g_uart_fifo, g_uart_rx_buf, sizeof(g_uart_rx_buf));
}

void UART1_ISR_Handler(void) interrupt UART1_VECTOR
{
    if (RI) {
        u8 dat;
        RI = 0;
        dat = SBUF;
        fifo_push(&g_uart_fifo, dat);
    }
}

void main_loop(void)
{
    u8 dat;

    while (fifo_pop(&g_uart_fifo, &dat)) {
        process_byte(dat);
    }
}
```

### 溢出处理

```c
if (fifo_push(&g_uart_fifo, dat) == 0) {
    g_uart_overflow_count++;
}
```

FIFO 满时新数据会写入失败，上层应记录溢出或采取补救措施。

## 缓冲区大小选择

- FIFO 满时新数据会丢弃。
- 若数据不能丢失，缓冲区应足够容纳两次 poll 之间的最大突发数据量。
- 115200 波特率下，512 字节约可容纳 44ms 数据。

## 注意事项

1. 不要在 ISR 中执行阻塞操作，例如 `printf`。
2. `FIFO_LOCK()` 通过关闭/恢复 `EA` 实现，避免在复杂嵌套中断中重复嵌套使用。
3. `fifo_init()` 后不要继续使用 NULL 缓冲区。
4. 批量读写在锁内执行时间较长，高频 ISR 场景优先使用单字节 API。
5. 使用 `xdata` 大缓冲区时检查 Keil 输出中的 `.xdata` 占用。

## 调试模式

开启调试模式后可查询内部读写索引：

```c
#define FIFO_DEBUG_ENABLE ENABLE

u16 r, w;
fifo_get_indices(&my_fifo, &r, &w);
```

## 版本历史

| 版本 | 日期 | 作者 | 说明 |
|------|------|------|------|
| v3.0 | 2026-04-16 | boweny | 完善 API，统一错误处理 |
> 当前版本说明：
> 本文描述通用 FIFO 设计思路，主要用于理解 UART/GPS 一类的“中断生产、主循环消费”模型。
> Black Pearl 当前 GPS 接收链路已经在 `Code_boweny/Device/GPS/GPS.c` 内部维护自己的轻量 FIFO；
> 若与历史文档描述不一致，以当前源码为准。
>
> 当前工程中的关系：
> - FIFO 主要服务于串口字节缓存和主循环解析。
> - 它是基础数据搬运层，不负责业务协议、姿态融合或自动导航决策。
