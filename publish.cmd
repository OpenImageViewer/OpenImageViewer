@echo off

GOTO Commentends
===================================================================
For using this build and packaging script you'll need to define 
1. MS build path
2. 7z Path.
3. git Path.
4. [optional] set OIV_OFFICIAL_BUILD to 1 if it's an official build.
====================================================================
:Commentends


rem Set custom paths
setlocal EnableDelayedExpansion
rem Global build variables - START
set MSBuildPath=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\amd64
set SevenZipPath=C:\Program Files\7-Zip
set GitPath=C:\Program Files\Git\bin
set path=%path%;%MSBuildPath%;%SevenZipPath%;%GitPath%
rem Change to 1 to make an official build
set OIV_OFFICIAL_BUILD=1
set OIV_VERSION_REVISION=0
set VersionPath=.\oiv\API\Version.h
set PublishPath=.\Bin\x64\Publish
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
if [%OIV_OFFICIAL_BUILD%] == [0] (
    for /f %%i in ('git rev-parse head') do set OIV_VERSION_REVISION=%%i
    set OIV_VERSION_REVISION=!OIV_VERSION_REVISION:~0,7!
    set versionString=!versionString!-!!OIV_VERSION_REVISION!-Unofficial
)

echo ==============================================
echo FOUND VERSION: !versionString!
echo ==============================================

rem Build project
msbuild.exe oiv.sln /m /p:configuration=Publish /t:%BuildOperation% /p:OIV_OFFICIAL_BUILD=%OIV_OFFICIAL_BUILD% /p:OIV_VERSION_REVISION=L\"%OIV_VERSION_REVISION%\"
if  %errorlevel% neq 0 (
    echo Compilation error
    pause
    goto END
)

rem Pack symbols into 7z file.
7z a -mx9 OIV-%versionString%-Win32x64VC14.2-Symbols.7z %PublishPath%\*.pdb
rem Pack application into 7z file.
7z a -mx9 OIV-!versionString!-Win32x64VC14.2.7z %PublishPath%\*.dll %PublishPath%\*.exe %PublishPath%\Resources
echo Success!
pause

:END
