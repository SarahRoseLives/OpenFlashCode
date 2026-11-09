@echo off
setlocal enableextensions

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Could not find vswhere.exe. Install Visual Studio Build Tools with the C++ workload.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo Could not find a Visual C++ toolchain.
    exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

if not exist build mkdir build

cl /nologo /EHsc /std:c++17 /O2 /W3 /DUNICODE /D_UNICODE ^
   src\main.cpp src\flashcode.cpp src\flashdata.cpp ^
   /Fo:build\ /Fe:build\OpenFlashCode.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED user32.lib gdi32.lib comctl32.lib
if errorlevel 1 exit /b 1

echo.
echo Built build\OpenFlashCode.exe

