@echo off
setlocal
set "REPO=%~dp0.."
cd /d "%REPO%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if "%UO_BUILD_DIR%"=="" set UO_BUILD_DIR=build
cmake -S . -B %UO_BUILD_DIR% -G Ninja -DCMAKE_BUILD_TYPE=%1 %UO_EXTRA_CMAKE%
if errorlevel 1 exit /b 1
cmake --build %UO_BUILD_DIR%
exit /b %errorlevel%
