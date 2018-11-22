# [O]pen [I]mage [V]iewer

**OIV** is a hardware accelerated blazingly fast open code c++17 compliant cross-platform 'C' library and application for viewing and manipulating images.
It is a tool for both home users and professionals and it's designed for flexibility, user experience and performance.
External dependencies are not needed, it relies solely on the CRT (excluding embedded image codecs).

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
* Remove freeimage as a fallback codec and implement specialized codecs.
* Show and preview sub images (DDS mipmaps, PSD layers, etc.).
* Play animated images.
* Extract various types of meta data.

--------------------------

## Build your copy from source / Start developing

### Instructions

1. **Get The Source Code:**  
Clone the repository by running the command:  
`git clone https://github.com/OpenImageViewer/OIV`

1. **Third Party Dependencies**  
A folder named `Externals` is expected in the root folder with all the  [libraries](#libraries) in it (See the `CMakeList.txt` file for exact location per dependency).

1. **Embedded Codec Dependencies:**  
Modify "ImageCodec\ImageLoader\Source\BuildConfig.h" to choose which codecs are statically embedded into the image loader.
Embedded codecs are optional and they can all be disabled.
Get the desired [codec dependencies](codec-libraries) and add it to the relevant project.

### Build the project

#### Windows
##### Requirements
* Windows Vista/7/8/8.1/10
* Microsoft build tools 2015 or higher ([download](https://www.microsoft.com/en-us/download/details.aspx?id=48159))
##### Build Command
* Run the command: "C:\Program Files (x86)\MSBuild\14.0\Bin\msbuild.exe" oiv.sln  
<span style="background-color: yellow;">**NOTE:** The path for msbuild may vary depending on your setup</span>

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

-----------------------------
## Libraries
Name         | Link
------------ | -------------
***libfreetype2*** | https://github.com/servo/libfreetype2

## Codec Libraries
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
