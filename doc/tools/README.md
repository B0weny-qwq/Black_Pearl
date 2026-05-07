# 串口日志解析工具

文件位置：

- `doc/tools/ship_log_viewer.html`
- `doc/tools/start_ship_log_viewer.py`
- `index.html`
- `start_ship_log_viewer.bat`
- `start_ship_log_viewer.ps1`

用途：

- 连接船控板串口
- 选择波特率、数据位、停止位、校验位
- 实时接收并解析串口日志
- 区分“已配对”和“遥控在线”
- 展示 `0x11` 手动控制、动作判定、`0x12` 状态回传、电量、磁力计
- 展示正式 GPS 状态日志：`fix / sat / lon / lat / angle / seq`
- 支持把 `.log/.txt` 日志粘贴进页面做离线解析

## 推荐打开方式

在仓库根目录直接运行：

```text
start_ship_log_viewer.bat
```

或者：

```powershell
.\start_ship_log_viewer.ps1
```

它会自动：

- 从仓库根目录启动本地 HTTP 服务
- 自动打开浏览器
- 进入当前页面

## 为什么不能直接双击 HTML

Web Serial 不能在普通 `file://` 本地文件上下文里访问串口。
浏览器要求页面运行在安全上下文，常见形式是：

- `http://127.0.0.1`
- `http://localhost`
- `https://...`

所以页面文件虽然是 `index.html` / `ship_log_viewer.html`，但仍然要通过本地服务打开。

## 当前路径

- 根入口：`http://127.0.0.1:8000/`
- 实际页面：`http://127.0.0.1:8000/doc/tools/ship_log_viewer.html`

根目录 `index.html` 会自动跳转到实际页面，避免用户再去找路径。

## 当前解析重点

- `[SHIP] I: pair req sent ...`
- `[SHIP] I: pair ok, enter work channel ...`
- `[SHIP] I: pair success paired=1 ...`
- `[SHIP] I: remote link online by cmd=0x11`
- `[SHIP] W: remote link timeout by cmd=0x11 ...`
- `[SHIP] I: rc cmd=0x11 ...`
- `[SHIP] I: throttle_raw=...`
- `[SHIP] I: manual parse cmd=0x11 ...`
- `[SHIP] I: manual motion=...`
- `[SHIP] I: tx cmd=0x12 ...`
- `[SHIP] I: gps state fix=... sat=... lon=... lat=... angle=... seq=...`
- `[SHIP] I: adc p0.0 ...`
- `[MAG] I: test raw=...`
- `[WL]/[SHIP]/[MAG]/[GPS]` 的 `W:` / `E:` 日志

## 说明

- 当前页面重点是给现场联调用，不是全协议抓包器。
- 当前语义里：
  - “已配对”只表示工作信道和配对状态已建立。
  - `0x11` 表示遥控在线与手动控制输入。
- 如果后续串口日志格式继续变化，只需要扩展页面里的正则解析规则，不需要改协议。
