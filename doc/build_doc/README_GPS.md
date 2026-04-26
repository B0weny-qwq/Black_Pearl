1. 概述

本模块基于 NMEA 0183 协议 实现 GPS 数据解析，适用于嵌入式系统（UART + 中断 + FIFO架构）。

支持：

GPS / 北斗 / 多模 GNSS（GP / BD / GN 标识）
标准 NMEA 语句解析
实时位置 / 速度 / 航向获取

📌 协议来源：

标准 NMEA0183
厂商扩展（CASIC等）
2. 通信参数
参数	值
接口	UART
波特率	9600 / 115200（常用）
数据位	8
停止位	1
校验	无

📌 数据格式（串口帧）：

起始位 + 8bit数据 + 停止位
3. NMEA 协议结构（核心）

所有 GPS 数据都是字符串：

$[Talker][Type],data1,data2,...*CS<CR><LF>
示例：
$GPGGA,235316.000,2959.9925,S,12000.0090,E,1,06,1.21,62.77,M,0.00,M,,*7B
字段说明
字段	含义
$	起始符
GP/GN/BD	系统标识
GGA/RMC/...	消息类型
*CS	校验
CRLF	结束

📌 校验规则：

CS = XOR($ 和 * 之间所有字符)
4. 常用 NMEA 消息（必须支持）
⭐ 1. GGA（定位核心）
$GPGGA,UTC,lat,N,lon,E,fix,sat,hdop,alt,M,...

📌 关键字段：

字段	含义
UTC	时间
lat/lon	经纬度
fix	定位状态
sat	卫星数
hdop	精度
alt	海拔

📌 定位状态：

0 = 无定位
1 = 有效定位

👉 这是最基础定位数据

⭐⭐ 2. RMC（推荐用这个做主数据源）
$GPRMC,UTC,status,lat,lon,speed,course,date,...

📌 关键字段：

字段	含义
status	A=有效
speed	地速（节）
course	航向角
date	日期

👉 工程建议：主用 RMC 做状态更新

⭐⭐ 3. GSA（定位质量）
$GPGSA,A,3,...,PDOP,HDOP,VDOP
字段	含义
3	3D定位
PDOP	总精度
HDOP	水平精度
⭐ 4. GSV（卫星信息）
$GPGSV,3,1,10,...

👉 提供：

可见卫星数
信号强度
⭐ 5. VTG（速度+方向）
$GPVTG,course,T,...,speed,N,...
5. 经纬度解析（重点）

NMEA 不是直接度！

纬度：ddmm.mmmm
经度：dddmm.mmmm
转换公式：
float lat = dd + mm / 60.0;
float lon = ddd + mm / 60.0;
示例：
2959.9925 → 29°59.9925'
lat = 29 + 59.9925 / 60
6. 工程解析架构（你必须这么写）
推荐结构：
UART ISR → FIFO → parser → state
1️⃣ UART中断（收字节）
void gps_uart_rx_isr(uint8_t ch)
{
    fifo_push(ch);
}
2️⃣ 主循环解析
void gps_poll(void)
{
    while (fifo_has_data()) {
        char c = fifo_pop();
        nmea_parse_char(c);
    }
}
3️⃣ 语句解析流程
$ → 收集 → ,分割 → 校验 → 解析字段
4️⃣ 状态结构（推荐）
typedef struct {
    uint8_t fix;
    float lat;
    float lon;
    float speed;
    float course;
    uint8_t sat;
} gps_state_t;
7. 推荐工程策略（很关键）
✅ 1. 主数据源选 RMC

原因：

时间稳定
不重复
包含核心数据

👉 你 README 的 DK2_RMC 思路是对的

✅ 2. 用时间戳判断刷新
RMC UTC 变 → 数据更新

避免：

GGA/GSV 多条重复解析
✅ 3. 做状态机解析（不要 strtok）

原因：

ISR数据流
字符流不是完整包
✅ 4. 校验必须做

否则：

串口噪声直接炸数据
8. 性能与坑点
⚠️ 1. 串口速率

115200：

≈ 86us / byte

👉 主循环卡住就丢数据

⚠️ 2. FIFO 溢出

表现：

定位跳变 / 丢帧

解决：

增大 FIFO
提高 poll 频率
⚠️ 3. 多系统问题
GP → GPS
BD → 北斗
GN → 混合

👉 推荐统一解析 GN

9. 可选扩展（进阶）

如果你要做高级系统（比如你的小车/无人船）：

🔥 1. 融合 IMU
GPS：低频（1~10Hz）
IMU：高频（100Hz+）

👉 用 EKF 融合

🔥 2. 做航向闭环

来源：

GPS course（低速不准）
陀螺仪（短期准）

👉 融合

🔥 3. ROS 接入
/gps/fix → NavSatFix
10. 最小可用实现（你现在就能跑）

你只需要：

UART收数据
找 $GNRMC
解析：
lat
lon
speed
course

👉 就能做导航了

🧠 总结（给你一句人话）

这个协议本质就是：

GPS模块 = 一个疯狂往串口吐 CSV 的设备

你要做的就是：

收字符串 → 切字段 → 转float → 用