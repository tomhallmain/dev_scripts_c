@echo off
setlocal enabledelayedexpansion

:: Check if Visual Studio environment is set up
where cl.exe >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Visual Studio environment not found. Please run from a Visual Studio Developer Command Prompt
    echo or run the appropriate vcvarsall.bat script first.
    exit /b 1
)

:: Parse command line arguments
set BUILD_TYPE=debug
set WARNINGS=0

:parse_args
if "%~1"=="" goto :end_parse_args
if /i "%~1"=="--release" set BUILD_TYPE=release
if /i "%~1"=="--warnings" set WARNINGS=1
if /i "%~1"=="--help" goto :show_help
shift
goto :parse_args
:end_parse_args

:: Clean and set up build directory
echo Cleaning build directory...
if exist build rmdir /s /q build
mkdir build

:: Set compiler flags based on build type
if "%BUILD_TYPE%"=="release" (
    set CFLAGS=/O2 /DNDEBUG
    echo Building in RELEASE mode...
) else (
    set CFLAGS=/Zi /Od /UNDEBUG /D_DEBUG /RTC1 /MDd
    echo Building in DEBUG mode...
)

:: Add warning flags if requested
if %WARNINGS%==1 (
    set CFLAGS=%CFLAGS% /W4 /WX
) else (
    echo Note: Warnings are disabled. Use --warnings to enable compiler warnings.
)

:: Common flags
set INCLUDES=/Iinclude /Ilib
set LIBS=

:: Compile source files
echo.
echo Compiling core files...
for %%f in (src\core\*.c) do (
    echo Compiling %%f...
    cl.exe %CFLAGS% %INCLUDES% /c "%%f" /Fo"build\%%~nf.obj"
    if !ERRORLEVEL! neq 0 (
        echo Error: Failed to compile %%f
        exit /b 1
    )
)

echo.
echo Compiling command files...
for %%f in (src\commands\*.c) do (
    echo Compiling %%f...
    cl.exe %CFLAGS% %INCLUDES% /c "%%f" /Fo"build\%%~nf.obj"
    if !ERRORLEVEL! neq 0 (
        echo Error: Failed to compile %%f
        exit /b 1
    )
)

:: Link the executable
echo.
echo Linking...
link.exe /OUT:build\ds.exe build\*.obj %LIBS%
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to link executable
    exit /b 1
)

:: Clean up object files
del /Q build\*.obj

echo.
echo Build complete! Executable is in build\ds.exe
echo Build type: %BUILD_TYPE%
goto :eof

:show_help
echo Usage: build_msvc.bat [options]
echo Options:
echo   --release    Build in release mode (default: debug)
echo   --warnings   Enable compiler warnings (default: disabled)
echo   --help       Show this help message
exit /b 0 