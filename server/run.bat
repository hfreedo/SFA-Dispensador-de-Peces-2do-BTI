@echo off
title SFA - Dispensador de Peces 2do. BTI - Servidor
cd /d "%~dp0"

echo.
echo ============================================
echo  SFA - DISPENSADOR DE PECES  2do BTI
echo ============================================
echo.

where python >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] Python no esta instalado.
    echo Descargalo de https://www.python.org/downloads/
    echo Marca "Add Python to PATH" durante la instalacion.
    pause
    exit /b 1
)

echo Instalando dependencia (pyserial)...
pip install pyserial -q
echo.

echo Iniciando servidor web...
echo.
python server.py

pause
