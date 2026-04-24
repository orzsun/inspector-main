@echo off
setlocal
set "ROOT=%~dp0."
set "OUT=%ROOT%\build\Release"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

if not exist "%ROOT%\build" mkdir "%ROOT%\build"
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++20 /EHsc /W4 /O2 /I "%ROOT%\src" ^
  "%ROOT%\src\main.cpp" ^
  "%ROOT%\src\LogParser.cpp" ^
  /Fe:"%OUT%\packet.exe"
if errorlevel 1 exit /b 1

endlocal
