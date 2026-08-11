# School PC setup

Your confirmed compiler is:

`C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe`

GCC 9.2.0, 64-bit.

This build does not require CMake and does not modify PATH permanently.

The only remaining task before the first full build is to populate `third_party/` with the required local dependencies. See `DEPENDENCIES.md`.

After that:

```bat
build.bat
run.bat
```
