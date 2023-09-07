# Open Image Viewer

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/22d2c9bc0fa149fcaf0b84e009839fa9)](https://app.codacy.com/gh/OpenImageViewer/OpenImageViewer?utm_source=github.com&utm_medium=referral&utm_content=OpenImageViewer/OpenImageViewer&utm_campaign=Badge_Grade_Dashboard)
[![Windows MSVC build](https://github.com/OpenImageViewer/OpenImageViewer/actions/workflows/build-windows.yaml/badge.svg)](https://github.com/OpenImageViewer/OpenImageViewer/actions/workflows/build-windows.yaml)

**Open Image Viewer** is a hardware accelerated blazingly fast open code c++20 compliant cross-platform application for **accurately** viewing and analyzing image files.  

It provides accurate representation of the image being viewed, unlike the vast majority of the current viewers that shows the monitor color space data.  

Emphasis on ergonomics and performance is thoroughly weighed to suit every type of user, the starting novice or the hardcore power user.

Currenlty there's virtually no GUI, keyboard key bindings is used for fast workflow, press F1 to list all keybindings.

For more information visit our [website at: www.openimageviewer.com](https://www.openimageviewer.com)  

[A Word from the author](http://www.openimageviewer.com/#word)  
[Highlights and features](http://www.openimageviewer.com/#features)  

![Selection rect demonstration with Open Image Viewer](https://i.ibb.co/NZXpb2W/cut.gif "Preview")

## Build your copy from source / Start developing

### Instructions

1. Clone the repository and update the submodules recursivly. 
2. Use CMake to generate project files or open the root CMake file in visual studio as a CMake project.
3. Compile and run.

### Notes

* PNG codec is disabled by deafult due to ci/cd issues
* FreeImage codec is supplied in the official releases as a fallback but disabled by deafult due to its low performance, see below how to build the project with FreeImage
* 64 bit is only officialy supported, though last checked 32 bit compiles and runs fluently

#### Windows

##### Windows requirements

* Windows 7/8/8.1/10/11
* Microsoft build tools 2022 or higher
  
##### When using Windows 7 SP1, make sure the following are installed

* [KB2670838 - Windows 7 platform update](https://www.microsoft.com/en-us/download/details.aspx?id=36805)
* [KB4019990 - D3DCompiler_47](https://www.catalog.update.microsoft.com/Search.aspx?q=4019990)
* [Universal C runtime](https://support.microsoft.com/en-us/topic/update-for-universal-c-runtime-in-windows-c0514201-7fe6-95a3-b0a5-287930f3560c)

#### Linux

##### Linux requirements

coming soon ...

#### MacOS

##### MacOS requirements

coming soon ...

## Extra Codec Library dependecies (optional)

Name | Link
------------ | -------------
***CodecFreeImage*** | <http://freeimage.sourceforge.net/>
--------------------------

## License

OIV uses the [OpenImageViewer License](LICENSE.md) license.
