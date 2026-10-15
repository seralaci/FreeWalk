@echo off
rem Builds FreeWalk on Windows with MSVC: tests, DLL and analysis tool in one build.
rem Usage: build.cmd [Release|Debug]   (default: Release)
setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

rem MSVC environment (cl, link, Windows SDK) from the newest full C++ install found by vswhere.
rem No parenthesized block, because the parenthesis in %ProgramFiles(x86)% would close the block.
if defined VCToolsInstallDir goto :build
set "VSINSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
for /f "usebackq delims=" %%i in (`"%VSINSTALLER%\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :novs
rem vcvars itself also calls vswhere from the PATH.
set "PATH=%PATH%;%VSINSTALLER%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

:build
cd /d "%~dp0"
set BUILD=build\msvc-%CONFIG%
cmake -S . -B %BUILD% -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
cmake --build %BUILD% || exit /b 1
ctest --test-dir %BUILD% --output-on-failure || exit /b 1
echo.
echo Built DLL: %BUILD%\FreeWalk.dll
exit /b 0

:novs
echo Visual Studio with C++ tools was not found.
exit /b 1
