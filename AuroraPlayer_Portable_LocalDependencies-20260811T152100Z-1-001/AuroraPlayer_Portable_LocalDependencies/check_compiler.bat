@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "GXX=C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe"
echo Compiler:
echo !GXX!
echo.
if exist "!GXX!" (
  echo FOUND
  "!GXX!" --version
) else (
  echo NOT FOUND
)
pause
