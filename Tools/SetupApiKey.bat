@echo off
chcp 65001 >nul
title MultiPlayerAction - NPC 对话 API Key 配置
echo ============================================================
echo  配置 NPC 对话所需的 Moonshot API Key
echo  说明：只有【开房间的主机】需要配置；加入别人房间无需配置。
echo  Key 获取：https://platform.moonshot.cn 注册后创建 API Key
echo ============================================================
echo.
set "KEY="
set /p KEY=请粘贴你的 MOONSHOT_API_KEY（输入不回显于日志）:
if not defined KEY (
    echo.
    echo [取消] 未输入任何内容，什么都没有改动。
    pause
    exit /b 1
)
setx MOONSHOT_API_KEY "%KEY%" >nul
if errorlevel 1 (
    echo.
    echo [失败] 写入环境变量失败，请以当前用户手动设置 MOONSHOT_API_KEY。
    pause
    exit /b 1
)
echo.
echo [完成] 已写入当前用户的环境变量 MOONSHOT_API_KEY。
echo 游戏会实时读取（含注册表回退），无需重启电脑；正在运行的游戏也会在下一次对话时生效。
echo.
pause
