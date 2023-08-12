@echo off
set CompilerEnv="C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
set outPath=%1
set targetName=timestamp
set targetPath=%outPath%\%targetName%

 if not exist %targetPath%.exe (
     echo Building timestamp.exe 
     md %outPath%
     call %CompilerEnv%
     cl /O1 /Oy /std:c++latest /favor:AMD64 /DUNICODE /D_UNICODE /DNOMINMAX /MD /Fo:"%targetPath%.obj" /I../External/LLUtils/Include timestamp.cpp /link /out:"%targetPath%.exe"
     del "%targetPath%.obj"
 ) else (
   echo Found timestamp.exe
 )
