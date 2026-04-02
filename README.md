# Sonic SS

Sonic project for the Sega Saturn using Jo Engine.

Compatible with:

- Emulators (tested on Ymir, Yabause and YabaSanshiro).
- CD-R via a disc drive.
- SAROO.
- Needs 4MB Cart RAM.

Available features:

- Battles against bots (4 bots)
- Player vs Player battles (also up to 4 bots)
- Audio test menu
- Joystick mapping menu
- 5 char
- Map select

Available characters (v0.1.X):

- Sonic — OK
- Amy — OK
- Tails — OK
- Knuckles — OK
- Shadow — OK

## v0.1.X

### 1. Splash Screen (Press Start)
![Splash Screen](readme_src/spl_scr.png)

### 2. Character Select
![Character Select](readme_src/slc_chr.png)

### 3. Battlefield
![Battlefield](readme_src/btl_fld01.gif)
![Battlefield](readme_src/btl_fld02.gif)

## v0.2.X

Available features:

- 7 char

Available characters (v0.2.X):

- Sonic — NOK
- Amy — NOK
- Tails — NOK
- Knuckles — NOK
- Shadow — NOK
- Cream — NOK
- Rouge — NOK

### Checklist (Road to v0.2) (20%)

- ✅ Sprites for Idle and Run.
- ✅ Sprites for Jump (3 steps) and Punch.
- ✅ Fix Tails's tail sprites.
- ✅ Set cooldown for all attacks.

- ⬜ (90%) Tiles DMA on Work RAM: “Direct Memory Access transfer” (copy from ROM → WRAM or WRAM → VRAM).
- ⬜ (90%) Tiles packing: grouping multiple sprites into the same block to reduce overhead.

- ✅ Sprites DMA on Cart RAM: copy from Cart → VRAM.
- ✅ Tiles packing: grouping multiple sprites into the same block to reduce overhead.

- ⬜ New FULL Sprite Sheet.
- - ⬜ (90%) Sonic.
- - ⬜ (70%) Amy.
- - ⬜ (70%) Tails.
- - ⬜ (70%) Knuckles.
- - ⬜ (10%) Shadow.
- - ⬜ (0%) Cream.
- - ⬜ (0%) Rouge.

- ⬜ Implement sound effects and music.

- ⬜ Menus
- - ✅ Create pause menu with Resume option and return to character select.
- - ⬜ Create main screen with Start Game and Options.
- - ⬜ In Start Game we should have Single and Multiplayer options.
- - ✅ Create character selection menu.
- - ⬜ In Options we should have Joystick Map and Audio Settings.
- - ⬜ In Options->Joystick Map we should have button remapping option.
- - ⬜ In Options->Audio we should have options to change master volume, music volume, and sound effect volume.
- - ⬜ In Options->Audio we should also have option to test music and sound effects.
- - ⬜ After selecting characters we should show a teams menu.
- - ⬜ After selecting teams we should show the map selection menu.
- - ⬜ Debug menu to adjust stun, knockback, pulse, and damage in-game.

- ⬜ Player 2.
- - ⬜ Detect when controller 2 is connected.
- - ⬜ Show P2 character indicator in character select screen.

- ⬜ Assign characters to specific locations on the map using the Map Editor (only P1/P2 is currently working).
- ⬜ Camera control.
- ⬜ Auto camera that stays at a midpoint between the players.
- ⬜ Allow the camera to follow player 1 or player 2.

- ⬜ (0%) Bots.
- - ⬜ (0%) Bot strategy.
- - ⬜ Make the bots follow the same sprite control flow as the players.
- - ⬜ (0%) Create spectator mode to watch bots fight.