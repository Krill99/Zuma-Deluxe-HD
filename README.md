# Zuma-Deluxe-HD
A remake of the legendary 2003 PopCap game with improved textures and support for 16:9 aspect ratio. Made by [GalaxyShad](https://github.com/GalaxyShad) and [s4lat](https://github.com/s4lat).

![preview](https://user-images.githubusercontent.com/52833080/162574925-d6c9f657-c2e4-43e3-860d-412e60f14834.png)

> [!NOTE]
> **Since 2026-10-26 the project builds on Windows, macOS and Linux.**
>
> Tested on:
> - Windows 10 - x86_64
> - macOS 26.6.2 (25G83) - M1 ARM
> - Linux Mint (kernel 7.0.0) - x86_64
> - Arch Linux (kernel 7.2.11) - x86_64
>
> Thanks for testing on Linux: [@Adolf-Eleador-II](https://github.com/Adolf-Eleador-II), [@ilanufriev](https://github.com/ilanufriev), [@Tarat0r](https://github.com/Tarat0r)

> [!NOTE]
> **Audio**
>
> Sound and music are implemented with open-source code only: SDL2 audio output plus the public-domain `stb_vorbis` decoder. Music is pre-rendered to Ogg and follows game state (level theme, near-hole suspense, win, game over). No BASS, libopenmpt or codec DLL is needed to build or play. See [docs/AUDIO.md](docs/AUDIO.md).

## Gameplay video on YouTube
[![Gameplay video](https://img.youtube.com/vi/VZYsNUUAozM/0.jpg)](https://www.youtube.com/watch?v=VZYsNUUAozM)

## Media
![Zuma Deluxe HD screenshot](https://github.com/GalaxyShad/Zuma-Deluxe-HD/assets/52833080/a84a71f4-385a-4d01-b4ce-b9ccf47d6f85)
![Zuma Deluxe HD gameplay](https://github.com/GalaxyShad/Zuma-Deluxe-HD/assets/52833080/a006a96f-805f-46d1-ac8f-340524df3cdd)

## Latest Windows Release (2021)
 [⬇ [Stable] ZumaHD v0.1.0 Windows x86.zip ⬇](https://github.com/GalaxyShad/Zuma-Deluxe-HD/releases/download/v0.1.0/zuma-hd-v0.1.0-windows.zip) 

> [!NOTE]
> This build is from 2021 and is only for Windows x86. The current code has changed a lot since then, so to play the latest version build it from source.

## How to Build

Requirements: a C compiler (MSVC, GCC or Clang), [CMake](https://cmake.org/download/) 4.0 or newer, Git, and the platform SDL2 runtime libraries listed below.

[SDL2](https://github.com/libsdl-org/SDL/tree/SDL2) and [Expat](https://github.com/libexpat/libexpat) are git submodules and are built with the game. [stb_vorbis](https://github.com/nothings/stb) and [stb_image](https://github.com/nothings/stb) live in `thirdparty/stb`. Audio is decoded in-process, so no module player, codec DLL or BASS library is required — only SDL2's own runtime.

### Get the code

```
git clone --recurse-submodules https://github.com/Krill99/Zuma-Deluxe-HD.git
cd Zuma-Deluxe-HD
```

If you cloned without submodules:

```
git submodule update --init thirdparty/expat thirdparty/sdl2
```

`thirdparty/openmpt` is optional and only needed to regenerate the music assets; skip it unless you are changing them (see [docs/AUDIO.md](docs/AUDIO.md)).

### Windows

Visual Studio 2022 (or Build Tools) with the Desktop C++ workload is required; CMake picks the newest toolset it finds.

```
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The executable lands in `build\bin\Release\ZumaHD.exe` together with `content\`. `SDL2.dll` is produced by the build next to it — keep the two together.

### Linux

Install a compiler, CMake and SDL2's development headers. On Debian/Ubuntu:

```
sudo apt update
sudo apt install build-essential cmake git \
    libsdl2-dev libasound2-dev libpulse-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
    libxi-dev libxinerama-dev libwayland-dev libxkbcommon-dev \
    libudev-dev libgbm-dev
```

Then build, using the system SDL2 instead of the submodule (faster and avoids X11/Wayland header hunting):

```
git submodule update --init thirdparty/expat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DZUMA_USE_SYSTEM_SDL2=ON
cmake --build build --parallel
```

On Fedora use `sudo dnf install gcc cmake git SDL2-devel alsa-lib-devel` plus the matching `libX*-devel` packages, and `-DZUMA_USE_SYSTEM_SDL2=ON` likewise. Omit that flag to build the vendored SDL2 instead.

The binary lands in `build/bin/ZumaHD` beside a copy of `content/`.

### Run

Run from the output directory — `settings.txt` and `progress.bin` are written to the current working directory.

Windows:

```
cd build\bin\Release
ZumaHD.exe
```

Linux:

```
cd build/bin
./ZumaHD
```

Sound works out of the box on both platforms. `ZUMA_AUDIO=off` starts the game silently if a machine has no usable output device.

To jump straight into a level while testing:

```
ZumaHD.exe --level=3 --difficulty=0     # index is 0-based, so this is level 1-4
./ZumaHD --level=3 --difficulty=0
```


## Contributing
This is a hobby project, and any help is very welcome! I'm always happy to see pull requests - big or small.

Some ways to help:
* Fix a bug or improve the code
* Improve the audio further - module-player parity is discussed in [#18](https://github.com/GalaxyShad/Zuma-Deluxe-HD/issues/18)
* Test the game on your OS and tell how it went
* Report bugs or share ideas in [issues](https://github.com/GalaxyShad/Zuma-Deluxe-HD/issues)

Not sure where to start? Just open an issue or a draft pull request, and we'll figure it out together.

## Sources
Thanks to [hclxing](https://hclxing.wordpress.com/2017/06/27/reverse-engineering-zuma-deluxes-level-file/) for the article on how paths are stored in .dat files.

## License 
Source code is under MIT License, but all assets (in content folder) belong to [PopCap and EA games](https://www.ea.com/ea-studios/popcap)
