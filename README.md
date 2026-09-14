# Open Image Viewer

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/22d2c9bc0fa149fcaf0b84e009839fa9)](https://app.codacy.com/gh/OpenImageViewer/OpenImageViewer)
[![Windows build](https://github.com/OpenImageViewer/OpenImageViewer/actions/workflows/build-windows.yaml/badge.svg)](https://github.com/OpenImageViewer/OpenImageViewer/actions/workflows/build-windows.yaml)

**Open Image Viewer (OIViewer)** is a hardware-accelerated C++26 image viewer for Windows and Linux, built for accurate display, fast browsing, and keyboard-driven use.

Simplicity and performance guide its design. OIViewer embraces modern operating systems, graphics APIs, and C++ features, adopting technologies that are generally available on its CI servers.

[Website](https://www.openimageviewer.com) · [Downloads](https://github.com/OpenImageViewer/OpenImageViewer/releases) · [A word from the author](https://www.openimageviewer.com/#word)

![Selection demonstration](https://i.ibb.co/NZXpb2W/cut.gif)

## Features

- Fast browsing with read-ahead loading, sorting, and slideshows.
- Zoom, pan, fullscreen, automatic scrolling, and high-DPI support.
- Animated GIF, APNG, and WebP playback, plus subimage navigation.
- Texel grids, pixel values, image properties, and EXIF metadata.
- Gamma, exposure, saturation, grayscale, transparency, and filtering controls.
- Selection, cropping, rotation, flipping, clipboard support, and saving.
- Direct3D 11, Vulkan, and OpenGL backends with automatic GPU selection.
- Configurable keyboard shortcuts and commands; press **F1** for active bindings.

### Image formats

- **Common images:** JPEG (JPG/JPEG/JIF/JPE), PNG/APNG, GIF, WebP, TIFF (TIF/TIFF), and Windows/OS/2 BMP.
- **Documents, textures, and icons:** Photoshop PSD/PSB, DirectDraw Surface DDS, Windows icons (ICO/ICON), and cursors (CUR).
- **Other raster formats:** JPEG XR (JXR/WDP/HDP), Kodak Photo CD (PCD), ZSoft Paintbrush (PCX), portable graymaps (PGM/PGMRAW) and pixmaps (PPM/PPMRAW) in ASCII or binary form, Targa (TGA/TARGA), Sun Raster (RAS), Amiga IFF/LBM, SGI (SGI/RGB/RGBA/BW), Macintosh PICT (PICT/PCT/PIC), Dr. Halo (CUT), and X11 Pixmap (XPM).
- **Camera RAW:** the RAW codec lists 3FR, ARW, BAY, BMQ, CAP, CINE, CR2, CRW, CS1, DC2, DCR, DRF, DSC, DNG, ERF, FFF, IA, IIQ, K25, KC2, KDC, MDC, MEF, MOS, MRW, NEF, NRW, ORF, PEF, PTX, PXN, QTK, RAF, RAW, RDC, RW2, RWL, RWZ, SR2, SRF, SRW, STI, and X3F. The bundled LibRaw version also implements Canon CR3 decoding. Compatibility depends on the camera model and encoding.
- **Save:** JPEG (JPG/JPEG) and PNG.

## Getting started

Download a package from [GitHub Releases](https://github.com/OpenImageViewer/OpenImageViewer/releases), extract it, and launch OIViewer. Press **Ctrl+O** or pass an image or folder on the [command line](#command-line).

Scroll to zoom, right-drag to pan, **Alt+left-drag** to select, and **Shift+scroll** to browse images. **Alt+scroll** switches subimages; middle-click toggles automatic scrolling.

### Common shortcuts

These are the shipped defaults; press **F1** for the full list of active bindings.

| Shortcut | Action |
| --- | --- |
| **Ctrl+O** | Open an image |
| **Left / Right** or **Page Up / Page Down** | Previous / next image in the folder |
| **Home / End** | First / last image in the folder |
| **Space** | Start or stop the slideshow |
| **Alt+Enter** | Toggle fullscreen |
| **Numpad \* / Numpad /** | Original size / fit to window |
| **G** | Toggle the texel grid |
| **Grave (`)** | Toggle image information |
| **Ctrl+C / Ctrl+V** | Copy the selected area / paste an image |
| **C** | Crop to the selected area |
| **H / V** | Flip horizontally / vertically |
| **Ctrl+S** | Save an image |
| **Ctrl+F1 / Ctrl+F2 / Ctrl+F3** | Sort by name / modification date / extension |

## Command line

```text
OIViewer "path/to/image-or-folder"
OIViewer --renderer Vulkan "photo.jpg"
OIViewer --help
```

With no graphics options, **Windows prefers D3D11, then Vulkan**, and **Linux prefers Vulkan, then GL**. These are also the renderers included in default builds. Windows can include OpenGL as a final choice.

## Configuration

Edit the files in `Resources/Configuration` beside the executable, then restart OIViewer:

- [Settings.json](Clients/OIViewer/Resources/Configuration/Settings.json): viewing and browsing settings.
- [KeyBindings.json](Clients/OIViewer/Resources/Configuration/KeyBindings.json): keyboard shortcuts.
- [Commands.json](Clients/OIViewer/Resources/Configuration/Commands.json): command definitions.

Automatic reloading of externally modified images is supported on Windows only.

## Runtime requirements

64-bit Windows or Linux x86_64, with a driver supporting Direct3D 11, Vulkan 1.1+, or OpenGL 3.0+, as included in the build.

- **Windows:** 7 SP1, 8, 8.1, 10, or 11. Windows 11 24H2 or newer is recommended.
- **Linux:** official binaries require glibc 2.39+ and a Wayland desktop.

Windows 7 SP1 also requires the [platform update](https://www.microsoft.com/en-us/download/details.aspx?id=36805), [D3DCompiler_47](https://www.catalog.update.microsoft.com/Search.aspx?q=4019990), and [Universal C runtime](https://support.microsoft.com/en-us/topic/update-for-universal-c-runtime-in-windows-c0514201-7fe6-95a3-b0a5-287930f3560c).

## Build from source

Use Git, CMake 3.24+, Ninja, and a C++26-capable toolchain. **Clang/clang-cl 21 or newer** is recommended.

```sh
git clone --recursive https://github.com/OpenImageViewer/OpenImageViewer.git
cd OpenImageViewer
```

For an existing checkout, initialize dependencies with `git submodule update --init --recursive`.

### Windows

Install the Windows SDK and use an x64 Visual Studio developer shell:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build --parallel
```

MSVC with Visual Studio 2026 or newer is also supported. For its generator, omit the Ninja and Clang options and build with `cmake --build build --config Release` in a separate build directory.

CMake can download Vulkan build tools; install 7-Zip for extraction or provide an existing Vulkan SDK.

### Linux

Install development packages for GTK 3, Wayland, X11, OpenGL/EGL, and Vulkan, plus `pkg-config` and Wayland protocol tools.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

Output is in `build/bin`, or `build/bin/Release` with a Visual Studio generator. See the [CI workflows](.github/workflows) for dependency setup.

### Build options

Pass `-DNAME=ON` or `-DNAME=OFF` to CMake:

| Option | Purpose |
| --- | --- |
| `OIV_BUILD_TESTS` | Build test binaries; default `ON` |
| `OIV_BUILD_RENDERER_D3D11` | Direct3D 11; Windows only |
| `OIV_BUILD_RENDERER_VK` | Vulkan |
| `OIV_BUILD_RENDERER_GL` | OpenGL |

Default renderers are listed [above](#command-line); keep at least one enabled. Codec switches are documented in [ImageCodec's CMake configuration](External/ImageCodec/CMakeLists.txt).

## Packaging

Run `./publish.ps1` (`pwsh ./publish.ps1` on Linux) to create a `.7z` package. It uses Ninja and `RelWithDebInfo`, builds in `publish`, and requires 7-Zip. Add `-EnablePackage $false` to build without an archive.

## License

OIViewer is distributed under the [OpenImageViewer License](LICENSE.md), which permits sharing and modification with attribution for noncommercial use.
