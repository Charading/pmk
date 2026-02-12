@echo off
title PMK Terminal
color 0A

:: Set up paths to local toolchain and Pico extension
set "PMK_DIR=%~dp0"
set "PATH=%PMK_DIR%.toolchain\arm-gcc\bin;%PATH%"
set "PATH=%PMK_DIR%.toolchain\cmake\bin;%PATH%"
set "PATH=%PMK_DIR%.toolchain\ninja;%PATH%"

:: Also check Pico VS Code extension paths
if exist "%USERPROFILE%\.pico-sdk\toolchain" (
    for /d %%V in ("%USERPROFILE%\.pico-sdk\toolchain\*") do set "PATH=%%V\bin;%PATH%"
)
if exist "%USERPROFILE%\.pico-sdk\cmake" (
    for /d %%V in ("%USERPROFILE%\.pico-sdk\cmake\*") do set "PATH=%%V\bin;%PATH%"
)
if exist "%USERPROFILE%\.pico-sdk\ninja" (
    for /d %%V in ("%USERPROFILE%\.pico-sdk\ninja\*") do set "PATH=%%V;%PATH%"
)

:: Auto-detect Pico SDK
if not defined PICO_SDK_PATH (
    if exist "%PMK_DIR%.toolchain\pico-sdk" (
        set "PICO_SDK_PATH=%PMK_DIR%.toolchain\pico-sdk"
    ) else if exist "%USERPROFILE%\.pico-sdk\sdk\2.2.0" (
        set "PICO_SDK_PATH=%USERPROFILE%\.pico-sdk\sdk\2.2.0"
    ) else if exist "%USERPROFILE%\.pico-sdk\sdk\2.1.1" (
        set "PICO_SDK_PATH=%USERPROFILE%\.pico-sdk\sdk\2.1.1"
    )
)

:: Create doskey alias so 'pmk' works without 'python'
doskey pmk=python "%PMK_DIR%pmk.py" $*

cd /d "%PMK_DIR%"

echo.
echo   ____  __  __ _  __
echo  ^|  _ \^|  \/  ^| ^|/ /
echo  ^| ^|_) ^| ^|\/^| ^| ' /
echo  ^|  __/^| ^|  ^| ^| . \
echo  ^|_^|   ^|_^|  ^|_^|_^|\_\
echo.
echo   Pico Magnetic Keyboard
echo   ===================
echo.
echo   Commands:
echo     pmk build -kb ^<board^>     Build firmware
echo     pmk flash -kb ^<board^>     Build + flash
echo     pmk new   -kb ^<board^>     Create new board
echo     pmk list                  List boards
echo     pmk doctor                Check toolchain
echo     pmk setup                 Install toolchain
echo.

cmd /k
