@ECHO Off
SETLOCAL

FOR %%I IN ("%~dp0..") DO SET "ROOT_DIR=%%~fI"
SET "EMULATOR_DIR=%ROOT_DIR%\..\..\Emulators"
SET "MEDNAFEN_EXECUTABLE_PATH=%EMULATOR_DIR%\mednafen\mednafen.exe"
SET "GAME_CUE=%ROOT_DIR%\game_build\game.cue"

if not exist "%MEDNAFEN_EXECUTABLE_PATH%" (
	echo ---
	echo Please install Mednafen here %EMULATOR_DIR%
	echo ---
	pause
	exit /b 1
)

if exist "%GAME_CUE%" (
"%MEDNAFEN_EXECUTABLE_PATH%" "%GAME_CUE%" -sound.volume "150"
) else (
echo Please compile first ! Missing %GAME_CUE%
pause
)
