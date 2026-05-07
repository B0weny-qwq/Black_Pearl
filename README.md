# Black Pearl v1.1

这个工程现场使用时，重点不是底层引脚表，而是怎么打开上位机、怎么看无线遥控、油门、按键、GPS 和状态回包。

## 1. 打开上位机

在仓库根目录运行：

```text
start_ship_log_viewer.bat
```

PowerShell 也可以运行：

```powershell
.\start_ship_log_viewer.ps1
```

脚本会启动本地 HTTP 服务并打开页面。浏览器地址是：

```text
http://127.0.0.1:8000/doc/tools/ship_log_viewer.html
```

不要直接双击 HTML 文件。Web Serial 必须在 `http://127.0.0.1`、`http://localhost` 或 `https://` 环境下运行。

## 2. 上位机怎么看

页面重点看这些卡片：

- 配对状态：只表示已经进入工作信道。
- 遥控在线：只看最近是否收到 `0x11`，关闭遥控器后应在超时后变离线。
- 油门数值：看 `throttle_raw / steering_raw / throttle_val / steering_val`。
- 动作判定：看 `manual motion` 的 forward/backward/left/right/stop。
- 按键状态：解析 A/B/C/D/E 和固件 `key action` 日志。
- GPS：同时显示 `gps state`、老版 `0x12` 字段和 15 字节 payload。

关键日志示例：

```text
[SHIP] I: rc cmd=0x11 lr=100 ud=179 key=0xA0(NONE) paired=1
[SHIP] I: throttle_raw=179 steering_raw=100 throttle_val=79 steering_val=0 key=0xA0(NONE)
[SHIP] I: manual motion=forward left=790 right=-790
[SHIP] I: tx cmd=0x12 ch=13 payload_len=15 sat=8 angle=1 power=0xAE auto=0x00
[SHIP] I: gps state fix=1 legacy=1 sat=8 lon=E121940096 lat=N373696970 angle=1 power=0xAE seq=56
[SHIP] I: gps sat source gsa=8 gga=10 report=8
[SHIP] I: gps payload oldfmt ew=E lon1=12156 lon2=4607 ns=W lat1=3724 lat2=2182
[SHIP] I: gps payload bytes=08 00 01 45 2F 7C 11 FF 57 0E 8C 08 86 AE 00
```

## 3. GPS 格式说明

`0x12` 继续保持老版遥控器兼容格式，payload 固定 15 字节，不新增字段：

```text
sat, angle_u16, 'E', lon1_u16, lon2_u16, 'W', lat1_u16, lat2_u16, power, auto
```

坐标不是 `deg1e7` 直接发给遥控器，而是按老版 RMC 原始字符串拆分：

- 经度 `12156.4607500` 发成 `lon1=12156`、`lon2=4607`。
- 纬度 `3724.2182068` 发成 `lat1=3724`、`lat2=2182`。
- 航向角按老版发整数度，不发 `deg * 100`。
- 卫星数按老版优先使用完整 GSA PRN 计数；GSA 不完整时回退 GGA 卫星数，并限制最大 24。
- 方向字节按老版保持 `E/W` 常量；真实半球只在 `gps state` 日志中显示。

如果现场看到 `fix=0 legacy=0 lon=E0 lat=N0`，说明 GPS 当前没有有效 RMC 定位。此时 `0x12` 坐标为 0 是预期现象，需要先看 GPS 天线、室外环境、波特率和 UART2 数据是否正常。

## 4. 现场检查顺序

1. 打开上位机并连接串口。
2. 看 `[SYS] I: gps init start` 和 `[GPS] I: init uart=2 route=P1.0/P1.1 baud=115200`。
3. 看无线配对 `pair req / pair success`。
4. 配对成功后看 `rc cmd=0x11` 是否持续出现。
5. 看油门数值和动作判定是否跟遥杆一致。
6. 关遥控器，看“遥控在线”是否超时变离线。
7. 看 `gps payload oldfmt` 和 `gps payload bytes` 是否与遥控器端显示一致。

## 5. 当前版本重点

当前联调版本保持无线、GPS、磁力计开启；IMU、AHRS、数据融合关闭。电机 PWM 现在按宏关闭，主要用于串口观察油门、动作、按键和 GPS 回包。

常用文件：

- [上位机页面](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/tools/ship_log_viewer.html)
- [无线协议主逻辑](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.c)
- [GPS 解析](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/GPS/GPS.c)
- [工程总览](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)
- [变更记录](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)
