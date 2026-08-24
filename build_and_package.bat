@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   GTAHUB-Addons - Build and Package All Artifacts
echo =======================================================

set ROOT_DIR=%~dp0
set OUTPUT_DIR=%ROOT_DIR%output

echo [1/4] Preparing output directory: %OUTPUT_DIR%
if exist "%OUTPUT_DIR%" rd /s /q "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%\client"
mkdir "%OUTPUT_DIR%\server\components"
mkdir "%OUTPUT_DIR%\server\pawn\include"

echo [2/4] Building Client (HUB-Core Release^|Win32)...
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD_EXE=%%i"
)

if not defined MSBUILD_EXE (
    if exist "D:\VS2019\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_EXE=D:\VS2019\MSBuild\Current\Bin\MSBuild.exe"
    ) else (
        echo [ERROR] MSBuild.exe not found!
        exit /b 1
    )
)

"%MSBUILD_EXE%" "%ROOT_DIR%HUB-Core\HUB-Core.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:m
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Client build failed!
    exit /b %ERRORLEVEL%
)

echo [3/4] Building Server Component (HUB-ServerComponent Release Win32/x86)...
cmake -B "%ROOT_DIR%HUB-ServerComponent\build" -S "%ROOT_DIR%HUB-ServerComponent" -A Win32
cmake --build "%ROOT_DIR%HUB-ServerComponent\build" --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Server component build failed!
    exit /b %ERRORLEVEL%
)

echo [4/4] Copying build artifacts to %OUTPUT_DIR%...
copy /y "%ROOT_DIR%HUB-Core\Release\HUB-Core.asi" "%OUTPUT_DIR%\client\HUB-Core.asi"
if exist "%ROOT_DIR%HUB-Core\Release\HUB-Core.pdb" copy /y "%ROOT_DIR%HUB-Core\Release\HUB-Core.pdb" "%OUTPUT_DIR%\client\HUB-Core.pdb"
copy /y "%ROOT_DIR%HUB-ServerComponent\build\Release\HUB-ServerComponent.dll" "%OUTPUT_DIR%\server\components\HUB-ServerComponent.dll"
copy /y "%ROOT_DIR%HUB-ServerComponent\pawn\hubcore_role.inc" "%OUTPUT_DIR%\server\pawn\include\hubcore_role.inc"
copy /y "%ROOT_DIR%HUB-ServerComponent\pawn\test_hub_role.pwn" "%OUTPUT_DIR%\server\pawn\test_hub_role.pwn"

echo.
echo =======================================================
echo   BUILD ^& PACKAGING COMPLETED SUCCESSFULLY!
echo   Output located at: %OUTPUT_DIR%
echo =======================================================
dir /s /b "%OUTPUT_DIR%"

endlocal
