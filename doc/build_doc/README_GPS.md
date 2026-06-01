# GPS NMEA0183 解析说明

## 概述

本模块基于 NMEA0183 协议实现 GPS / 北斗 / 多模 GNSS 数据解析，适用于 UART + 中断 + FIFO 的嵌入式系统结构。

支持内容：

- GPS / 北斗 / 多模 GNSS，识别 `GP / BD / GN` talker。
- 标准 NMEA 语句解析。
- 实时位置、速度、航向、定位质量获取。

## 通信参数

| 参数 | 值 |
|------|----|
| 接口 | UART |
| 常见波特率 | 9600 / 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |

串口帧格式：起始位 + 8bit 数据 + 停止位。

## NMEA 协议结构

GPS 数据本质是 ASCII 字符串：

```text
$[Talker][Type],data1,data2,...*CS<CR><LF>
```

示例：

```text
$GPGGA,235316.000,2959.9925,S,12000.0090,E,1,06,1.21,62.77,M,0.00,M,,*7B
```

字段说明：

| 字段 | 含义 |
|------|------|
| `$` | 起始符 |
| `GP/GN/BD` | 系统标识 |
| `GGA/RMC/...` | 消息类型 |
| `*CS` | 校验和 |
| `CRLF` | 结束符 |

校验规则：

```text
CS = '$' 与 '*' 之间所有字符的 XOR
```

## 常用 NMEA 消息

### GGA：定位核心信息

```text
$GPGGA,UTC,lat,N,lon,E,fix,sat,hdop,alt,M,...
```

| 字段 | 含义 |
|------|------|
| UTC | 时间 |
| lat/lon | 经纬度 |
| fix | 定位状态 |
| sat | 卫星数 |
| hdop | 水平精度 |
| alt | 海拔 |

`fix=0` 表示无定位，`fix=1` 表示有效定位。

### RMC：推荐主数据源

```text
$GPRMC,UTC,status,lat,lon,speed,course,date,...
```

| 字段 | 含义 |
|------|------|
| status | `A`=有效，`V`=无效 |
| speed | 地速，单位节 |
| course | 航向角 |
| date | 日期 |

工程中推荐以 RMC 作为主状态更新时间源。

### GSA：定位质量

提供定位模式、PDOP、HDOP、VDOP。

### GSV：可见卫星

提供可见卫星数量和信号强度。

### VTG：速度和方向

提供航向角和地速补充信息。

## 经纬度换算

NMEA 经纬度不是直接的十进制度数：

```text
纬度：ddmm.mmmm
经度：dddmm.mmmm
```

换算公式：

```text
degree = degree_part + minute_part / 60
```

示例：

```text
2959.9925 -> 29°59.9925'
lat = 29 + 59.9925 / 60
```

在 STC32G 固件中建议使用定点整数换算，不使用浮点。

## 推荐工程结构

```text
UART ISR -> FIFO -> parser -> state
```

中断中只收字节：

```c
void gps_uart_rx_isr(uint8_t ch)
{
    fifo_push(ch);
}
```

主循环中解析：

```c
void gps_poll(void)
{
    while (fifo_has_data()) {
        char c = fifo_pop();
        nmea_parse_char(c);
    }
}
```

解析流程：

```text
$ -> 收集 -> 逗号分割 -> 校验 -> 解析字段 -> 更新状态
```

## 工程建议

1. 主数据源优先使用 RMC。
2. 用 RMC UTC 或日期变化判断状态刷新。
3. 使用逐字符状态机，不建议依赖 `strtok`。
4. 必须做 XOR 校验，否则串口噪声会污染状态。
5. 字段解析成功后再更新状态结构体，避免坏句子覆盖上一帧有效状态。

## 性能与风险

- 115200 波特率下单字节约 86us，主循环卡顿会导致丢数据。
- FIFO 太小会造成溢出，表现为定位跳变或丢帧。
- 多系统 talker 中 `GN` 表示混合系统，建议统一支持。
- GSV 可能由多条语句组成，不应作为主位置更新时间源。

## 最小可用实现

导航场景最小只需要：

1. UART 收数据。
2. 查找 `$GNRMC` 或 `$GPRMC`。
3. 校验通过后解析经纬度、速度、航向。
4. 使用定点格式保存结果。

一句话总结：GPS 模块就是一个持续输出 CSV 风格字符串的串口设备，固件要做的是收字符、切字段、校验、换算并更新状态。
> 当前版本说明：
> 本文主要解释 GPS NMEA 解析基础原理和实现思路。
> 若与当前工程源码、根目录 README、`MainLoop` 或 `GPS.c` 不一致，以当前源码为准。
>
> 当前架构关系：
> - `GPS.c` 只负责 UART2 字节接收、NMEA 解析和 GPS 状态缓存。
> - `ship_protocol.c` 可读取 GPS 状态用于 `0x12` 遥测输出。
> - `AutoDrive` / `NorthCalib` 可复用 GPS 的位置、速度和航迹信息。
> - GPS 模块本身不负责返航决策、北向校准或电机控制。
