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
    echo Para ejecutar sin Python, descarga el ZIP portable de Releases:
    echo https://github.com/hfreedo/SFA-Dispensador-de-Peces-2do-BTI/releases/latest
    echo Extrae el ZIP y abre SFA_Dispensador_Peces_2BTI.exe.
    echo.
    echo Descargalo de https://www.python.org/downloads/
    echo Marca "Add Python to PATH" durante la instalacion.
    pause
    exit /b 1
)

echo Instalando dependencia (pyserial)...
python -m pip install -r requirements.txt -q
if errorlevel 1 (
    echo [ERROR] No se pudieron instalar las dependencias.
    pause
    exit /b 1
)
echo.

echo Iniciando servidor web...
echo.
python server.py

pause
