@echo off
setlocal
set "ROOT=%~dp0"
set "DEVCPP=C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin"
set "PATH=%ROOT%bin;%DEVCPP%;%PATH%"

if not exist "%ROOT%AuroraPlayer.exe" (
  echo AuroraPlayer.exe is not built yet.
  echo Run build.bat first.
  pause
  exit /b 1
)

"%ROOT%AuroraPlayer.exe"
