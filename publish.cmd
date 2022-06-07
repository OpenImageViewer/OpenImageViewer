@echo off

GOTO :Commentends
===================================================================
For using this build and packaging script you'll need to define 
1. MS build path
2. CMake path
3. 7z Path.
4. git Path.
5. [optional] set OIV_OFFICIAL_BUILD to 1 if it's an official build.
====================================================================
:Commentends

rem Build script operations
set OpRunCmake=1
set OpBuild=1
set OpPack=1


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
set OIV_RELEASE_SUFFIX=
set OIV_VERSION_REVISION=0
set OIV_VERSION_BUILD=8

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

set versionString=%a[1]%.%a[2]%!OIV_RELEASE_SUFFIX!.%OIV_VERSION_BUILD%
set versionStringShort=!versionString!
if [%OIV_OFFICIAL_RELEASE%] == [0] (
    for /f %%i in ('git rev-parse head') do set OIV_VERSION_REVISION=%%i
    set OIV_VERSION_REVISION=!OIV_VERSION_REVISION:~0,7!
    set versionString=!versionStringShort!-!OIV_VERSION_REVISION!-Nightly
)


for /f "tokens=*" %%i in ('timestamp') do set TIMESTAMP=%%i

set DATE_YYMMDD=%TIMESTAMP:~0,4%-%TIMESTAMP:~5,2%-%TIMESTAMP:~8,2%
set DATE_YYMMDD_HH_mm_SS=%DATE_YYMMDD%_%TIMESTAMP:~11,2%-%TIMESTAMP:~14,2%-%TIMESTAMP:~17,2%
echo ==============================================
echo FOUND VERSION: !versionString!
echo SHORT VERSION: !versionStringShort!
echo SHORT DATE   : !DATE_YYMMDD!
echo LONG DATE    : !DATE_YYMMDD_HH_mm_SS!
echo ==============================================

rem=====================================================================================================
rem Run Cmake
if !OpRunCmake! equ 1 (
cmake -DCMAKE_GENERATOR="Visual Studio 16 2019"  -A x64 -DIMCODEC_BUILD_CODEC_PSD=ON -DIMCODEC_BUILD_CODEC_JPG=ON -DIMCODEC_BUILD_CODEC_PNG=ON -DIMCODEC_BUILD_CODEC_DDS=ON -DIMCODEC_BUILD_CODEC_GIF=ON -DIMCODEC_BUILD_CODEC_TIFF=ON -DIMCODEC_BUILD_CODEC_WEBP=ON -DIMCODEC_BUILD_CODEC_FREEIMAGE=ON -DOIV_OFFICIAL_BUILD=%OIV_OFFICIAL_BUILD% -DOIV_OFFICIAL_RELEASE=%OIV_OFFICIAL_RELEASE% -DOIV_VERSION_BUILD=%OIV_VERSION_BUILD% -DOIV_RELEASE_SUFFIX=L\"%OIV_RELEASE_SUFFIX%\" -S . -B ./build
if  !errorlevel! neq 0 (
    echo.
    echo Error: Failed to generate cmake configuration, please make sure cmake is installed correctly: https://cmake.org
    echo.
    goto :FAILURE
)
)
rem=====================================================================================================
rem Build project
if !OpBuild! equ 1 (
msbuild.exe .\Build\OpenImageViewer.sln /m /p:ToolExe=clang-cl.exe /p:ToolPath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\Llvm\x64\bin"  /p:configuration=Release /t:%BuildOperation% /p:OIV_OFFICIAL_BUILD=%OIV_OFFICIAL_BUILD%;OIV_OFFICIAL_RELEASE=%OIV_OFFICIAL_RELEASE%
if  !errorlevel! neq 0 (
    echo.
    echo Compilation error
    echo.
    goto :FAILURE
)
)
rem=====================================================================================================
rem Pack files
if !OpPack! equ 1 (
set OutputPath=%DATE_YYMMDD_HH_mm_SS%-v%versionStringShort%
copy %DependenciesPath%\*.dll %BuildPath%\
md !OutputPath!
set BaseFileName=!OutputPath!/!DATE_YYMMDD!-OIV-!versionString!-Win32x64VC-LLVM

rem Pack symbols into 7z file.
7z a -mx9 !BaseFileName!-Symbols.7z !BuildPath!\*.pdb

rem Pack application into 7z file.
7z a -mx9 !BaseFileName!.7z !BuildPath!\FreeImage.dll !BuildPath!\turbojpeg.dll !BuildPath!\libpng16.dll !BuildPath!\OIViewer.exe !BuildPath!\Resources

if  !errorlevel! neq 0 (
    echo.
    echo Can not pack files
    echo.
    goto :FAILURE
)

)


echo.
echo ==========
echo ^|Success^^!^|
echo ==========
echo.
goto :END


:FAILURE
echo.
echo ==========
echo ^|Failed to execute operations.^|
echo ==========
echo.

:END
pause

