@ECHO Off
SETLOCAL

FOR %%I IN ("%~dp0..") DO SET "ROOT_DIR=%%~fI"
SET "EMULATOR_DIR=%ROOT_DIR%\..\..\Emulators"
SET "NOVA_BIOS_PATH=%EMULATOR_DIR%\nova\bios\bios.bin"
SET "GAME_CUE=%ROOT_DIR%\game_build\game.cue"

if not exist "%NOVA_BIOS_PATH%" (
	echo ---
	echo Nova doesn't support HLE bios today.
	echo Therefore, please put Sega Saturn bios here %NOVA_BIOS_PATH%
	echo ---
	pause
	exit /b 1
)

if exist "%GAME_CUE%" (
"%EMULATOR_DIR%\nova\nova.exe" "%GAME_CUE%"
) else (
echo Please compile first ! Missing %GAME_CUE%
pause
)
