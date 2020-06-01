# Open Image Viewer

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/22d2c9bc0fa149fcaf0b84e009839fa9)](https://app.codacy.com/gh/OpenImageViewer/OpenImageViewer?utm_source=github.com&utm_medium=referral&utm_content=OpenImageViewer/OpenImageViewer&utm_campaign=Badge_Grade_Dashboard)
[![Build status](https://ci.appveyor.com/api/projects/status/yua3myv699sm3o17/branch/master?svg=true)](https://ci.appveyor.com/project/LiorL/openimageviewer/branch/master)


**Open Image Viewer** is a hardware accelerated blazingly fast open code c++17 compliant cross-platform 'C' library and application for viewing and 
manipulating images.

The motivation for this project is to create an open code image viewer with great emphasis on ergonomics and performance for every type of user suitable for the starting novice or the hardcore power user.

**Open Image Viewer** in its current form is a collection of 3 projects:
1. Independent c++ Image codec library.
2. C API image viewing engine.
3. Image viewer - windows only, (linux, very soon)

## Highlights
* Supports potentially any image format, plugin based image codecs with FreeImage as fallback.
* Fast initial image load.
* Hardware accelerated by using OpenGL and Direct3D11.
* Handles files up to 256 mega pixels (16 mega pixels per dimension).
* State of the art selection rect.
* Display sub images, such as DDS mipmaps.
* On screen display of image pixel position and value, works also with all floating point types. 
* Infinite panning, no need to ever lift the mouse by using low level API for capturing mouse events.
* Custom pan/zoom limits - place any pixel of an image anywhere on the physical monitor.
* Pixel grid.
* Multi full screen - image spans across all monitors.
* Support for image meta data, such as orientation.
* The project is lightweight and using lightweight dependencies, no boost or other shenanigans.

## Todo
* Add compatibility with g++, Linux and MacOS.
* Implement Metal, Vulkan and Direct3D12 renderers.
* Add GPU support for Lanczos re-sampling.
* Support for images larger than 256 mega pixels.
* Implement more specialized codecs, thus decreasing further the dependency in freeimage.
* Play animated images.
* Suppport more types of meta data.

--------------------------

## Build your copy from source / Start developing


### Instructions

1. **Clone the repository and update the submodules recursivly.**  

2. **If using visual studio then open the root cmake file as CMake project or use the CMake tool.

3. **Compile and run.**

### Notes
* IMCODEC_BUILD_CODEC_DDS is currently the only codec build flag that is enabled by default. For a full set of image format support, enable any of the the IMCODEC_BUILD_CODEC_* cmake options.
* Only 64 bit is officialy supported.


#### Windows
##### Requirements
* Windows Vista/7/8/8.1/10
* Microsoft build tools 2019 or higher
### Linux
#### Requirements
coming soon ...
#### Build Process
coming soon ...

### MacOS
#### Requirements
coming soon ...
#### Build Process
coming soon ...

## Extra Codec Library dependecies (optional)
Name | Link
------------ | -------------
***CodecJPG - libjpeg-turbo*** | https://sourceforge.net/projects/libjpeg-turbo/  
***CodecPNG - libpng*** | http://www.libpng.org/pub/png/libpng.html  
***CodecDDS - NVIDIA dds loader fork*** | https://github.com/paroj/nv_dds  
***CodecPSD - libpsd fork*** | https://github.com/TheNicker/libpsd  
***CodecFreeImage*** | http://freeimage.sourceforge.net/
-----------------------------

## License
OIV uses the [OpenImageViewer License](LICENSE.md) license.
