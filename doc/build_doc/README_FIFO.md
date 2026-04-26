# FIFO 模块使用指南

## 概述

FIFO 是一个基于环形缓冲区（Ring Buffer）实现的数据队列模块，专为 STC32G 单片机设计。适用于 ISR（中断）与主循环之间的数据传递场景，例如 UART 接收、传感器数据缓存等。

---

## 核心特性

| 特性 | 说明 |
|------|------|
| 线程安全 | 支持通过关闭全局中断（EA）实现 ISR 上下文安全访问 |
| 原子操作 | 所有 API 均保证在 `FIFO_LOCK` 保护下的原子性 |
| 零拷贝 | 读写操作直接在外部传入的缓冲区上进行，无额外内存分配 |
| 批量操作 | 支持单字节和批量读写 |
| 调试支持 | 可通过 `FIFO_DEBUG_ENABLE` 开启读写索引查询 |

---

## 硬件需求

- **缓冲区**：需由用户在外部提供（静态数组或动态内存），FIFO 本身不管理内存生命周期
- **内存空间**：对于 `xdata` 区域的大型缓冲区（如 GPS 512 字节），确保在 xdata 段有足够空间

---

## 配置选项（`fifo.h`）

```c
#define FIFO_USE_MALLOC    DISABLE   // 是否启用动态分配（当前固定为 DISABLE）
#define FIFO_DEFAULT_SIZE  128       // 默认缓冲区大小
#define FIFO_LOCK_ENABLE   ENABLE    // 是否启用中断锁（建议开启）
#define FIFO_DEBUG_ENABLE  DISABLE   // 是否启用调试 API
```

> **建议**：始终保持 `FIFO_LOCK_ENABLE = ENABLE`，除非你能 100% 确认不存在 ISR 与主循环并发访问的情况。

---

## 数据结构

```c
typedef struct {
    u8  *buffer;    // 用户提供的外部缓冲区
    u16  size;      // 缓冲区总大小（字节）
    u16  r_index;   // 读索引（消费者指针）
    u16  w_index;   // 写索引（生产者指针）
    u16  count;     // 当前有效数据字节数
} fifo_t;
```

---

## API 参考

### 初始化

```c
void fifo_init(fifo_t *fifo, u8 *buf, u16 size);
```

在使用任何 FIFO API 之前必须先调用此函数，将用户缓冲区与 FIFO 控制块绑定。

**参数：**
- `fifo` - FIFO 控制块指针（通常为静态变量或全局变量）
- `buf` - 用户提供的缓冲区首地址
- `size` - 缓冲区大小（字节）

**示例：**

```c
static fifo_t uart_fifo;
static u8 xdata uart_rx_buffer[256];

fifo_init(&uart_fifo, uart_rx_buffer, sizeof(uart_rx_buffer));
```

---

### 状态查询

```c
u8 fifo_is_empty(fifo_t *fifo);   // 返回 1 表示空，0 表示非空
u8 fifo_is_full(fifo_t *fifo);    // 返回 1 表示满，0 表示未满
u16 fifo_get_count(fifo_t *fifo); // 返回当前有效数据字节数
u16 fifo_get_free(fifo_t *fifo);  // 返回剩余可写空间
```

---

### 清空与丢弃

```c
void fifo_clear(fifo_t *fifo);       // 清空所有数据，重置读写指针
u16 fifo_discard_all(fifo_t *fifo);  // 丢弃所有数据，返回丢弃的字节数
u16 fifo_skip(fifo_t *fifo, u16 len);// 跳过前 len 字节，返回实际跳过字节数
```

**注意：** `fifo_clear()` 和 `fifo_discard_all()` 都不会清空缓冲区内容，只是移动读写指针。

---

### 单字节操作

```c
u8 fifo_push(fifo_t *fifo, u8 dat);      // 写入 1 字节，返回 1 成功 / 0 失败
u8 fifo_pop(fifo_t *fifo, u8 *dat);      // 读取并消费 1 字节，返回 1 成功 / 0 失败
u8 fifo_peek(fifo_t *fifo, u8 *dat);     // 查看队首 1 字节但不移除，返回 1 成功 / 0 失败
```

---

### 批量操作

```c
u16 fifo_write(fifo_t *fifo, u8 *dat, u16 len);  // 批量写入，返回实际写入字节数
u16 fifo_read(fifo_t *fifo, u8 *dat, u16 len);   // 批量读取并消费，返回实际读取字节数
u16 fifo_drain(fifo_t *fifo, u8 *dat, u16 max_len);// 原子读取+消费，与 fifo_read 等价
```

**关于 `fifo_read` 与 `fifo_drain`：** 两者行为完全相同，`fifo_drain` 仅为语义强调"原子读取+消费"意图而保留。

---

## 典型使用模式

### ISR 生产 + 主循环消费（推荐）

这是最常见的用法，适用于 UART、传感器等数据采集场景。

```c
// ==================== 声明区 ====================
static fifo_t g_uart_fifo;
static u8 xdata g_uart_rx_buf[256];

// ==================== 初始化（main 中） ====================
fifo_init(&g_uart_fifo, g_uart_rx_buf, sizeof(g_uart_rx_buf));

// ==================== ISR 中（生产端） ====================
void UART1_ISR_Handler(void) interrupt UART1_VECTOR
{
    if (RI) {
        RI = 0;
        u8 dat = SBUF;
        fifo_push(&g_uart_fifo, dat); // 中断锁自动保护
    }
}

// ==================== 主循环中（消费端） ====================
void main(void)
{
    EA = 1;
    while (1) {
        u8 dat;
        while (fifo_pop(&g_uart_fifo, &dat)) {
            // 处理每个字节
            process_byte(dat);
        }
        // 其他任务...
    }
}
```

### 溢出保护处理

当 FIFO 满时，`fifo_push` 返回 0 表示写入失败。需要在 ISR 中处理溢出：

```c
void UART1_ISR_Handler(void) interrupt UART1_VECTOR
{
    if (RI) {
        RI = 0;
        u8 dat = SBUF;
        if (fifo_push(&g_uart_fifo, dat) == 0) {
            // FIFO 满，记录溢出事件或采取补救措施
            g_uart_overflow_count++;
        }
    }
}
```

### GPS 多语句组包场景

如果需要从 FIFO 中提取完整 NMEA 语句：

```c
u16 fifo_get_line(fifo_t *fifo, char *out, u16 max_len)
{
    u16 copied = 0;
    u8 dat;

    while (copied < max_len - 1 && fifo_pop(fifo, &dat)) {
        if (dat == '\n') {
            out[copied++] = 0;
            return copied;
        }
        if (dat != '\r') {  // 跳过 CR
            out[copied++] = (char)dat;
        }
    }
    out[copied] = 0;
    return copied;
}
```

---

## 内存布局示意

```
写入方向 →
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 01 │ 02 │ 03 │ 04 │ 05 │ 06 │ 07 │ 08 │   size=8, count=6, r=1, w=7
└────┴────┴────┴────┴────┴────┴────┴────┘
         ↑                         ↑
      r_index                   w_index

已读区域    有效数据区              空闲区域
（可覆盖）  01~06                  07
```

---

## 注意事项

### 1. 缓冲区大小选择

- FIFO 满时新数据会被丢弃。如果数据不允许丢失，缓冲区应足够大以容纳两次 poll 之间的最大数据量。
- 计算公式：`最小 size = 最大突发字节数 + 1`（保留一个 slot）
- 512 字节适用于 115200 波特率下约 44ms 的数据（每字符约 86μs）

### 2. ISR 中不要使用阻塞操作

所有 FIFO API 均为非阻塞设计，可以在 ISR 中安全调用。但注意不要在 ISR 中调用任何可能阻塞的函数（如 `printf`）。

### 3. 全局中断嵌套风险

`FIFO_LOCK()` 通过关闭/恢复 EA 实现。如果 ISR 中有多层中断嵌套调用 FIFO，嵌套的内层会错误恢复 EA。应确保 ISR 链路上只有一个 FIFO 锁。

### 4. 不要在 `fifo_init` 后仍使用 NULL 缓冲区

`fifo_push` 和 `fifo_pop` 在检测到 NULL 缓冲区时会静默返回 0（失败），不会崩溃但可能导致数据丢失。

### 5. 批量操作慎用

`fifo_write` 和 `fifo_read` 批量操作在锁内执行时间较长。如果 ISR 频繁调用，应优先使用单字节 `fifo_push`。

### 6. xdata 区域注意

对于 `xdata` 区域的大型缓冲区，确保链接脚本中 xdata 段足够大。在 Keil 中检查 `.xdata` 段大小。

---

## 调试模式

启用调试模式后，可查询内部读写索引：

```c
// fifo.h 中设置
#define FIFO_DEBUG_ENABLE  ENABLE

// 使用
u16 r, w;
fifo_get_indices(&my_fifo, &r, &w);
// r == w 且 count == 0  → 空
// r == w 且 count != 0  → 满（边界情况）
```

---

## 版本历史

| 版本 | 日期 | 作者 | 说明 |
|------|------|------|------|
| v3.0 | 2026-04-16 | boweny | 完善 API，统一错误处理 |
