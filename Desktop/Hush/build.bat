@echo off
REM KhDetector Build Script for Windows
REM Cross-platform VST3 Plugin Builder

setlocal enabledelayedexpansion

echo KhDetector VST3 Plugin Build Script
echo ===================================

REM Default values
set BUILD_TYPE=Debug
set CLEAN_BUILD=false
set INSTALL_PLUGIN=false
set RUN_TESTS=false
set GENERATOR="Visual Studio 17 2022"

REM Parse command line arguments
:parse_args
if "%~1"=="" goto end_parse
if /I "%~1"=="-r" set BUILD_TYPE=Release& shift & goto parse_args
if /I "%~1"=="--release" set BUILD_TYPE=Release& shift & goto parse_args
if /I "%~1"=="-d" set BUILD_TYPE=Debug& shift & goto parse_args
if /I "%~1"=="--debug" set BUILD_TYPE=Debug& shift & goto parse_args
if /I "%~1"=="-c" set CLEAN_BUILD=true& shift & goto parse_args
if /I "%~1"=="--clean" set CLEAN_BUILD=true& shift & goto parse_args
if /I "%~1"=="-i" set INSTALL_PLUGIN=true& shift & goto parse_args
if /I "%~1"=="--install" set INSTALL_PLUGIN=true& shift & goto parse_args
if /I "%~1"=="-t" set RUN_TESTS=true& shift & goto parse_args
if /I "%~1"=="--test" set RUN_TESTS=true& shift & goto parse_args
if /I "%~1"=="-h" goto show_help
if /I "%~1"=="--help" goto show_help
echo Unknown option: %~1
goto error_exit

:show_help
echo Usage: %0 [OPTIONS]
echo Options:
echo   -r, --release     Build in Release mode (default: Debug)
echo   -d, --debug       Build in Debug mode
echo   -c, --clean       Clean build directory before building
echo   -i, --install     Install plugin to system directory after building
echo   -t, --test        Run unit tests after building
echo   -h, --help        Show this help message
exit /b 0

:end_parse

echo Build Configuration:
echo   Build Type: %BUILD_TYPE%
echo   Clean Build: %CLEAN_BUILD%
echo   Install Plugin: %INSTALL_PLUGIN%
echo   Run Tests: %RUN_TESTS%
echo.

REM Check for required tools
where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake is not installed or not in PATH
    echo Please install CMake 3.20 or later
    goto error_exit
)

where git >nul 2>nul
if errorlevel 1 (
    echo Error: Git is not installed or not in PATH
    goto error_exit
)

REM Initialize submodules if needed
if not exist "external\vst3sdk\CMakeLists.txt" (
    echo Initializing VST3 SDK submodule...
    git submodule update --init --recursive
    if errorlevel 1 goto error_exit
)

REM Clean build directory if requested
if /I "%CLEAN_BUILD%"=="true" (
    echo Cleaning build directory...
    if exist build rmdir /s /q build
)

REM Create build directory
if not exist build mkdir build

REM Configure
echo Configuring CMake...
cmake -B build -S . -G %GENERATOR% -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 goto error_exit

REM Build
echo Building KhDetector...
cmake --build build --config %BUILD_TYPE% --parallel
if errorlevel 1 goto error_exit

echo Build completed successfully!

REM Find the built plugin
set PLUGIN_PATH=build\%BUILD_TYPE%\KhDetector.vst3
set INSTALL_PATH=%USERPROFILE%\AppData\Roaming\VST3\KhDetector.vst3

if exist "%PLUGIN_PATH%" (
    echo Plugin built at: %PLUGIN_PATH%
    
    REM Install if requested
    if /I "%INSTALL_PLUGIN%"=="true" (
        echo Installing plugin...
        if not exist "%USERPROFILE%\AppData\Roaming\VST3" mkdir "%USERPROFILE%\AppData\Roaming\VST3"
        copy "%PLUGIN_PATH%" "%INSTALL_PATH%" >nul
        if errorlevel 1 (
            echo Warning: Failed to install plugin
        ) else (
            echo Plugin installed to: %INSTALL_PATH%
        )
    )
    
    REM Run tests if requested
    if /I "%RUN_TESTS%"=="true" (
        echo Running unit tests...
        where ctest >nul 2>nul
        if not errorlevel 1 (
            cd build
            ctest --verbose --output-on-failure
            if not errorlevel 1 (
                echo All tests passed!
            ) else (
                echo Some tests failed!
            )
            cd ..
        ) else (
            echo Warning: CTest not found, running tests directly...
            if exist "build\%BUILD_TYPE%\KhDetectorTests.exe" (
                "build\%BUILD_TYPE%\KhDetectorTests.exe"
            ) else (
                echo Test executable not found!
            )
        )
    )
) else (
    echo Warning: Plugin file not found at expected location
)

echo Build complete!
goto end

:error_exit
echo Build failed!
exit /b 1

:end
endlocal 