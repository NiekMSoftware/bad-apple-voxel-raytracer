@echo off
setlocal

set "APP_NAME=raytracing-demo"
set "PRESET=msvc"
set "CONFIG=Release"

:: Resolve project root
set "ROOT=%~dp0."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

set "BUILD_DIR=%ROOT%\build\msvc"
set "EXE=%ROOT%\x64\%APP_NAME%\%CONFIG%\%APP_NAME%.exe"

echo.
echo [%APP_NAME%] Config    : %CONFIG%
echo [%APP_NAME%] Build dir : %BUILD_DIR%
echo [%APP_NAME%] Output    : %EXE%
echo.

:: -------------------------------------------------------
:: Step 1 - Configure (only if build directory is missing)
:: -------------------------------------------------------
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [%APP_NAME%] Configuring...
    cmake --preset "%PRESET%" -S "%ROOT%"
    if errorlevel 1 (
        echo [%APP_NAME%] ERROR: Configure failed.
        exit /b 1
    )
)

:: -------------------------------------------------------
:: Step 2 - Build
:: -------------------------------------------------------
echo [%APP_NAME%] Building...
cmake --build "%BUILD_DIR%" --config %CONFIG% --target %APP_NAME%
if errorlevel 1 (
    echo [%APP_NAME%] ERROR: Build failed.
    exit /b 1
)

:: -------------------------------------------------------
:: Step 3 - Run from the exe's own directory so that
::          relative asset paths resolve correctly
:: -------------------------------------------------------
if not exist "%EXE%" (
    echo [%APP_NAME%] ERROR: Executable not found: %EXE%
    exit /b 1
)

echo [%APP_NAME%] Running...
echo.
pushd "%ROOT%\x64\%APP_NAME%\%CONFIG%"
"%EXE%"
popd

endlocal