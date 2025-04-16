@echo off
REM Check if cl.exe (Visual Studio compiler) exists, otherwise try g++
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set CXX=cl
    set CXXFLAGS=/W4 /EHsc /I"../../include" /I"../.."
) else (
    set CXX=g++
    set CXXFLAGS=-Wall -Wextra -std=c++20 -I "../../include" -I "../.." -fPIC -shared
)

REM 
set JDK_PATH=C:\Program Files\Eclipse Adoptium\jdk-21.0.6.7-hotspot
set CXXFLAGS=%CXXFLAGS% -I"%JDK_PATH%\include" -I"%JDK_PATH%\include\win32"

echo Using compiler: %CXX%
echo Compiler flags: %CXXFLAGS%

REM Compile the module
%CXX% %CXXFLAGS% -c module.cpp -o module.o
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed!
    exit /b 1
)

echo Module compiled successfully!
