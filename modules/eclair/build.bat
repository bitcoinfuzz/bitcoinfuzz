@echo off
set CXX=g++
set CXXFLAGS=-Wall -Wextra -std=c++20 -I ../../include -I ../.. -fPIC -shared
set LDFLAGS=-ljvm

REM Path to JDK - adjust this to your JDK location
set JDK_PATH=C:\Program Files\Java\jdk-17
set CXXFLAGS=%CXXFLAGS% -I"%JDK_PATH%\include" -I"%JDK_PATH%\include\win32"

REM Compile the module
%CXX% %CXXFLAGS% -c module.cpp -o module.o

REM Create the archive
C:\MinGW\bin\ar rcs module.a module.o

echo Module compiled successfully!

