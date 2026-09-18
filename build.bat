@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
pushd "%SCRIPT_DIR%" >nul || exit /b 1

set "PRESET=%~1"
if not defined PRESET set "PRESET=windows-ninja-debug"
set "RUN_TESTS=%~2"

call :check_cmake || goto :fail

if /I "%PRESET:vs=%"=="%PRESET%" (
    call :enter_msvc_env || goto :fail
    if not defined CMAKE_GENERATOR (
        set "CMAKE_GENERATOR=Ninja"
    )
)

echo [AtriumCppTools] configure preset: %PRESET%
cmake --preset "%PRESET%" || goto :fail

echo [AtriumCppTools] build preset: build-%PRESET%
cmake --build --preset "build-%PRESET%" || goto :fail

if /I "%RUN_TESTS%"=="test" (
    echo [AtriumCppTools] running tests
    ctest --preset "test-%PRESET%" || goto :fail
)

echo [AtriumCppTools] done.
popd >nul
exit /b 0

:check_cmake
where cmake >nul 2>nul
if errorlevel 1 (
    echo [AtriumCppTools] cmake was not found on PATH.
    exit /b 1
)
exit /b 0

:enter_msvc_env
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [AtriumCppTools] Could not find vswhere.exe. Install Visual Studio Build Tools or Visual Studio.
    exit /b 1
)

set "VS_INSTALL="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL=%%I"
)

if not defined VS_INSTALL (
    echo [AtriumCppTools] Could not find a Visual Studio installation with MSVC x64 tools.
    exit /b 1
)

set "VCVARS=%VS_INSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [AtriumCppTools] Could not find vcvars64.bat under "%VS_INSTALL%".
    exit /b 1
)

echo [AtriumCppTools] MSVC environment: %VCVARS%
call "%VCVARS%" >nul || exit /b 1
exit /b 0

:fail
set "EXIT_CODE=%ERRORLEVEL%"
if not defined EXIT_CODE set "EXIT_CODE=1"
if "%EXIT_CODE%"=="0" set "EXIT_CODE=1"
popd >nul
exit /b %EXIT_CODE%
