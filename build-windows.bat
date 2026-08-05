@echo off
REM ============================================================================
REM  build-windows.bat  --  Build the Key Detector VST3 on Windows with MSVC.
REM
REM  Requirements (one-time):
REM    * Visual Studio 2022 (Community is free) OR "Build Tools for Visual Studio
REM      2022" with the "Desktop development with C++" workload.
REM    * CMake 3.22+  (https://cmake.org/download/ or bundled with Visual Studio)
REM    * Git          (needed the first time to download JUCE)
REM
REM  Just double-click this file, or run it from a terminal in the project folder.
REM  The build downloads JUCE 8.0.9 automatically on the first run.
REM ============================================================================

setlocal

echo.
echo === Configuring (Visual Studio 2022, x64) ===
cmake -B build -G "Visual Studio 17 2022" -A x64 -DKEYDETECTOR_BUILD_TESTS=OFF
if errorlevel 1 goto :error

echo.
echo === Building Release VST3 ===
cmake --build build --config Release --target KeyDetector_VST3
if errorlevel 1 goto :error

echo.
echo ============================================================================
echo  BUILD OK.
echo  VST3 is here:
echo    build\KeyDetector_artefacts\Release\VST3\Key Detector.vst3
echo.
echo  To install for Ableton Live, copy that "Key Detector.vst3" folder into:
echo    C:\Program Files\Common Files\VST3\
echo  then in Live: Preferences -^> Plug-Ins -^> enable VST3, and Rescan.
echo ============================================================================
goto :eof

:error
echo.
echo *** BUILD FAILED. Scroll up for the first error. ***
echo Most common cause: Visual Studio C++ tools or CMake not installed / not on PATH.
exit /b 1
