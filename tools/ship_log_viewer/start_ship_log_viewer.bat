@echo off
rem @file start_ship_log_viewer.bat
rem @brief 本地船端串口日志查看器的一键启动入口。
rem @details 优先使用 Windows py 启动器，其次使用 python，最终执行同目录的 Python 启动脚本。
setlocal
cd /d "%~dp0"
set "VIEWER_SCRIPT=%~dp0start_ship_log_viewer.py"

where py >nul 2>nul
if %errorlevel%==0 (
  py -3 "%VIEWER_SCRIPT%"
  goto :eof
)

where python >nul 2>nul
if %errorlevel%==0 (
  python "%VIEWER_SCRIPT%"
  goto :eof
)

echo [viewer] Python launcher not found. Install Python or add py/python to PATH.
pause
