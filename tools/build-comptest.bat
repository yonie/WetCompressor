@echo off
REM Build and run comptest - measures CompEngine with no plugin and no host.
REM Run from the plugin repo root:  tools\build-comptest.bat
REM Mirrors WetEQ's tools\build-eqtest.bat, which existed while this did not.

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found at "%VCVARS%"
    exit /b 1
)
call "%VCVARS%" >nul

if not exist build-tools mkdir build-tools

cl /nologo /EHsc /O2 /std:c++17 /I WetCompressor\source ^
   /Fe:build-tools\comptest.exe /Fo:build-tools\ ^
   tools\comptest.cpp WetCompressor\source\compengine.cpp
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo.
build-tools\comptest.exe %*
exit /b %errorlevel%
