# 串口日志上位机兼容目录

主目录已经迁移到 `tools/ship_log_viewer/`。

## 当前用途

- `doc/tools/ship_log_viewer.html` 只保留兼容跳转入口
- 根目录 `start_ship_log_viewer.bat` / `start_ship_log_viewer.ps1` 已改为启动 `tools/ship_log_viewer/start_ship_log_viewer.py`
- 正式说明请看 [`tools/ship_log_viewer/README.md`](../../tools/ship_log_viewer/README.md)

当前说明边界：

- 本目录不再作为上位机主维护入口，只保留旧文档兼容跳转。
- 若日志格式、viewer 字段或启动方式变化，应优先更新 `tools/ship_log_viewer/README.md` 和根目录 README。
