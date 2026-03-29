:: ============================================================================
:: _common.bat - Shared logic for all build+run scripts
::
:: Sets up the build directory, config, and executable path, then
:: invokes CMake to build the target and runs it immediately after.
::
:: Parameters (set by caller before calling this):
::   %APP_NAME%      - e.g. primitives
::   %PRESET%        - e.g. msvc-primitives
::   %CONFIG%        - Debug | Release | RelWithDebInfo
:: ============================================================================

@echo off
setlocal

:: Resolve project root (one level up from scripts/)
set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build\msvc"

:: Determine executable name — Debug builds get a _debug postfix
if /i "%CONFIG%"=="Debug" (
    set "EXE=%ROOT%\x64\%APP_NAME%\Debug\%APP_NAME%_debug.exe"
) else (
    set "EXE=%ROOT%\x64\%APP_NAME%\%CONFIG%\%APP_NAME%.exe"
)

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
:: Step 3 - Run
:: -------------------------------------------------------
if not exist "%EXE%" (
    echo [%APP_NAME%] ERROR: Executable not found: %EXE%
    exit /b 1
)

echo [%APP_NAME%] Running...
echo.
"%EXE%"

endlocal