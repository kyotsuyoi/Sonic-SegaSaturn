@ECHO Off
SETLOCAL

FOR %%I IN ("%~dp0..") DO SET "ROOT_DIR=%%~fI"
SET "EMULATOR_DIR=%ROOT_DIR%\..\..\Emulators"
SET "GAME_CUE=%ROOT_DIR%\game_build\game.cue"

if exist "%GAME_CUE%" (
"%EMULATOR_DIR%\yabause\yabause.exe" -a -i "%GAME_CUE%"
) else (
echo Please compile first ! Missing %GAME_CUE%
pause
)
