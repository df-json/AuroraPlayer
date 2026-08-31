```bat
@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Aurora Player - Portable Build

echo.
echo ============================================================
echo              AURORA PLAYER - PORTABLE BUILD
echo ============================================================
echo.

REM ============================================================
REM PROJECT PATHS
REM ============================================================

set "ROOT=%~dp0"
set "TP=%ROOT%third_party"
set "BUILD=%ROOT%build"
set "OUT=%ROOT%AuroraPlayer.exe"

REM ============================================================
REM BUNDLED COMPILER
REM ============================================================
REM
REM The complete TDM-GCC-64 folder must be inside:
REM
REM     toolchain\TDM-GCC-64\
REM
REM This build does NOT depend on Dev-C++, VS Code, or a
REM system-installed g++.
REM

set "TOOLCHAIN=%ROOT%toolchain\TDM-GCC-64"
set "GXX=%TOOLCHAIN%\bin\g++.exe"

if not exist "!GXX!" (
    echo ERROR: Bundled C++ compiler was not found.
    echo.
    echo Expected:
    echo !GXX!
    echo.
    echo Put the complete TDM-GCC-64 folder inside:
    echo.
    echo !ROOT!toolchain\
    echo.
    echo The project must contain:
    echo.
    echo toolchain\TDM-GCC-64\bin\g++.exe
    echo.
    pause
    exit /b 1
)

REM ============================================================
REM FORCE BUNDLED TOOLCHAIN TO BE USED
REM ============================================================

set "PATH=!TOOLCHAIN!\bin;!PATH!"

echo Compiler:
echo !GXX!
echo.

"!GXX!" --version

if errorlevel 1 (
    echo.
    echo ERROR: Bundled compiler could not be started.
    echo.
    pause
    exit /b 1
)

REM ============================================================
REM DIRECTORIES
REM ============================================================

if not exist "!BUILD!" mkdir "!BUILD!"
if not exist "!ROOT!bin" mkdir "!ROOT!bin"

REM ============================================================
REM DEPENDENCY CHECK
REM ============================================================

echo.
echo ============================================================
echo              CHECKING DEPENDENCIES
echo ============================================================
echo.

set "MISSING=0"

REM SDL2
if not exist "!TP!\SDL2\include\SDL.h" (
    echo [MISSING] SDL2 headers
    set "MISSING=1"
)

if not exist "!TP!\SDL2\lib\libSDL2.dll.a" (
    if not exist "!TP!\SDL2\lib\libSDL2.a" (
        echo [MISSING] SDL2 library
        set "MISSING=1"
    )
)

if not exist "!TP!\SDL2\bin\SDL2.dll" (
    echo [MISSING] SDL2.dll
    set "MISSING=1"
)

REM Dear ImGui
for %%F in (
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
) do (
    if not exist "!TP!\imgui\%%F" (
        echo [MISSING] imgui\%%F
        set "MISSING=1"
    )
)

if not exist "!TP!\imgui\backends\imgui_impl_sdl2.cpp" (
    echo [MISSING] imgui_impl_sdl2.cpp
    set "MISSING=1"
)

if not exist "!TP!\imgui\backends\imgui_impl_sdlrenderer2.cpp" (
    echo [MISSING] imgui_impl_sdlrenderer2.cpp
    set "MISSING=1"
)

REM miniaudio
if not exist "!TP!\miniaudio\miniaudio.h" (
    echo [MISSING] miniaudio.h
    set "MISSING=1"
)

REM SQLite
if not exist "!TP!\sqlite\sqlite3.c" (
    echo [MISSING] sqlite3.c
    set "MISSING=1"
)

if not exist "!TP!\sqlite\sqlite3.h" (
    echo [MISSING] sqlite3.h
    set "MISSING=1"
)

REM stb
if not exist "!TP!\stb\stb_image.h" (
    echo [MISSING] stb_image.h
    set "MISSING=1"
)

REM TagLib
if not exist "!TP!\taglib\include\taglib\fileref.h" (
    echo [MISSING] TagLib headers
    set "MISSING=1"
)

if not exist "!TP!\taglib\lib\libtag.dll.a" (
    if not exist "!TP!\taglib\lib\libtag.a" (
        echo [MISSING] TagLib library
        set "MISSING=1"
    )
)

if not exist "!TP!\taglib\bin\libtag-2.dll" (
    echo [MISSING] libtag-2.dll
    set "MISSING=1"
)

REM zlib
if not exist "!TP!\zlib\include\zlib.h" (
    echo [MISSING] zlib.h
    set "MISSING=1"
)

if not exist "!TP!\zlib\include\zconf.h" (
    echo [MISSING] zconf.h
    set "MISSING=1"
)

if not exist "!TP!\zlib\lib\libz.a" (
    if not exist "!TP!\zlib\lib\libz.dll.a" (
        echo [MISSING] zlib library
        set "MISSING=1"
    )
)

if not exist "!TP!\zlib\bin\zlib1.dll" (
    echo [MISSING] zlib1.dll
    set "MISSING=1"
)

if "!MISSING!"=="1" (
    echo.
    echo ============================================================
    echo              MISSING DEPENDENCIES
    echo ============================================================
    echo.
    echo Fix the missing files above before building.
    echo.
    pause
    exit /b 2
)

echo All dependencies found.
echo.

REM ============================================================
REM CLEAN BUILD
REM ============================================================

echo Cleaning previous object files...
del /q "!BUILD!\*.o" >nul 2>&1
echo.

REM ============================================================
REM COMMON INCLUDE DIRECTORIES
REM ============================================================

set "INCLUDES=-I"!ROOT!include" -I"!TP!\SDL2\include" -I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\miniaudio" -I"!TP!\sqlite" -I"!TP!\stb" -I"!TP!\taglib\include" -I"!TP!\zlib\include""

REM ============================================================
REM SQLITE
REM ============================================================

echo ============================================================
echo                    SQLITE
echo ============================================================
echo.

echo Compiling SQLite as C...

"!GXX!" -x c -std=c11 -O2 -fmax-errors=1 ^
    -c "!TP!\sqlite\sqlite3.c" ^
    -o "!BUILD!\sqlite3.o"

if errorlevel 1 goto BUILD_FAILED

echo SQLite OK.
echo.

REM ============================================================
REM DEAR IMGUI
REM ============================================================

echo ============================================================
echo                  DEAR IMGUI
echo ============================================================
echo.

"!GXX!" -std=c++17 -O2 -fmax-errors=1 ^
    -I"!TP!\imgui" ^
    -I"!TP!\imgui\backends" ^
    -I"!TP!\SDL2\include" ^
    -c "!TP!\imgui\imgui.cpp" ^
    -o "!BUILD!\imgui.o"

if errorlevel 1 goto BUILD_FAILED

"!GXX!" -std=c++17 -O2 -fmax-errors=1 ^
    -I"!TP!\imgui" ^
    -I"!TP!\imgui\backends" ^
    -I"!TP!\SDL2\include" ^
    -c "!TP!\imgui\imgui_draw.cpp" ^
    -o "!BUILD!\imgui_draw.o"

if errorlevel 1 goto BUILD_FAILED

"!GXX!" -std=c++17 -O2 -fmax-errors=1 ^
    -I"!TP!\imgui" ^
    -I"!TP!\imgui\backends" ^
    -I"!TP!\SDL2\include" ^
    -c "!TP!\imgui\imgui_tables.cpp" ^
    -o "!BUILD!\imgui_tables.o"

if errorlevel 1 goto BUILD_FAILED

"!GXX!" -std=c++17 -O2 -fmax-errors=1 ^
    -I"!TP!\imgui" ^
    -I"!TP!\imgui\backends" ^
    -I"!TP!\SDL2\include" ^
    -c "!TP!\imgui\imgui_widgets.cpp" ^
    -o "!BUILD!\imgui_widgets.o"

if errorlevel 1 goto BUILD_FAILED

"!GXX!" -std=c++17 -O2 -fmax-errors=1 ^
    -I"!TP!\imgui" ^
    -I"!TP!\imgui\backends" ^
    -I"!TP!\SDL2\include" ^
    -c "!TP!\imgui\backends\imgui_impl_sdl2.cpp" ^
    -o "!BUILD!\imgui_sdl2.o"

if errorlevel 1 goto BUILD_FAILED

"!GXX!" -std=c++17 -O2 -fmax-errors=1 ^
    -I"!TP!\imgui" ^
    -I"!TP!\imgui\backends" ^
    -I"!TP!\SDL2\include" ^
    -c "!TP!\imgui\backends\imgui_impl_sdlrenderer2.cpp" ^
    -o "!BUILD!\imgui_sdlrenderer2.o"

if errorlevel 1 goto BUILD_FAILED

echo Dear ImGui OK.
echo.

REM ============================================================
REM APPLICATION SOURCE
REM ============================================================

echo ============================================================
echo              AURORA PLAYER SOURCES
echo ============================================================
echo.

for %%F in (
    main
    App
    AudioPlayer
    Database
    MusicLibrary
    Search
    Queue
    Metadata
    Settings
    UI
) do (

    echo Compiling %%F.cpp...

    "!GXX!" -std=c++17 -O2 -Wall -Wextra -fmax-errors=1 ^
        !INCLUDES! ^
        -c "!ROOT!src\%%F.cpp" ^
        -o "!BUILD!\%%F.o"

    if errorlevel 1 goto BUILD_FAILED
)

echo.
echo Application sources OK.
echo.

REM ============================================================
REM LINK
REM ============================================================

echo ============================================================
echo                     LINKING
echo ============================================================
echo.

"!GXX!" -std=c++17 -O2 ^
    "!BUILD!\main.o" ^
    "!BUILD!\App.o" ^
    "!BUILD!\AudioPlayer.o" ^
    "!BUILD!\Database.o" ^
    "!BUILD!\MusicLibrary.o" ^
    "!BUILD!\Search.o" ^
    "!BUILD!\Queue.o" ^
    "!BUILD!\Metadata.o" ^
    "!BUILD!\Settings.o" ^
    "!BUILD!\UI.o" ^
    "!BUILD!\imgui.o" ^
    "!BUILD!\imgui_draw.o" ^
    "!BUILD!\imgui_tables.o" ^
    "!BUILD!\imgui_widgets.o" ^
    "!BUILD!\imgui_sdl2.o" ^
    "!BUILD!\imgui_sdlrenderer2.o" ^
    "!BUILD!\sqlite3.o" ^
    -L"!TP!\SDL2\lib" ^
    -L"!TP!\taglib\lib" ^
    -L"!TP!\zlib\lib" ^
    -mconsole ^
    -lSDL2 ^
    -ltag ^
    "!TP!\zlib\lib\libz.dll.a" ^
    -lshell32 ^
    -lole32 ^
    -lcomdlg32 ^
    -o "!OUT!"

if errorlevel 1 goto LINK_FAILED

REM ============================================================
REM COPY RUNTIME DLLS
REM ============================================================

echo.
echo Copying runtime files...
echo.

REM Application dependency DLLs
copy /Y "!TP!\SDL2\bin\SDL2.dll" "!ROOT!SDL2.dll" >nul

if exist "!TP!\taglib\bin\libtag-2.dll" (
    copy /Y "!TP!\taglib\bin\libtag-2.dll" "!ROOT!libtag-2.dll" >nul
)

if exist "!TP!\taglib\bin\libtag_c-2.dll" (
    copy /Y "!TP!\taglib\bin\libtag_c-2.dll" "!ROOT!libtag_c-2.dll" >nul
)

if exist "!TP!\zlib\bin\zlib1.dll" (
    copy /Y "!TP!\zlib\bin\zlib1.dll" "!ROOT!zlib1.dll" >nul
)

REM GCC runtime DLLs
if exist "!TOOLCHAIN!\bin\libgcc_s_seh_64-1.dll" (
    copy /Y "!TOOLCHAIN!\bin\libgcc_s_seh_64-1.dll" "!ROOT!libgcc_s_seh_64-1.dll" >nul
) else (
    echo [ERROR] libgcc_s_seh_64-1.dll not found.
    goto RUNTIME_FAILED
)

if exist "!TOOLCHAIN!\bin\libstdc++_64-6.dll" (
    copy /Y "!TOOLCHAIN!\bin\libstdc++_64-6.dll" "!ROOT!libstdc++_64-6.dll" >nul
) else (
    echo [ERROR] libstdc++_64-6.dll not found.
    goto RUNTIME_FAILED
)
if exist "!TOOLCHAIN!\bin\libstdc++_64-6.dll" (
    copy /Y "!TOOLCHAIN!\bin\libstdc++_64-6.dll" "!ROOT!libstdc++_64-6.dll" >nul
) else (
    echo [ERROR] libstdc++_64-6.dll not found in bundled toolchain.
    goto RUNTIME_FAILED
)

REM Keep bin folder for organized dependency copies too
copy /Y "!TP!\SDL2\bin\SDL2.dll" "!ROOT!bin\SDL2.dll" >nul

if exist "!TP!\taglib\bin\libtag-2.dll" (
    copy /Y "!TP!\taglib\bin\libtag-2.dll" "!ROOT!bin\libtag-2.dll" >nul
)

if exist "!TP!\taglib\bin\libtag_c-2.dll" (
    copy /Y "!TP!\taglib\bin\libtag_c-2.dll" "!ROOT!bin\libtag_c-2.dll" >nul
)

if exist "!TP!\zlib\bin\zlib1.dll" (
    copy /Y "!TP!\zlib\bin\zlib1.dll" "!ROOT!bin\zlib1.dll" >nul
)

REM ============================================================
REM SUCCESS
REM ============================================================

echo.
echo ============================================================
echo                  BUILD SUCCESSFUL
echo ============================================================
echo.
echo Executable:
echo.
echo   !OUT!
echo.
echo Runtime files:
echo.
echo   !ROOT!
echo.
echo Bundled compiler:
echo.
echo   !GXX!
echo.
echo ============================================================
echo.
echo Aurora Player is now built using the local project toolchain.
echo No system g++ installation is required.
echo.
pause
exit /b 0

REM ============================================================
REM COMPILATION FAILURE
REM ============================================================

REM ============================================================
REM RUNTIME FILE FAILURE
REM ============================================================

:RUNTIME_FAILED

echo.
echo ============================================================
echo              RUNTIME FILE COPY FAILED
echo ============================================================
echo.
echo A required GCC runtime DLL could not be copied.
echo.
pause
exit /b 1

:BUILD_FAILED

echo.
echo ============================================================
echo                  BUILD FAILED
echo ============================================================
echo.
echo The compiler stopped at the first error.
echo Review the error shown above.
echo.
pause
exit /b 1

REM ============================================================
REM LINK FAILURE
REM ============================================================

:LINK_FAILED

echo.
echo ============================================================
echo                   LINK FAILED
echo ============================================================
echo.
echo The source files compiled, but the executable could not
echo be linked.
echo.
pause
exit /b 1
```
