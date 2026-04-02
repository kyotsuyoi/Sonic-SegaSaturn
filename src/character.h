#ifndef __CHARACTER_H__
# define __CHARACTER_H__

#include <stdbool.h>
#include <jo/jo.h>

# define WORLD_MAP_ID                    (0)
# define WORLD_DEFAULT_X                 (540)
# define WORLD_DEFAULT_Y                 (120)

# define MAP_TILE_BLOCK_ATTR             (1)

# define SONIC_WIDTH                     (32)
# define SONIC_WIDTH_2                   (16)
# define SONIC_HEIGHT                    (36)
# define SONIC_SPIN_SPEED                (20)

/* If sonic almost touch the ground we allow the user to jump */
# define SONIC_JUMP_PER_PIXEL_TOLERANCE  (8)

typedef enum
{
    CHARACTER_SONIC,
    CHARACTER_AMY,
    CHARACTER_KNUCKLES,
    CHARACTER_SHADOW,
    CHARACTER_TAILS,
    CHARACTER_COUNT
}               character_type;

void            character_choose(character_type type);
character_type  character_get_type(void);
bool            character_load(void);
void            character_unload(void);
void            character_draw(void);
void            character_set_idle(void);
void            character_set_run(void);
void            character_set_jump(void);
void            character_update_movement(bool left, bool right, bool jump_pressed, bool action_pressed);
void            character_sync_animation(bool in_air, float speed, float speed_y);
void            character_set_row_frame(int row, int frame);
bool            character_is_ready(void);
void            character_refresh_sprite(void);
const char      *character_get_last_error(void);
bool            is_debug_overlay_enabled(void);
void            character_test_prepare(void);
bool            character_test_begin_store_sheet_to_cart(void);
bool            character_test_process_store_sheet_to_cart(void);
bool            character_test_is_store_sheet_in_progress(void);
bool            character_test_load_row(int row);
bool            character_test_upload_frame(int frame);
void            character_test_draw_preview(int x, int y);
unsigned int    character_dynamic_memory_used(void);
int             character_test_cart_current_row(void);
int             character_test_cart_total_rows(void);
const char      *character_test_cart_stage(void);
int             character_test_cart_current_pixel(void);

typedef struct
{
    int         walking_anim_id;
    int         spin_sprite_id;
    int         x;
    int         y;
    bool        flip_sonic;
    bool        spin;
    bool        can_jump;
    int         angle;
} sonic_t;

extern sonic_t player;

unsigned int    character_sprites_memory_used(void);

#endif /* !__CHARACTER_H__ */
