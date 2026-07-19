@echo off
chcp 65001 >nul
title MultiPlayerAction - API Key Setup
echo ============================================================
echo  NPC dialogue API key setup (only the HOST needs this)
echo  Key from: https://platform.moonshot.cn
echo ============================================================
echo.
set "KEY="
set /p KEY=Paste your MOONSHOT_API_KEY: 
if not defined KEY (
    echo [Cancelled] Nothing entered, nothing changed.
    pause
    exit /b 1
)
setx MOONSHOT_API_KEY "%KEY%" >nul
if errorlevel 1 (
    echo [Failed] Could not write the environment variable.
    pause
    exit /b 1
)
echo.
echo [OK] MOONSHOT_API_KEY saved for the current user.
echo No restart needed - the game reads it on the next dialogue request.
echo.
pause
