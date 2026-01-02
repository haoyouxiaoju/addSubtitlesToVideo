@echo off
setlocal

:: Define paths
set CMAKE_EXE="D:\download\qt\Tools\CMake_64\bin\cmake.exe"

:: Create build directory if it doesn't exist
if not exist build mkdir build

echo ==========================================
echo Configuring Project...
echo ==========================================
%CMAKE_EXE% -S . -B build -G "Visual Studio 17 2022" -A x64

if %errorlevel% neq 0 (
    echo Configuration failed!
    pause
    exit /b %errorlevel%
)

echo ==========================================
echo Building Project (Release)...
echo ==========================================
%CMAKE_EXE% --build build --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b %errorlevel%
)

echo ==========================================
echo Build Successful!
echo Executable is located in: build\Release\VideoSubtitleGenerator.exe
echo ==========================================
pause
