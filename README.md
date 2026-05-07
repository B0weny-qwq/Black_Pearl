# Black Pearl v1.1

这个工程当前现场最重要的不是底层引脚说明，而是：

- 怎么打开上位机页面
- 怎么连串口
- 怎么看配对、遥控在线、油门、GPS 和磁力计
- 出问题时先看什么日志

## 1. 先打开上位机

在仓库根目录直接运行：

```text
start_ship_log_viewer.bat
```

如果你用 PowerShell，也可以：

```powershell
.\start_ship_log_viewer.ps1
```

它会自动：

- 启动本地 HTTP 服务
- 打开浏览器
- 跳到串口日志解析页面

根入口地址是：

```text
http://127.0.0.1:8000/
```

实际页面地址是：

```text
http://127.0.0.1:8000/doc/tools/ship_log_viewer.html
```

不要直接双击 HTML 文件。Web Serial 必须跑在 `http://127.0.0.1`、`http://localhost` 或 `https://` 下。

## 2. 上位机能看什么

页面当前重点看这些信息：

- 配对状态
- 遥控链路是否在线
- 油门数值
- 动作判定
- `0x12` 状态回包
- GPS 状态
- 电量采样
- 磁力计原始值

关键日志会被自动解析，例如：

- `[SHIP] I: pair req sent ...`
- `[SHIP] I: pair ok, enter work channel ...`
- `[SHIP] I: pair success paired=1 ...`
- `[SHIP] I: rc cmd=0x11 ...`
- `[SHIP] I: throttle_raw=...`
- `[SHIP] I: manual parse cmd=0x11 ...`
- `[SHIP] I: manual motion=...`
- `[SHIP] I: tx cmd=0x12 ...`
- `[SHIP] I: gps state fix=... sat=... lon=... lat=... angle=... seq=...`
- `[SHIP] W: remote link timeout by cmd=0x11 ...`
- `[MAG] I: test raw=...`

## 3. 正确理解页面状态

页面里有两个很容易混淆的状态：

- 已配对
- 遥控在线

它们不是一回事。

- “已配对”表示配对成功，已经进入工作信道
- “遥控在线”表示最近持续收到了 `0x11`

所以：

- 遥控器关机后，页面应该在超时后显示“离线”
- 但配对状态不一定会立刻回到“未配对”

## 4. GPS 现在怎么看

当前页面会显示正式 GPS 状态摘要：

- `fix`
- `sat`
- `lon`
- `lat`
- `angle`
- `seq`

固件里也会打印类似日志：

```text
[SHIP] I: gps state fix=1 sat=8 lon=E1212345678 lat=N312345678 angle=1234 power=0xAE seq=56
```

如果页面没有 GPS 数据，先看两件事：

1. 是否有下面这两条初始化日志

```text
[SYS] I: gps init start
[GPS] I: init uart=2 route=P1.0/P1.1 baud=115200
```

2. 是否真的收到了 GPS 语句并更新了 `gps state`

如果初始化有，但 `gps state` 一直没有，通常说明：

- GPS 模块没输出数据
- 线没通
- 波特率不对
- 还没有锁定，`fix=0`

## 5. 现场联调建议顺序

建议现场按这个顺序看：

1. 先打开上位机页面并连上串口
2. 看有没有无线初始化日志
3. 看有没有 GPS 初始化日志
4. 看配对请求和配对响应
5. 配对成功后，看 `rc cmd=0x11`
6. 看油门数值和动作判定是否跟遥杆一致
7. 关掉遥控器，看页面是否自动掉线
8. 再看 `0x12` 和 `gps state` 是否持续回传

## 6. 当前常用文件

- 上位机说明：[doc/tools/README.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/tools/README.md)
- 上位机页面：[doc/tools/ship_log_viewer.html](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/tools/ship_log_viewer.html)
- 固件无线主逻辑：[Code_boweny/Device/WIRELESS/ship_protocol.c](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/Code_boweny/Device/WIRELESS/ship_protocol.c)
- 工程总览：[doc/project_doc/total.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/total.md)
- 变更记录：[doc/project_doc/date.md](/C:/Users/S/Desktop/STC_PROJECT/Black_Pearl_v1.1/doc/project_doc/date.md)

## 7. 当前版本重点

当前联调重点是：

- 无线配对成功
- 遥控器手动链路跑通
- 关遥控后能正确判定离线
- `0x12` 状态回包可见
- GPS 状态可见
- 磁力计可见

IMU / AHRS / 数据融合不是当前现场主目标。
