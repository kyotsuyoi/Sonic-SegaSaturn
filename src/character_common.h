#ifndef CHARACTER_COMMON_H
#define CHARACTER_COMMON_H

#include <jo/jo.h>
#include "game_constants.h"
#include "character.h"
#include "character_registry.h"

/**
 * Shared logic for running/walking animation state.
 *
 * This implements the same behavior used by Sonic and Amy and keeps animation
 * timing consistent by driving a set of dummy sprite animations.
 */
void character_running_animation_handling(character_t *chr, jo_sidescroller_physics_params *physics);

/* Shared numeric animation modes for sheet-based characters (Sonic/Knuckles). */
#define CHARACTER_ANIM_IDLE    0
#define CHARACTER_ANIM_WALK    1
#define CHARACTER_ANIM_RUN     2
#define CHARACTER_ANIM_JUMP    3
#define CHARACTER_ANIM_FALL    4
#define CHARACTER_ANIM_LAND    5
#define CHARACTER_ANIM_PUNCH   6
#define CHARACTER_ANIM_DAMAGED 7

void character_running_animation_handling_sheet(character_t *chr,
                                                 jo_sidescroller_physics_params *physics,
                                                 int *anim_mode,
                                                 int *anim_frame,
                                                 int *anim_ticks,
                                                 int *fall_time_ms,
                                                 bool *land_pending,
                                                 int long_fall_ms,
                                                 int punch_frames,
                                                 bool *perform_punch2_or_kick2);

/* Helpers shared by multiple characters (Sonic/Amy) */
int character_create_blank_sprite(void);
int character_create_blank_sprite_with_size(int width, int height);
int character_create_blank_animation(int frame_count);
int character_ensure_wram_sprite(character_t *chr, int *global_sprite_id);
int character_ensure_punch_wram_sprite(character_t *chr);
int character_ensure_damaged_wram_sprite(character_t *chr);
int character_ensure_defeated_wram_sprite(character_t *chr, int defeated_width, int defeated_height);

bool character_has_movement_input(const character_t *chr);
void character_update_animation_frame_generic(int *anim_frame, int *anim_ticks, int frame_count, bool hold_last);
int character_calc_frame_generic(int *anim_mode, int *anim_frame, int *anim_ticks, int mode,
                                 int idle_frames, int walk_frames, int run_frames,
                                 int jump_frames, int fall_frames, int land_frames,
                                 int punch_frames, int damaged_frames);

void character_copy_sheet_frame_to_sprite_with_cart(int sprite_id, const char *cart_name, int sheet_width, int sheet_height, const jo_img *sheet_img, bool use_cart_ram, int frame_x, int frame_y, int frame_width, int target_width);

typedef int (*character_calc_frame_fn)(character_t *chr, int mode);
typedef int (*character_ensure_sprite_fn)(character_t *chr);
typedef void (*character_render_frame_fn)(character_t *chr, int sprite_id);
typedef void (*character_frame_coords_fn)(character_t *chr, int mode, int frame, int *frame_x, int *frame_y, int *frame_width, int *target_width);
typedef void (*character_copy_frame_fn)(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width);

void character_get_frame_coords(character_t *chr,
                                int mode,
                                int frame,
                                int *frame_x,
                                int *frame_y,
                                int *frame_width,
                                int *target_width);

void character_render_sheet_frame(character_t *chr,
                                  int sprite_id,
                                  bool sheet_ready,
                                  int *anim_mode,
                                  int *anim_frame,
                                  character_frame_coords_fn frame_coords_fn,
                                  character_copy_frame_fn copy_frame_fn);

void character_draw_sheet_character(character_t *chr,
                                    bool sheet_ready,
                                    int *anim_mode,
                                    int *anim_frame,
                                    character_calc_frame_fn calc_frame_fn,
                                    character_ensure_sprite_fn ensure_wram_fn,
                                    character_ensure_sprite_fn ensure_punch_fn,
                                    character_render_frame_fn render_frame_fn,
                                    bool flip);

void character_display(character_t *chr,
                       jo_sidescroller_physics_params *physics,
                       void (*draw_fn)(character_t *chr));

void character_set_current(character_t *chr,
                           jo_sidescroller_physics_params *phy,
                           character_t **ref,
                           jo_sidescroller_physics_params **phy_ref);

bool character_load_generic(character_t *chr,
                            bool *loaded,
                            bool *sheet_ready,
                            bool *use_cart_ram,
                            jo_img *sheet_img,
                            int *sheet_width,
                            int *sheet_height,
                            int *sprite_id,
                            int *punch_sprite_id,
                            int *damaged_sprite_id,
                            int *defeated_sprite_id,
                            const char *sprite_dir,
                            const char *sheet_name,
                            const char *defeated_tile_name,
                            const jo_tile *defeated_tile,
                            int defeated_tile_count,
                            ui_character_choice_t profile);

void character_unload_generic(character_t *chr,
                              bool *loaded,
                              bool *sheet_ready,
                              bool *use_cart_ram,
                              jo_img *sheet_img,
                              int *sheet_width,
                              int *sheet_height,
                              int *sprite_id,
                              const char *sheet_name);

void character_copy_sheet_frame_to_sprite(int sprite_id, const jo_img *sheet, int frame_x, int frame_y);
void character_copy_sheet_frame_to_sprite_with_size(int sprite_id, const jo_img *sheet, int frame_x, int frame_y, int width, int height);
void character_copy_defeated_sheet_frame_to_sprite(int sprite_id, const jo_img *sheet, int defeated_width, int defeated_height);

bool character_copy_cart_sheet_frame_to_sprite(int sprite_id, const char *cart_name, int sheet_width, int sheet_height, int frame_x, int frame_y, int width, int height);
bool character_draw_cart_frame(character_t *chr, int sprite_id, const char *cart_name, int sheet_width, int sheet_height);

bool character_ensure_sheet_in_cart(const char *sheet_name, const char *sprite_dir);

bool character_load_sheet(jo_img *sheet, const char *filename, const char *dir, jo_color transparent);
void character_unload_sheet(jo_img *sheet);

/**
 * Decide which sheet frame (row/col) corresponds to the current character state.
 *
 * Returns true if a frame was selected (and row/col are set), or false if the
 * character is defeated (the caller should handle drawing the defeated sprite).
 */
bool character_get_sheet_frame_coords(const character_t *chr, int *row, int *col);

/**
 * Draws the correct frame from the provided sheet based on character state.
 *
 * Returns true if a frame was drawn, false if the character is defeated.
 */
bool character_draw_sheet_frame(character_t *chr, int sprite_id, const jo_img *sheet);

/**
 * Draws the defeated sprite of `chr` using `sheet` and dimensions.
 *
 * Returns true if a defeated sprite was drawn (i.e., life <= 0).
 */
bool character_draw_defeated(character_t *chr, int sprite_id, const jo_img *sheet, int defeated_width, int defeated_height);

/**
 * Draw using the legacy per-frame animation path (no sheet required).
 *
 * This is used when the WRAM sprite sheet is not available (e.g. failed load),
 * but we still want to render based on the character's anim IDs.
 */
void character_draw_legacy_frame(const character_t *chr);

#endif // CHARACTER_COMMON_H
