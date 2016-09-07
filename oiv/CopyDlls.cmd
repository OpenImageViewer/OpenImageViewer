rem %1 outdir
rem %2 configuration
rem %3 OgreBuild
rem %4 OgreDepends


IF %2% EQU  Release (
    goto COPY_RELEASE
 ) ELSE IF %2 EQU  Debug ( 
    goto COPY_DEBUG
 )

:COPY_DEBUG
echo Copying Ogre %2 dll's
xcopy "%3\bin\debug\OgreMain_d.dll" %1 /Y /D
xcopy "%3\bin\debug\RenderSystem_Direct3D11_d.dll" %1 /Y /D
echo Copying dependencies %2 dll's
xcopy "%4\bin\debug\OIS_d.dll" %1 /Y /D
goto END

:COPY_RELEASE
echo Copying Ogre %2 dll's
xcopy "%3\bin\release\OgreMain.dll" %1 /Y /D
xcopy "%3\bin\release\RenderSystem_Direct3D11.dll" %1 /Y /D
echo Copying dependencies %2 dll's
xcopy "%4\bin\release\OIS.dll" %1 /Y /D
goto END


:END

set errorlevel = 0