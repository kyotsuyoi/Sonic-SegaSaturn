@ECHO Off
SETLOCAL

FOR %%I IN ("%~dp0..") DO SET "ROOT_DIR=%%~fI"
SET "EMULATOR_DIR=%ROOT_DIR%\..\..\Emulators"
SET "VCD_DIR=C:\Program Files (x86)\Elaborate Bytes\VirtualCloneDrive"
SET "GAME_CUE=%ROOT_DIR%\game_build\game.cue"

if exist "%GAME_CUE%" (
echo Mounting image...
"%VCD_DIR%\vcdmount.exe" "%GAME_CUE%"
pushd "%EMULATOR_DIR%\SSF"
echo Running SSF...
"SSF.exe"
popd
echo Unmounting image...
"%VCD_DIR%\vcdmount.exe" /u
) else (
echo Please compile first ! Missing %GAME_CUE%
pause
)
