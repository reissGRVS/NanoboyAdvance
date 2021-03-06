<h2>NanoboyAdvance</h2>

![logo](media/logo_cropped.png)

![license](https://img.shields.io/github/license/fleroviux/NanoboyAdvance)
[![CodeFactor](https://www.codefactor.io/repository/github/fleroviux/NanoboyAdvance/badge)](https://www.codefactor.io/repository/github/fleroviux/NanoboyAdvance)

NanoboyAdvance is a Nintendo Game Boy Advance (TM) emulator written in C++17.<br>
The goal is to create a minimal, accurate and reasonably efficient Game Boy Advance emulator in modern C++.

The emulator implements the core hardware completely and with high accuracy.
Almost all games can be emulated without any known issues, even the 'Classic NES' titles which abuse a
variety of edge-cases and undefined behaviour.

![screenshot1](media/screenshot1.png)

## Features

- very accurate GBA emulation
- RTC emulation
- high quality audio rendering
- game controller support
- basic GLSL shader support
- lightweight: minimal, configurable SDL2 frontend

## Compiling

NanoboyAdvance can be compiled on Windows, Linux and macOS (FreeBSD should work, but are not tested).
A modern C++17-capable compiler such as Clang/Clang-CL or G++ is mandatory.
MSVC is not supported and most definitely doesn't work in the default configuration (stack overflow while parsing templated code).

### Dependencies

There are a few dependencies that you need to get:
- CMake
- SDL2 development library
- GLEW

On Arch Linux:\
`pacman -S cmake sdl2 glew`

On an Ubuntu or Debian derived system:\
`apt install cmake libsdl2-dev libglew-dev`

On macOS:
- install macOS 10.15 SDK for OpenGL support
- SDL2: `brew install sdl2`
- GLEW: `brew install glew`

### Clone Git repository

Make sure to clone the repository with its submodules:
```
git clone --recurse-submodules https://github.com/fleroviux/NanoboyAdvance.git
```

### Setup CMake build environment

Setup the CMake build in a folder of your choice.

#### Linux
```
cd /somewhere/on/your/system/NanoboyAdvance
mkdir build
cd build
cmake ..
```
A final `make` then compiles the emulator.
The compiled executables then can be found in `build/source/platform/`.

#### macOS
As a workaround you may need to manually expose GLEW headers:
```bash
export CPPFLAGS="$CPPFLAGS -I/usr/local/opt/glew/include"
```
Otherwise the build process should be identical to Linux.

#### Windows
Setup [vcpkg](https://github.com/microsoft/vcpkg) and install the required libraries.
```
git clone https://github.com/microsoft/vcpkg
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install sdl2
vcpkg install glew
```

Generate the Visual Studio solution with CMake.
```
cd path/to/NanoboyAdvance
mkdir build
cd build
set VCPKG_ROOT=path/to/vcpkg
cmake -T clangcl ..
```

Build the Visual Studio solution. It can also be done via the command line.
```
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
msbuild NanoboyAdvance.sln
```

#### Miscellaneous

If you get an error regarding to `libc++fs` not being found, try commenting out `target_link_libraries(nba stdc++fs)` in `source/CMakeLists.txt`.

## Running

In order to run NanoboyAdvance you will need a BIOS file.
You can either dump your own or get an open source replacement online (https://github.com/Nebuleon/ReGBA/blob/master/bios/gba_bios.bin).
The BIOS file must be placed as `bios.bin` in the same folder as the executable. However keep in mind, that a replacement BIOS
will not be as accurate as the original one.

## Known issues
See https://github.com/fleroviux/NanoboyAdvance/issues

## Acknowledgement

- GBATEK by Martin Korth: a great piece of documentation that made this emulator possible.
- Talarubi & Near: The GBA color correction shader (https://byuu.net/video/color-emulation)
- mGBA by endrift: great test suite, also referenced its code for a few details.
- VisualBoyAdvance by Forgotten: inspired me to pursue emulator programming.
