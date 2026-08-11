# Aurora Player 2.0 — Portable g++ Build

This version is intended for a Windows computer where you can run `g++` but cannot install CMake, Visual Studio, Git, or system-wide libraries.

## What it does

`build.bat` builds the same Aurora Player source tree with MinGW/g++ instead of CMake.

It downloads third-party development files into the project's own `vendor/` folder. It does **not** install them system-wide.

The application remains native C++ using:

- SDL2
- Dear ImGui
- miniaudio
- SQLite
- TagLib
- stb_image

## Requirements

The computer needs:

1. Windows
2. `g++` and `gcc` in PATH
3. Internet access on the first build
4. Permission to run programs from the project folder

No CMake is required.

## Build

Open Command Prompt in this folder:

```text
build.bat
```

Run the program:

```text
run.bat
```

Or directly:

```text
build-gpp\AuroraPlayer.exe
```

## Tests

```text
build.bat test
```

## Clean build output

```text
build.bat clean
```

This removes `build-gpp` but keeps downloaded dependencies in `vendor`.

## Important: compiler compatibility

Use a **64-bit MinGW-w64** g++ compiler. The bundled binary libraries are from the MSYS2 MinGW64 environment and are intended for x86_64 MinGW-w64 toolchains.

Check your compiler with:

```text
g++ -dumpmachine
```

A typical compatible result contains:

```text
x86_64-w64-mingw32
```

If your school computer has a 32-bit compiler such as `i686-w64-mingw32`, this package will not be compatible without changing the dependency set.

## If the school computer has no internet

Run `build.bat` once on a Windows computer that has the same 64-bit MinGW-w64 family, then copy the whole project folder including `vendor/` to the school computer.

The source and dependency files remain inside the project directory.

## If `tar` cannot extract `.tar.zst`

The dependency packages for SDL2, TagLib, zlib, and libiconv use MSYS2 `.pkg.tar.zst` archives. Modern Windows installations normally provide `tar.exe`, but an older or restricted machine may not support this archive format.

In that case, the school computer's administrator may need to provide a newer `tar.exe`, or the dependency folder can be prepared on another computer and copied over.

## No system installation

The script intentionally avoids:

- `cmake --install`
- Registry changes
- System-wide library installation
- Package-manager installation
- Administrator privileges

The executable and runtime DLLs are kept in `build-gpp`.
