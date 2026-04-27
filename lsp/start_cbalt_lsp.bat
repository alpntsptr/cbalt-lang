@echo off
setlocal
set SCRIPT_DIR=%~dp0
set SCRIPT_PATH=%SCRIPT_DIR%cbalt_lsp.py
set COMPILER_PATH=%SCRIPT_DIR%..\bin\cbalt.exe

where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%SCRIPT_PATH%" --compiler "%COMPILER_PATH%"
    exit /b %errorlevel%
)

where python >nul 2>nul
if %errorlevel%==0 (
    python "%SCRIPT_PATH%" --compiler "%COMPILER_PATH%"
    exit /b %errorlevel%
)

if exist "C:\laragon\bin\python\python-3.13\python.exe" (
    "C:\laragon\bin\python\python-3.13\python.exe" "%SCRIPT_PATH%" --compiler "%COMPILER_PATH%"
    exit /b %errorlevel%
)

echo Python tidak ditemukan. Jalankan langsung dengan interpreter Python yang ada di mesin kamu.
exit /b 1
