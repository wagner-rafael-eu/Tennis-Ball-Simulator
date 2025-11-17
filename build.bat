@echo off
REM Build script for Tennis Ball Physics Simulator
REM Requires Visual Studio Build Tools or Visual Studio installed

echo Building Tennis Ball Physics Simulator...

REM Try to find Visual Studio
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Visual Studio not found. Please run this from a Visual Studio Developer Command Prompt.
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Compile
cl.exe /EHsc /W4 /D_UNICODE /DUNICODE /std:c++17 ^
    /Fe:build\TennisBallSimulator.exe ^
    main.cpp ^
    /link d2d1.lib dwrite.lib user32.lib gdi32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Executable: build\TennisBallSimulator.exe
    echo Copying executable to project root...
    copy /Y build\TennisBallSimulator.exe TennisBallSimulator.exe
    echo.
    echo Run with: .\TennisBallSimulator.exe
) else (
    echo.
    echo Build failed!
    exit /b 1
)
