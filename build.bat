@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Aurora Player - Incremental Portable Build

echo.
echo ============================================================
echo           AURORA PLAYER - INCREMENTAL BUILD
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

set "TOOLCHAIN=%ROOT%toolchain\TDM-GCC-64"
set "GXX=%TOOLCHAIN%\bin\g++.exe"

if not exist "!GXX!" (
    echo ERROR: Bundled C++ compiler was not found.
    echo.
    echo Expected:
    echo !GXX!
    echo.
    pause
    exit /b 1
)

set "PATH=!TOOLCHAIN!\bin;!PATH!"

echo Compiler:
echo !GXX!
echo.

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
    pause
    exit /b 2
)

echo All dependencies found.
echo.

REM ============================================================
REM INCLUDE DIRECTORIES
REM ============================================================

set "INCLUDES=-I"!ROOT!include" -I"!TP!\SDL2\include" -I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\miniaudio" -I"!TP!\sqlite" -I"!TP!\stb" -I"!TP!\taglib\include" -I"!TP!\zlib\include""

REM ============================================================
REM SQLITE
REM ============================================================

echo.
echo ============================================================
echo                       SQLITE
echo ============================================================
echo.

if not exist "!BUILD!\sqlite3.o" (
    echo [BUILD] SQLite
    "!GXX!" -x c -std=c11 -O2 -fmax-errors=1 ^
        -c "!TP!\sqlite\sqlite3.c" ^
        -o "!BUILD!\sqlite3.o"

    if errorlevel 1 goto BUILD_FAILED
) else (
    for %%A in ("!TP!\sqlite\sqlite3.c") do set "SQLITE_SRC=%%~tA"
    for %%A in ("!BUILD!\sqlite3.o") do set "SQLITE_OBJ=%%~tA"

    echo [OK] SQLite object already exists.
)

REM ============================================================
REM DEAR IMGUI
REM ============================================================

echo.
echo ============================================================
echo                    DEAR IMGUI
echo ============================================================
echo.

call :CompileCppIfNeeded "!TP!\imgui\imgui.cpp" "!BUILD!\imgui.o" "-I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\SDL2\include""
if errorlevel 1 goto BUILD_FAILED

call :CompileCppIfNeeded "!TP!\imgui\imgui_draw.cpp" "!BUILD!\imgui_draw.o" "-I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\SDL2\include""
if errorlevel 1 goto BUILD_FAILED

call :CompileCppIfNeeded "!TP!\imgui\imgui_tables.cpp" "!BUILD!\imgui_tables.o" "-I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\SDL2\include""
if errorlevel 1 goto BUILD_FAILED

call :CompileCppIfNeeded "!TP!\imgui\imgui_widgets.cpp" "!BUILD!\imgui_widgets.o" "-I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\SDL2\include""
if errorlevel 1 goto BUILD_FAILED

call :CompileCppIfNeeded "!TP!\imgui\backends\imgui_impl_sdl2.cpp" "!BUILD!\imgui_sdl2.o" "-I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\SDL2\include""
if errorlevel 1 goto BUILD_FAILED

call :CompileCppIfNeeded "!TP!\imgui\backends\imgui_impl_sdlrenderer2.cpp" "!BUILD!\imgui_sdlrenderer2.o" "-I"!TP!\imgui" -I"!TP!\imgui\backends" -I"!TP!\SDL2\include""
if errorlevel 1 goto BUILD_FAILED

echo Dear ImGui checked.
echo.

REM ============================================================
REM AURORA PLAYER SOURCES
REM ============================================================

echo.
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

    call :CompileCppIfNeeded "!ROOT!src\%%F.cpp" "!BUILD!\%%F.o" "!INCLUDES!"

    if errorlevel 1 goto BUILD_FAILED
)

echo.
echo Aurora Player sources checked.
echo.

REM ============================================================
REM LINK
REM ============================================================

echo.
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

if exist "!TOOLCHAIN!\bin\libgcc_s_seh_64-1.dll" (
    copy /Y "!TOOLCHAIN!\bin\libgcc_s_seh_64-1.dll" "!ROOT!libgcc_s_seh_64-1.dll" >nul
)

if exist "!TOOLCHAIN!\bin\libstdc++_64-6.dll" (
    copy /Y "!TOOLCHAIN!\bin\libstdc++_64-6.dll" "!ROOT!libstdc++_64-6.dll" >nul
)

if exist "!TP!\SDL2\bin\SDL2.dll" (
    copy /Y "!TP!\SDL2\bin\SDL2.dll" "!ROOT!bin\SDL2.dll" >nul
)

if exist "!TP!\taglib\bin\libtag-2.dll" (
    copy /Y "!TP!\taglib\bin\libtag-2.dll" "!ROOT!bin\libtag-2.dll" >nul
)

if exist "!TP!\taglib\bin\libtag_c-2.dll" (
    copy /Y "!TP!\taglib\bin\libtag_c-2.dll" "!ROOT!bin\libtag_c-2.dll" >nul
)

if exist "!TP!\zlib\bin\zlib1.dll" (
    copy /Y "!TP!\zlib\bin\zlib1.dll" "!ROOT!bin\zlib1.dll" >nul
)

echo.
echo ============================================================
echo                  BUILD SUCCESSFUL
echo ============================================================
echo.
echo Executable:
echo !OUT!
echo.
echo ============================================================
echo.

exit /b 0


REM ============================================================
REM INCREMENTAL C++ COMPILATION FUNCTION
REM ============================================================

:CompileCppIfNeeded

set "SRC=%~1"
set "OBJ=%~2"
set "EXTRA=%~3"

if not exist "%OBJ%" (
    echo [BUILD] %~nx1
    "!GXX!" -std=c++17 -O2 -Wall -Wextra -fmax-errors=1 %EXTRA% -c "%SRC%" -o "%OBJ%"
    exit /b !errorlevel!
)

powershell -NoProfile -Command ^
    "$s=(Get-Item -LiteralPath '%SRC%').LastWriteTime; $o=(Get-Item -LiteralPath '%OBJ%').LastWriteTime; if($s -gt $o){exit 1}else{exit 0}"

if errorlevel 1 (
    echo [BUILD] %~nx1
    "!GXX!" -std=c++17 -O2 -Wall -Wextra -fmax-errors=1 %EXTRA% -c "%SRC%" -o "%OBJ%"
    exit /b !errorlevel!
)

echo [SKIP]  %~nx1
exit /b 0


REM ============================================================
REM BUILD FAILURE
REM ============================================================

:BUILD_FAILED

echo.
echo ============================================================
echo                  BUILD FAILED
echo ============================================================
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
pause
exit /b 1