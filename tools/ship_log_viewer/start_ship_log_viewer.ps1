$ErrorActionPreference = "Stop"
<#
.SYNOPSIS
本地船端串口日志查看器的一键启动入口。

.DESCRIPTION
切换到脚本所在目录后优先使用 Windows py 启动器，其次使用 python，
最终执行同目录的 start_ship_log_viewer.py。该脚本只负责启动工具页面，
不参与固件、无线协议或 MainLoop 控制链路。
#>
Set-Location -LiteralPath $PSScriptRoot
$viewerScript = Join-Path $PSScriptRoot "start_ship_log_viewer.py"

if (Get-Command py -ErrorAction SilentlyContinue) {
    py -3 $viewerScript
    exit $LASTEXITCODE
}

if (Get-Command python -ErrorAction SilentlyContinue) {
    python $viewerScript
    exit $LASTEXITCODE
}

Write-Host "[viewer] Python launcher not found. Install Python or add py/python to PATH."
