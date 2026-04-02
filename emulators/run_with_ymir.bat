@ECHO Off
SETLOCAL

FOR %%I IN ("%~dp0..") DO SET "ROOT_DIR=%%~fI"
SET "EMULATOR_DIR=%ROOT_DIR%\..\..\Emulators"
SET "YMIR_EXECUTABLE_PATH=%EMULATOR_DIR%\ymir\ymir-sdl3.exe"
SET "GAME_BIN_CUE=%ROOT_DIR%\game_build\game_bin.cue"

if not exist "%YMIR_EXECUTABLE_PATH%" (
    echo ---
    echo Please install Ymir here %EMULATOR_DIR%
    echo ---
    pause
    exit /b 1
)

if exist "%GAME_BIN_CUE%" (
rem forward any args to the emulator (e.g. -D -E for debug/exceptions)
"%YMIR_EXECUTABLE_PATH%" %* "%GAME_BIN_CUE%"
) else (
echo Please compile first ! Missing %GAME_BIN_CUE%
pause
exit /b 1
)
