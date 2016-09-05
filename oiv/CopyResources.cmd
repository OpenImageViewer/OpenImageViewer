rem set roaming=%AppData%\UniOgre
rem mkdir %roaming%

xcopy %1settings.ini %2 /Y /D
xcopy %1\Resources %2\Resources /Y /D /S /I /E

set errorlevel = 0