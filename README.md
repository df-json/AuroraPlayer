# Aurora Player 2.0

A native, offline Spotify-inspired local music player built entirely in C++17.

## What is included

- Native SDL2 + Dear ImGui desktop UI
- miniaudio playback engine
- SQLite persistence
- TagLib metadata parsing for common MP3/FLAC/Ogg/MP4 formats
- Embedded album-art extraction to an application cache
- Recursive folder scanning in a background thread
- Search across songs, artists, albums and genres
- Play/pause/stop/seek/volume
- Previous/next playback
- Queue with add-next, remove and play-from-queue
- Shuffle playback order
- Repeat Off / All / One
- Local playlists with create/delete/add/remove
- Liked songs
- Recently played history with configurable threshold
- Artist and album library views
- Library sorting by name, artist, album, date added and play count
- Dark/light theme and UI scaling
- Application-level metadata editing without changing original audio files
- Missing-file reconciliation
- SQLite backup using SQLite's online backup API
- Keyboard controls
- CMake build

## Windows requirements

Install Visual Studio 2022 with Desktop development with C++, CMake 3.20+, and Git.

No Node.js, Python, Java, .NET, Qt, Electron, React or web browser runtime is required.

## Build

```text
cd CppMusicPlayer
build_windows.bat
```

Or:

```text
cmake -S . -B build
cmake --build build --config Release
```

Run:

```text
build\\Release\\CppMusicPlayer.exe
```

The first configure downloads the native C++ dependencies through CMake FetchContent, so the initial configure requires internet access. The finished player itself does not require internet access.

## First use

1. Launch Aurora Player.
2. Open Settings.
3. Add your music folder.
4. Wait for the background scan to finish.
5. Browse Songs, Albums or Artists.
6. Double-click a song to play it.

The database is stored at `data/music.db` relative to the working directory.
Album artwork extracted from files is cached in `.aurora_art` folders beside the corresponding music file.

## Metadata policy

Metadata is read from the audio file using TagLib. Editing metadata in Aurora only changes the application's SQLite record; the original audio file is not rewritten.

## Keyboard shortcuts

- Space: play/pause
- Left Arrow: seek backward 5 seconds
- Right Arrow: seek forward 5 seconds
- Up Arrow: volume up
- Down Arrow: volume down

## Architecture

```text
main.cpp
   |
   v
 App
   +--> UI --------------------+
   |                            |
   +--> AudioPlayer -> miniaudio|
   +--> MusicLibrary -> TagLib |
   +--> Database -> SQLite      |
   +--> Queue                   |
   +--> Settings               |
                                v
                           Local files
```

The project deliberately keeps the major systems in separate, readable C++ files instead of hiding the application behind a large framework.

## Portable g++ / school-computer build

If you have MinGW-w64 `g++` but cannot install CMake, use:

```text
build.bat
run.bat
```

See `PORTABLE_GPP.md` for details. The script downloads dependencies into the local `vendor/` directory instead of installing them system-wide.
