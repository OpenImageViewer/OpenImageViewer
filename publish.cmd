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
set VSdir=C:/Program Files/Microsoft Visual Studio/2022/Enterprise
set CMakePath=C:/Program Files/CMake/bin
set MSBuildPath=%VSdir%/MSBuild/Current/Bin/amd64
set SevenZipPath=C:/Program Files/7-Zip
set GitPath=C:/Program Files/Git/bin
set DependenciesPath=./oiv/Dependencies
set NinjaPath=%VSdir%/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja
set path=%path%;%MSBuildPath%;%SevenZipPath%;%GitPath%;%CMakePath%;%NinjaPath%
set WindowsKitDir=C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0
rem Change to 1 to make an official build
set OIV_OFFICIAL_BUILD=1
set OIV_OFFICIAL_RELEASE=0
set OIV_RELEASE_SUFFIX=
set OIV_VERSION_REVISION=0
set OIV_VERSION_BUILD=11
set BuildType="RelWithDebInfo"
set VersionPath=.\oivlib\oiv\Include\Version.h
set BuildPath=./publish
set BinPath=%BuildPath%/bin
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


echo "Build timestamp..."
pushd .
cd %BuildPath%
set BuildAbsPath=%CD%
popd
pushd .
cd Utils
call buildTimeStamp.cmd %BuildAbsPath%
popd
rem Change directory seperator from'/' to '\' for the timestamp command
set timeStampPath=.\!BuildPath:~2!\timestamp.exe

for /f "tokens=*" %%i in ('%timeStampPath%') do set TIMESTAMP=%%i

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
cmake.exe -S . -B %BuildPath% -G "Ninja" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_MT="%WindowsKitDir%/x64/mt.exe" ^
-DCMAKE_C_COMPILER="%VSdir%/VC/Tools/Llvm/x64/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="%VSdir%/VC/Tools/Llvm/x64/bin/clang-cl.exe" ^
-DCMAKE_MAKE_PROGRAM="%NinjaPath%/ninja.exe" -DCMAKE_RC_COMPILER="%WindowsKitDir%/x64/rc.exe" -DIMCODEC_BUILD_CODEC_FREEIMAGE=ON ^
-DOIV_OFFICIAL_BUILD=%OIV_OFFICIAL_BUILD% -DOIV_OFFICIAL_RELEASE=%OIV_OFFICIAL_RELEASE% -DOIV_VERSION_BUILD=%OIV_VERSION_BUILD%  ^
-DOIV_RELEASE_SUFFIX=L\"%OIV_RELEASE_SUFFIX%\"

if  !errorlevel! neq 0 (
    echo.
    [91m Error: Failed to generate cmake configuration, please make sure cmake is installed correctly: https://cmake.org
    echo [0m
    echo.
    goto :FAILURE
)
)
rem=====================================================================================================
rem Build project
if !OpBuild! equ 1 (
call "%VSdir%\VC\Auxiliary\Build\vcvars64.bat"
if  !errorlevel! neq 0 (
    echo.
    echo [91m Compilation error
    echo [0m
    echo.
    goto :FAILURE
)
cd publish
ninja
if  !errorlevel! neq 0 (
    echo.
    echo [91m Compilation error
    echo [0m
    echo.
    goto :FAILURE
)
cd ..
)
rem=====================================================================================================
rem Pack files
if !OpPack! equ 1 (
set OutputPath=./%BuildPath%/%DATE_YYMMDD_HH_mm_SS%-v%versionStringShort%
xcopy "%DependenciesPath%\*.dll" "%BinPath%\" /D /F
md !OutputPath!
set BaseFileName=!OutputPath!/!DATE_YYMMDD!-OIV-!versionString!-Win32x64VC-LLVM

rem Pack symbols into 7z file.
7z a -mx9 !BaseFileName!-Symbols.7z !BinPath!\*.pdb

rem Pack application into 7z file.
7z a -mx9 !BaseFileName!.7z !BinPath!\*.dll !BinPath!\*.exe !BinPath!\Resources

if  !errorlevel! neq 0 (
    echo.
    echo [91m Can not pack files
    echo [0m
    echo.
    goto :FAILURE
)

)


echo.
echo ==========
echo [92m ^|Success^^!^|
echo [0m
echo ==========
echo.
goto :END


:FAILURE
echo.
echo ==========
echo [91m ^|Failed to execute operations.^|
echo [0m
echo ==========
echo.

:END
pause

