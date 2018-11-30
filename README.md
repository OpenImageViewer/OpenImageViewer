# [O]pen [I]mage [V]iewer

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/86c11bc7e75e4677b8c2b5d50f9cd1c3)](https://app.codacy.com/app/TheNicker/OIV?utm_source=github.com&utm_medium=referral&utm_content=OpenImageViewer/OIV&utm_campaign=Badge_Grade_Settings)

**OIV** is an hardware accelerated blazingly fast open code c++17 compliant cross platform 'C' library and application for viewing and manipulating images.  
It is a tool for both home users and professionals and it's designed for flexibility, user experience and performance.  
No external dependencies are needed, it relies solely on the CRT (excluding embedded image codecs).

**OIV** is:  
1. Independent c++ Image codec library.  
2. C API image viewing engine.  
3. Image viewer - windows only.    

## Features
* Supports many image formats.
* Fast initial image load.
* OpenGL and Direct3D11 as a back bone for displaying images.
* Handles large files, up to 256 mega pixels (16 mega pixels per dimension).
* **Infinite panning**, image keeps panning even when the mouse cursor is at the edge of the desktop working area.
* **Custom pan limits** - pan the image outside/inside the display area, this allows to center on the monitor any area of the image on any zoom level.
* Custom zoom limits. 
* Pixel grid.
* **Multi full screen** - image spans across all monitors.
* **Fine panning** - image panning is bound only to *screen space* - *image space panning* could be less than 1 pixel.  
* Exif support.
* Full Image information (pixel format, transparency, size in memory, etc.)

## Todo
* Complete CMake and compatibility with g++, Linux and MacOS.
* Implement Metal, Vulkan and Direct3D12 renderers.
* Add GPU support for Lanczos re-sampling.  
* Support for images larger than 256 mega pixels.
* Add image color transformations interface for adjusting brightness, contrast, saturation, etc.
* Remove freeimage as a fallback Codec and implement specialized codecs.
* Show and preview sub images (DDS mipmaps, PSD layers, etc.).
* Play animated images.
* Extract various types of meta data.

--------------------------

## Build
### Windows
#### Requirements
* Windows Vista/7/8/8.1/10
* Microsoft build tools 2015 - download from here: https://www.microsoft.com/en-us/download/details.aspx?id=48159  

#### Instructions
**1. Get the source code:**
Clone the repository by running the command: git clone https://github.com/OpenImageViewer/OIV

**2. Embedded Codec dependencies:**
 Modify "ImageCodec\ImageLoader\Source\BuildConfig.h" to choose which codecs are statically embedded into the image loader.  
Embedded codecs are optional and they all may be disabled.  
Get the desired codec dependencies and add it to the relevant project.  
            
CodecJPG - libjpeg-turbo https://sourceforge.net/projects/libjpeg-turbo/  
CodecPNG - libpng http://www.libpng.org/pub/png/libpng.html  
CodecDDS - NVIDIA dds loader fork: https://github.com/paroj/nv_dds  
CodecPSD - libpsd fork:  https://github.com/TheNicker/libpsd  
CodecFreeImage - http://freeimage.sourceforge.net/  


  **3. Build the project** by running the command: "C:\Program Files (x86)\MSBuild\14.0\Bin" oiv.sln

### Linux
coming soon ...

### MacOS
coming soon ...


-----------------------------

## License
OIV is licensed under the [OpenImageViewer License](LICENSE.md).
