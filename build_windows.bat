@echo off
setlocal
if not exist build mkdir build
cmake -S . -B build
if errorlevel 1 goto :error
cmake --build build --config Release
if errorlevel 1 goto :error
echo.
echo Build complete.
echo Run: build\Release\CppMusicPlayer.exe
exit /b 0
:error
echo.
echo Build failed. Read the error above.
exit /b 1
