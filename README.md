# Open Image Viewer

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/22d2c9bc0fa149fcaf0b84e009839fa9)](https://app.codacy.com/gh/OpenImageViewer/OpenImageViewer?utm_source=github.com&utm_medium=referral&utm_content=OpenImageViewer/OpenImageViewer&utm_campaign=Badge_Grade_Dashboard)
[![Windows MSVC build](https://github.com/OpenImageViewer/OpenImageViewer/actions/workflows/build-windows.yaml/badge.svg)](https://github.com/OpenImageViewer/OpenImageViewer/actions/workflows/build-windows.yaml)

**Open Image Viewer** is a hardware-accelerated, open-source C++26 image viewer focused on accurate image presentation, fast navigation, and efficient keyboard-driven workflows.

It aims to present images accurately instead of simply displaying image data through the monitor color space.

For more information visit [www.openimageviewer.com](https://www.openimageviewer.com).

[A Word from the author](http://www.openimageviewer.com/#word)  
[Highlights and features](http://www.openimageviewer.com/#features)

![Selection rect demonstration with Open Image Viewer](https://i.ibb.co/NZXpb2W/cut.gif "Preview")

## Features

* Hardware-accelerated rendering with D3D11 and OpenGL renderer support.
* Fast folder browsing, sorting, slideshow playback, zooming, panning, and fullscreen viewing.
* Keyboard-first operation with the active key bindings available from F1.
* Image inspection tools including image information, texel grid, pixel inspection, and selection workflows.
* Common image actions such as crop, copy selection, paste image, rotation, flipping, and color correction.
* Codecs and third-party dependencies are built from source via repository submodules.

## Supported Platforms

Windows is the supported viewer target. 64-bit builds are the official release path. 32-bit builds may compile and run but are not part of the official release flow.

Non-Windows viewer clients are not implemented yet. Core library and test builds may be configured with `-DOIV_BUILD_CLIENT=OFF` when working outside Windows.

### Windows Runtime Notes

Windows 7 SP1, 8, 8.1, 10, and 11 are supported targets.

When using Windows 7 SP1, install:

* [KB2670838 - Windows 7 platform update](https://www.microsoft.com/en-us/download/details.aspx?id=36805)
* [KB4019990 - D3DCompiler_47](https://www.catalog.update.microsoft.com/Search.aspx?q=4019990)
* [Universal C runtime](https://support.microsoft.com/en-us/topic/update-for-universal-c-runtime-in-windows-c0514201-7fe6-95a3-b0a5-287930f3560c)

## Build From Source

### Prerequisites

* Git
* CMake 3.24 or newer
* Windows SDK
* One supported Windows build setup:
  * Ninja with a C++26-capable clang/clang-cl or MSVC toolchain
  * Visual Studio Build Tools 2026 or newer with MSVC

Recommended Windows development stack: CMake, Ninja, clang-cl, VS Code, and the Windows SDK. This stack does not require Visual Studio Build Tools or MSVC.

### Clone

```powershell
git clone --recursive --depth 1 https://github.com/OpenImageViewer/OpenImageViewer.git
cd OpenImageViewer
```

If the repository was cloned without submodules, initialize them before configuring:

```powershell
git submodule update --init --recursive
```

### Configure and Build

Run one of the following from an environment where the selected compiler and Windows SDK are available.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or use the Visual Studio generator:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The viewer executable and copied resources are generated under the build tree's `bin` directory.

## Packaging

For a release-style Windows package, run:

```powershell
.\publish.ps1
```

The publish script uses Ninja internally and requires 7-Zip when packaging is enabled.

## Tests

Tests are built by default with the main CMake configuration.

```powershell
cmake --build build --target tests
build\bin\tests.exe
```

## License

OIV is distributed under the [OpenImageViewer License](LICENSE.md).
