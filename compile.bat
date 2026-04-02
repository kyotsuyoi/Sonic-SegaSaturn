@ECHO Off
SETLOCAL EnableExtensions EnableDelayedExpansion
SET COMPILER_DIR=..\..\Compiler
SET JO_ENGINE_SRC_DIR=../../jo_engine
SET BUILD_DIR=game_build
SET TRACKS_DIR=game_tracks
SET PATH=%COMPILER_DIR%\WINDOWS\Other Utilities;%PATH%

call .\clean.bat
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist build mkdir build

ECHO Clear done.

@ECHO Off
SET COMPILER_DIR=..\..\Compiler
SET PATH=%COMPILER_DIR%\WINDOWS\Other Utilities;%COMPILER_DIR%\WINDOWS\bin;%PATH%
make re
SET "MAKE_EXIT=%ERRORLEVEL%"

if exist game.bin move /Y game.bin %BUILD_DIR%\ > nul
if exist game.cue move /Y game.cue %BUILD_DIR%\ > nul
if exist game_bin.cue move /Y game_bin.cue %BUILD_DIR%\ > nul
if exist game.elf move /Y game.elf %BUILD_DIR%\ > nul
if exist game.iso move /Y game.iso %BUILD_DIR%\ > nul
if exist game.map move /Y game.map %BUILD_DIR%\ > nul
if exist game.raw move /Y game.raw %BUILD_DIR%\ > nul

if exist "%TRACKS_DIR%\game (track 2).wav" copy /Y "%TRACKS_DIR%\game (track 2).wav" "%BUILD_DIR%\game (track 2).wav" > nul
if exist "%TRACKS_DIR%\game (track 3).wav" copy /Y "%TRACKS_DIR%\game (track 3).wav" "%BUILD_DIR%\game (track 3).wav" > nul
if exist "%TRACKS_DIR%\game (track 4).wav" copy /Y "%TRACKS_DIR%\game (track 4).wav" "%BUILD_DIR%\game (track 4).wav" > nul

@ECHO Off
SET EMULATOR_DIR=..\..\Emulators
SET YMIR_EXECUTABLE_PATH=%EMULATOR_DIR%\ymir\ymir-sdl3.exe

if exist %BUILD_DIR%\game.iso (
copy /Y %BUILD_DIR%\game.iso %BUILD_DIR%\game.bin > nul
if errorlevel 1 (
echo [WARN] Failed to update game.bin from game.iso. The file may be locked by an emulator.
)
echo FILE "game.iso" BINARY> %BUILD_DIR%\game.cue
echo   TRACK 01 MODE1/2048>> %BUILD_DIR%\game.cue
echo       INDEX 01 00:00:00>> %BUILD_DIR%\game.cue
echo       POSTGAP 00:02:00>> %BUILD_DIR%\game.cue
echo FILE "game (track 2).wav" WAVE>> %BUILD_DIR%\game.cue
echo   TRACK 02 AUDIO>> %BUILD_DIR%\game.cue
echo     PREGAP 00:02:00>> %BUILD_DIR%\game.cue
echo     INDEX 01 00:00:00>> %BUILD_DIR%\game.cue
echo FILE "game (track 3).wav" WAVE>> %BUILD_DIR%\game.cue
echo   TRACK 03 AUDIO>> %BUILD_DIR%\game.cue
echo     INDEX 01 00:00:00>> %BUILD_DIR%\game.cue
echo FILE "game (track 4).wav" WAVE>> %BUILD_DIR%\game.cue
echo   TRACK 04 AUDIO>> %BUILD_DIR%\game.cue
echo     INDEX 01 00:00:00>> %BUILD_DIR%\game.cue
echo FILE "game.bin" BINARY> %BUILD_DIR%\game_bin.cue
echo   TRACK 01 MODE1/2048>> %BUILD_DIR%\game_bin.cue
echo       INDEX 01 00:00:00>> %BUILD_DIR%\game_bin.cue
echo   POSTGAP 00:02:00>> %BUILD_DIR%\game_bin.cue
echo FILE "game (track 2).wav" WAVE>> %BUILD_DIR%\game_bin.cue
echo   TRACK 02 AUDIO>> %BUILD_DIR%\game_bin.cue
echo     PREGAP 00:02:00>> %BUILD_DIR%\game_bin.cue
echo     INDEX 01 00:00:00>> %BUILD_DIR%\game_bin.cue
echo FILE "game (track 3).wav" WAVE>> %BUILD_DIR%\game_bin.cue
echo   TRACK 03 AUDIO>> %BUILD_DIR%\game_bin.cue
echo     INDEX 01 00:00:00>> %BUILD_DIR%\game_bin.cue
echo FILE "game (track 4).wav" WAVE>> %BUILD_DIR%\game_bin.cue
echo   TRACK 04 AUDIO>> %BUILD_DIR%\game_bin.cue
echo     INDEX 01 00:00:00>> %BUILD_DIR%\game_bin.cue
)

if not exist %YMIR_EXECUTABLE_PATH% (
echo Ymir executable not found at %YMIR_EXECUTABLE_PATH%
echo Compilation finished.
exit /b 0
)

if NOT "!MAKE_EXIT!"=="0" (
echo [WARN] Build step returned code !MAKE_EXIT!.
)

if exist %BUILD_DIR%\game_bin.cue (
echo Emulation is running!
"%YMIR_EXECUTABLE_PATH%" "%cd%\%BUILD_DIR%\game_bin.cue"
CLS
echo Finished!
) else if exist %BUILD_DIR%\game.cue (
echo Emulation is running!
"%YMIR_EXECUTABLE_PATH%" "%cd%\%BUILD_DIR%\game.cue"
CLS
echo Finished!
) else (
echo Compilation -%BUILD_DIR%\game_bin.cue/%BUILD_DIR%\game.cue- not found!
)

ENDLOCAL
