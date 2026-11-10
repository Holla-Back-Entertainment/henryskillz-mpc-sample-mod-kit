@echo off
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel% equ 0 (
  py -3 tools\easy_builder.py
) else (
  python tools\easy_builder.py
)
echo.
pause
