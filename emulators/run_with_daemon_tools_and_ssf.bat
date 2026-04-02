@ECHO Off
SETLOCAL

FOR %%I IN ("%~dp0..") DO SET "ROOT_DIR=%%~fI"
SET "EMULATOR_DIR=%ROOT_DIR%\..\..\Emulators"
SET "DT_DIR=C:\Program Files (x86)\DAEMON Tools Lite"
SET "GAME_CUE=%ROOT_DIR%\game_build\game.cue"

if exist "%GAME_CUE%" (
echo Mounting image...
"%DT_DIR%\DTLite.exe" -mount 0,"%GAME_CUE%"
pushd "%EMULATOR_DIR%\SSF"
echo Running SSF...
"SSF.exe"
popd
echo Unmounting image...
"%DT_DIR%\DTLite.exe" -unmount 0
) else (
echo Please compile first ! Missing %GAME_CUE%
pause
)
