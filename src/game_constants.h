#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#define WORLD_MAP_ID (0)
#define WORLD_DEFAULT_X (540)
#define WORLD_DEFAULT_Y (120)

/* World coordinates for camera centering on player at start */
#define WORLD_CAMERA_TARGET_X (100)
#define WORLD_CAMERA_TARGET_Y (800)

/* No interaction tile: use attr 0 or omit the attribute in MAP */
#define MAP_TILE_NO_INTERACTION_ATTR (10)
/* Solid/block tile: horizontal + vertical collision */
#define MAP_TILE_BLOCK_ATTR (11)
/* Floor/platform tile: allows standing on top (vertical collision only) */
#define MAP_TILE_PLATFORM_ATTR (12)

#define CHARACTER_WIDTH (32)
#define CHARACTER_WIDTH_2 (24)
#define CHARACTER_HEIGHT (36)
#define CHARACTER_SPIN_SPEED (20)
#define CHARACTER_JUMP_PER_PIXEL_TOLERANCE (14)

/* Airborne visual smoothing shared by player and bots */
#define AIRBORNE_SPRITE_DELAY_MS (800)
#define AIRBORNE_FALL_SPEED_THRESHOLD (4.2f)
#define GAME_FRAME_MS (17)

#define CHARACTER_SPRITE_Z (450)
#define DAMAGE_FX_SPRITE_Z (430)

/* Defeated/blood sprite dimensions (used by bots and character sprites) */
#define DEFEATED_SPRITE_TILE_WIDTH (48) /* must be multiple of 8 for jo_sprite_add_tga_tileset */
#define DEFEATED_SPRITE_WIDTH (48) /* desired displayed width */
#define DEFEATED_SPRITE_HEIGHT (32)

#define STUN_LIGHT_FRAMES (18)
#define STUN_HEAVY_FRAMES (24)
#define COUNTER_STUN_FRAMES (120)

/* Default sprite frame duration used when creating animations.
   This is measured in game ticks (GAME_FRAME_MS ms each).
   For ~60 FPS, GAME_FRAME_MS is 17ms, so a value of 4 gives ~68ms per frame.
*/
#define DEFAULT_SPRITE_FRAME_DURATION (4)

#endif
