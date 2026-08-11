# Aurora Player — Local dependency setup

This version uses the existing Embarcadero Dev-C++ TDM-GCC 9.2.0 compiler on the school PC. It does **not** install anything into Windows.

The idea is exactly what you requested: third-party dependencies live inside this project.

## Required folder layout

```text
third_party/
├── SDL2/
│   ├── include/
│   ├── lib/
│   └── bin/
├── imgui/
│   ├── imgui.cpp
│   ├── imgui.h
│   ├── imgui_draw.cpp
│   ├── imgui_tables.cpp
│   ├── imgui_widgets.cpp
│   └── backends/
├── miniaudio/
│   └── miniaudio.h
├── sqlite/
│   ├── sqlite3.c
│   └── sqlite3.h
├── stb/
│   └── stb_image.h
└── taglib/
    ├── include/
    ├── lib/
    └── bin/
```

## 1. SDL2

Download the **MinGW development package**, not the Visual C++ package.

The official SDL release index lists `SDL2-devel-2.30.9-mingw.zip`. urlSDL2 release indexhttps://www.libsdl.org/release/

Extract its `x86_64-w64-mingw32` contents into:

```text
third_party\SDL2\
```

You should end up with:

```text
third_party\SDL2\include\SDL.h
third_party\SDL2\lib\libSDL2.dll.a
third_party\SDL2\lib\libSDL2main.a
third_party\SDL2\bin\SDL2.dll
```

Do NOT use the `VC` package; this build uses MinGW.

## 2. Dear ImGui

Get the Dear ImGui source matching the project (the CMake project originally used v1.92.2b).

Copy these into:

```text
third_party\imgui\
```

and the SDL2/SDL renderer backends into:

```text
third_party\imgui\backends\
```

Required files include:

```text
imgui.cpp
imgui.h
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
backends\imgui_impl_sdl2.cpp
backends\imgui_impl_sdl2.h
backends\imgui_impl_sdlrenderer2.cpp
backends\imgui_impl_sdlrenderer2.h
```

## 3. miniaudio

Copy:

```text
miniaudio.h
```

to:

```text
third_party\miniaudio\miniaudio.h
```

It is a single-header dependency and is compiled by `AudioPlayer.cpp`.

## 4. SQLite

Download the SQLite amalgamation and copy:

```text
sqlite3.c
sqlite3.h
```

to:

```text
third_party\sqlite\
```

The build compiles `sqlite3.c` locally, so SQLite is not installed on Windows.

## 5. stb_image

Copy:

```text
stb_image.h
```

to:

```text
third_party\stb\
```

## 6. TagLib

TagLib is the one dependency that is best prepared on a normal development PC rather than built on the restricted school machine.

The current MinGW package is available through MSYS2's MinGW64 repository, and TagLib 2.2.1 is currently packaged there. It has dependencies including zlib. citeturn2search0

For this project, prepare a **64-bit MinGW-compatible TagLib** bundle containing:

```text
third_party\taglib\include\taglib\...
third_party\taglib\lib\libtag.dll.a
third_party\taglib\bin\tag.dll
```

and any runtime DLLs required by that TagLib build.

Do not use the ancient 32-bit TagLib Windows port found on SourceForge; it is from 2005 and is not appropriate for this 64-bit GCC 9.2 build. citeturn5search2

## Build

Once all folders are populated:

```bat
build.bat
```

Then:

```bat
run.bat
```

## Important

If the school PC cannot download files, prepare the `third_party` folder on another computer and copy the **entire project folder** over USB/cloud storage, subject to your school's rules.

No CMake is required.
No administrator privileges are required.
No system-wide SDL2/TagLib/SQLite installation is required.
