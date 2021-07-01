@echo off

GOTO Commentends
===================================================================
For using this build and packaging script you'll need to define 
1. MS build path
2. CMake path
3. 7z Path.
4. git Path.
5. [optional] set OIV_OFFICIAL_BUILD to 1 if it's an official build.
====================================================================
:Commentends


rem Set custom paths
setlocal EnableDelayedExpansion
rem Global build variables - START
set CMakePath=C:\Program Files\CMake\bin
set MSBuildPath=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\amd64
set SevenZipPath=C:\Program Files\7-Zip
set GitPath=C:\Program Files\Git\bin
set DependenciesPath=.\oiv\Dependencies
set path=%path%;%MSBuildPath%;%SevenZipPath%;%GitPath%;%CMakePath%
rem Change to 1 to make an official build
set OIV_OFFICIAL_BUILD=1
set OIV_OFFICIAL_RELEASE=0
set OIV_VERSION_REVISION=0
set OIV_VERSION_BUILD=4

set VersionPath=.\oivlib\oiv\Include\Version.h
set BuildPath=.\Build\Release
set BuildOperation=Build
rem Global build variables - END

set counter=0
rem Get version from source code
for /F "tokens=*" %%A in (%VersionPath%) do (
    set Line=%%A
    set ver=!line:~35,3!
    rem Trim spaces
    set ver=!ver: =!
    set a[!counter!]=!ver!
    set /a counter=!counter! + 1
)

set versionString=%a[1]%.%a[2]%
if [%OIV_OFFICIAL_RELEASE%] == [0] (
    for /f %%i in ('git rev-parse head') do set OIV_VERSION_REVISION=%%i
    set OIV_VERSION_REVISION=!OIV_VERSION_REVISION:~0,7!
    set versionStringShort=!versionString!.%OIV_VERSION_BUILD%
    set versionString=!versionStringShort!-!OIV_VERSION_REVISION!-Nightly
)

set DATE_YYMMDD=%DATE:~6,4%-%DATE:~3,2%-%DATE:~0,2%
set DATE_YYMMDD_HH_mm_SS=%DATE_YYMMDD%_%TIME:~0,2%-%TIME:~3,2%-%TIME:~6,2%
echo ==============================================
echo FOUND VERSION: !versionString!
echo SHORT VERSION: !versionStringShort!
echo ==============================================

cmake -DCMAKE_GENERATOR="Visual Studio 16 2019"  -A x64 -DIMCODEC_BUILD_CODEC_PSD=ON -DIMCODEC_BUILD_CODEC_JPG=ON -DIMCODEC_BUILD_CODEC_PNG=ON -DIMCODEC_BUILD_CODEC_DDS=ON -DIMCODEC_BUILD_CODEC_GIF=ON -DIMCODEC_BUILD_CODEC_TIFF=ON -DIMCODEC_BUILD_CODEC_WEBP=ON -DIMCODEC_BUILD_CODEC_FREEIMAGE=ON -DOIV_OFFICIAL_BUILD=%OIV_OFFICIAL_BUILD% -DOIV_OFFICIAL_RELEASE=%OIV_OFFICIAL_RELEASE% -DOIV_VERSION_BUILD=%OIV_VERSION_BUILD% -S . -B ./build
if  %errorlevel% neq 0 (
    echo.
    echo Error: Failed to generate cmake configuration, please make sure cmake is installed correctly: https://cmake.org
    echo.
    pause
    goto END
)

rem Build project
msbuild.exe .\Build\OpenImageViewer.sln /m /p:ToolExe=clang-cl.exe /p:ToolPath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\Llvm\x64\bin"  /p:configuration=Release /t:%BuildOperation% /p:OIV_OFFICIAL_BUILD=%OIV_OFFICIAL_BUILD%;OIV_OFFICIAL_RELEASE=%OIV_OFFICIAL_RELEASE%
if  %errorlevel% neq 0 (
    echo.
    echo Compilation error
    echo.
    pause
    goto END
)


set OutputPath=%DATE_YYMMDD_HH_mm_SS%-v%versionStringShort%
copy %DependenciesPath%\*.dll %BuildPath%\
md OutputPath

set BaseFileName=%OutputPath%/%DATE_YYMMDD%-OIV-%versionString%-Win32x64VC-LLVM

 
rem Pack symbols into 7z file.
7z a -mx9 %BaseFileName%-Symbols.7z %BuildPath%\*.pdb
rem Pack application into 7z file.

7z a -mx9 %BaseFileName%.7z %BuildPath%\FreeImage.dll %BuildPath%\turbojpeg.dll %BuildPath%\libpng16.dll %BuildPath%\oiv.dll  %BuildPath%\OIViewer.exe %BuildPath%\Resources
echo Success!
pause

:END
