#include <stdio.h>
#include <string.h>
#include "character.h"
#include "ram_cart.h"
#include "jo/fs.h"

/* fallback declarations for compatibility in case different header is picked */
extern void ram_cart_add_used_bytes(unsigned int bytes);
extern void ram_cart_subtract_used_bytes(unsigned int bytes);
#include "jo/console.h"
#include "jo/sprites.h"
#include "jo/image.h"
#include "jo/tga.h"
#include "jo/tools.h"

static void wait_ticks(int ticks)
{
    unsigned int start = jo_get_ticks();
    while ((jo_get_ticks() - start) < (unsigned int)ticks) {
        // idle
    }
}

static void character_show_loading_screen(const char *filename)
{
    (void)filename;
}

static char *read_tga_from_dirs(const char *filename, int *file_len);
static void character_set_last_error(const char *message);

static bool character_get_sheet_dimensions(const char *sheet_name, int *width, int *height)
{
    if (sheet_name == JO_NULL || width == JO_NULL || height == JO_NULL)
        return false;

    *width = 256;
    *height = 612;

    return true;
}

static bool character_store_sheet_to_cart(const char *sheet_name)
{
    int sheet_width;
    int sheet_height;

    if (!character_get_sheet_dimensions(sheet_name, &sheet_width, &sheet_height))
    {
        character_set_last_error("char:sheet dims");
        return false;
    }

    if (!ram_cart_store_tga(sheet_name, sheet_name, "SPT"))
    {
        character_set_last_error(ram_cart_get_last_error());
        return false;
    }

    ram_cart_set_sprite_dimensions(sheet_name, sheet_width, sheet_height);
    character_set_last_error("char:sheet cart ok");
    return true;
}

#define CHARACTER_ROW_IDLE 0
#define CHARACTER_ROW_RUN  1
#define CHARACTER_ROW_JUMP 3
#define CHARACTER_ROW_FALL 4
#define CHARACTER_ROW_PUNCH 6
#define CHARACTER_ROW_HEIGHT 36
#define CHARACTER_FRAME_WIDTH 32
#define CHARACTER_FRAME_MAX_WIDTH 48
#define CHARACTER_FRAME_COUNT 8
#define CHARACTER_ROW_WIDTH (CHARACTER_FRAME_WIDTH * CHARACTER_FRAME_COUNT)
#define CHARACTER_TGA_DIR "cd/SPT"
#define CHARACTER_IDLE_ANIMATION_TICKS 6
#define CHARACTER_RUN_ANIMATION_TICKS 1
#define CHARACTER_PUNCH_ANIMATION_TICKS 4
#define CHARACTER_JUMP_ANIMATION_TICKS 4
#define CHARACTER_FALL_ANIMATION_TICKS 4
#define CHARACTER_LANDING_ANIMATION_TICKS 4
#define CHARACTER_HARD_LANDING_SPEED 6.0f

static const char * const character_tga_file[CHARACTER_COUNT] =
{    "SNC_FUL.TGA",
    "AMY_FUL.TGA",
    "KNK_FUL.TGA",
    "SDW_FUL.TGA",
    "TLS_FUL.TGA"
};

static character_type   current_character = CHARACTER_SONIC;
sonic_t                 player;
static int              idle_anim = -1;
static int              run_anim = -1;
static int              jump_anim = -1;
static int              current_anim = -1;
static int              active_row = -1;
static int              selected_row = 0;
static int              selected_frame = 1;
static int              character_frame_id = -1;
static int              character_frame_bottom[17][8];
static int              character_reference_bottom = CHARACTER_ROW_HEIGHT - 1;
static int              character_sheet_width = 0;
static int              character_sheet_height = 0;
static bool             character_sheet_in_cart = false;
static unsigned int     sprite_bytes_allocated = 0;
static unsigned int     wram_bytes_allocated = 0;
static int              character_state = CHARACTER_ROW_IDLE;
static int              character_frame = 0;
static int              character_tick = 0;
static jo_color        *character_row_cache = NULL;
static jo_color        *character_frame_cache = NULL;
static char             character_last_error[128] = "char:init";
static bool             character_move_left = false;
static bool             character_move_right = false;
static bool             character_action_pressed = false;
static bool             character_punch_requested = false;
static bool             character_was_in_air = false;
static float            character_max_fall_speed = 0.0f;

typedef enum
{
    CHARACTER_ANIM_STATE_IDLE,
    CHARACTER_ANIM_STATE_RUN,
    CHARACTER_ANIM_STATE_JUMP,
    CHARACTER_ANIM_STATE_FALL,
    CHARACTER_ANIM_STATE_PUNCH,
    CHARACTER_ANIM_STATE_LAND
} character_anim_state;

static character_anim_state current_anim_state = CHARACTER_ANIM_STATE_IDLE;

static const char *character_current_sheet_name(void)
{
    return character_tga_file[current_character];
}

static void character_set_last_error(const char *message)
{
    if (message == JO_NULL)
        return;

    strncpy(character_last_error, message, sizeof(character_last_error) - 1);
    character_last_error[sizeof(character_last_error) - 1] = '\0';
}

static int character_get_frame_width(int row, int frame);
static int character_get_frame_offset(int row, int frame);

const char *character_get_last_error(void)
{
    return character_last_error;
}

static void character_compute_frame_bottoms(void)
{
    character_reference_bottom = CHARACTER_ROW_HEIGHT - 1;
    if (character_row_cache == NULL)
        return;

    for (int row = 0; row < 17; ++row)
    {
        for (int frame = 0; frame < 8; ++frame)
            character_frame_bottom[row][frame] = CHARACTER_ROW_HEIGHT - 1;

        for (int frame = 0; frame < 8; ++frame)
        {
            int bottom = -1;
            int width = character_get_frame_width(row, frame);
            int offset = character_get_frame_offset(row, frame);

            for (int y = CHARACTER_ROW_HEIGHT - 1; y >= 0 && bottom < 0; --y)
            {
                for (int x = 0; x < width; ++x)
                {
                    if (character_row_cache[y * CHARACTER_ROW_WIDTH + offset + x] != JO_COLOR_Transparent)
                    {
                        bottom = y;
                        break;
                    }
                }
            }

            if (bottom < 0)
                bottom = CHARACTER_ROW_HEIGHT - 1;

            character_frame_bottom[row][frame] = bottom;
            if (bottom > character_reference_bottom)
                character_reference_bottom = bottom;
        }
    }
}

void    character_choose(character_type type)
{
    if (type < CHARACTER_COUNT)
        current_character = type;
}

character_type  character_get_type(void)
{
    return current_character;
}

static void character_select_row_frame(int row, int frame);

static float character_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static int character_get_idle_frame_count(void)
{
    if (current_character == CHARACTER_SONIC || current_character == CHARACTER_SHADOW)
        return 6;
    return 8;
}

static int character_get_frame_width(int row, int frame)
{
    if (row == CHARACTER_ROW_PUNCH)
    {
        if (frame == 2 || frame == 3)
            return 48;
        return 32;
    }
    return 32;
}

static int character_get_frame_offset(int row, int frame)
{
    int off = 0;

    if (row == CHARACTER_ROW_PUNCH)
    {
        for (int i = 0; i < frame; ++i)
            off += character_get_frame_width(row, i);
        return off;
    }

    return frame * CHARACTER_FRAME_WIDTH;
}

static int character_get_run_frame_delay(float speed)
{
    int speed_step = (int)character_absf(speed);

    if (speed_step >= 6)
        return 3;
    if (speed_step >= 5)
        return 4;
    if (speed_step >= 4)
        return 5;
    if (speed_step >= 3)
        return 6;
    if (speed_step >= 2)
        return 7;
    if (speed_step >= 1)
        return 8;
    return 9;
}

static void character_set_animation_frame(int row, int frame)
{
    if (selected_row == row && selected_frame == frame && active_row == row)
        return;

    character_set_row_frame(row, frame);
}

static void character_start_state(character_anim_state next_state, int row, int frame)
{
    current_anim_state = next_state;
    character_state = row;
    character_frame = frame;
    character_tick = 0;
    character_set_animation_frame(row, frame);
}

static void character_advance_looping_row(int row, int frame_count, int tick_delay)
{
    if (frame_count <= 0)
        return;

    if (current_anim_state == CHARACTER_ANIM_STATE_IDLE && character_state != row)
        character_start_state(CHARACTER_ANIM_STATE_IDLE, row, 0);
    else if (current_anim_state == CHARACTER_ANIM_STATE_RUN && character_state != row)
        character_start_state(CHARACTER_ANIM_STATE_RUN, row, 0);

    ++character_tick;
    if (character_tick < tick_delay)
        return;

    character_tick = 0;
    character_frame = (character_frame + 1) % frame_count;
    character_set_animation_frame(row, character_frame);
}

static void character_update_idle_animation(void)
{
    if (current_anim_state != CHARACTER_ANIM_STATE_IDLE || character_state != CHARACTER_ROW_IDLE)
        character_start_state(CHARACTER_ANIM_STATE_IDLE, CHARACTER_ROW_IDLE, 0);
    else
        character_advance_looping_row(CHARACTER_ROW_IDLE, character_get_idle_frame_count(), CHARACTER_IDLE_ANIMATION_TICKS);
}

static void character_update_run_animation(float speed)
{
    if (current_anim_state != CHARACTER_ANIM_STATE_RUN || character_state != CHARACTER_ROW_RUN)
        character_start_state(CHARACTER_ANIM_STATE_RUN, CHARACTER_ROW_RUN, 0);
    else
        character_advance_looping_row(CHARACTER_ROW_RUN, 8, character_get_run_frame_delay(speed));
}

static void character_update_jump_rise_animation(void)
{
    if (current_anim_state != CHARACTER_ANIM_STATE_JUMP || character_state != CHARACTER_ROW_JUMP)
    {
        character_start_state(CHARACTER_ANIM_STATE_JUMP, CHARACTER_ROW_JUMP, 0);
        return;
    }

    if (character_frame >= 4)
        return;

    ++character_tick;
    if (character_tick < CHARACTER_JUMP_ANIMATION_TICKS)
        return;

    character_tick = 0;
    ++character_frame;
    if (character_frame > 4)
        character_frame = 4;
    character_set_animation_frame(CHARACTER_ROW_JUMP, character_frame);
}

static void character_update_fall_animation(void)
{
    if (current_anim_state != CHARACTER_ANIM_STATE_FALL || character_state != CHARACTER_ROW_FALL)
    {
        character_start_state(CHARACTER_ANIM_STATE_FALL, CHARACTER_ROW_FALL, 0);
        return;
    }

    if (character_frame >= 3)
        return;

    ++character_tick;
    if (character_tick < CHARACTER_FALL_ANIMATION_TICKS)
        return;

    character_tick = 0;
    ++character_frame;
    if (character_frame > 3)
        character_frame = 3;
    character_set_animation_frame(CHARACTER_ROW_FALL, character_frame);
}

static void character_update_punch_animation(void)
{
    if (current_anim_state != CHARACTER_ANIM_STATE_PUNCH || character_state != CHARACTER_ROW_PUNCH)
    {
        character_start_state(CHARACTER_ANIM_STATE_PUNCH, CHARACTER_ROW_PUNCH, 0);
        return;
    }

    if (character_frame >= 5)
    {
        character_start_state(CHARACTER_ANIM_STATE_IDLE, CHARACTER_ROW_IDLE, 0);
        return;
    }

    ++character_tick;
    if (character_tick < CHARACTER_PUNCH_ANIMATION_TICKS)
        return;

    character_tick = 0;
    ++character_frame;
    if (character_frame > 5)
        character_frame = 5;
    character_set_animation_frame(CHARACTER_ROW_PUNCH, character_frame);
}

static void character_update_landing_animation(bool action_pressed)
{
    if (current_anim_state != CHARACTER_ANIM_STATE_LAND || character_state != CHARACTER_ROW_FALL || character_frame < 4)
    {
        character_start_state(CHARACTER_ANIM_STATE_LAND, CHARACTER_ROW_FALL, 4);
        return;
    }

    if (character_frame < 6)
    {
        ++character_tick;
        if (character_tick < CHARACTER_LANDING_ANIMATION_TICKS)
            return;

        character_tick = 0;
        ++character_frame;
        if (character_frame > 6)
            character_frame = 6;
        character_set_animation_frame(CHARACTER_ROW_FALL, character_frame);
        return;
    }

    if (action_pressed)
    {
        if (character_move_left || character_move_right)
            character_start_state(CHARACTER_ANIM_STATE_RUN, CHARACTER_ROW_RUN, 0);
        else
            character_start_state(CHARACTER_ANIM_STATE_IDLE, CHARACTER_ROW_IDLE, 0);
    }
}

void    character_set_row_frame(int row, int frame)
{
    character_select_row_frame(row, frame);
}

static inline int    create_animation(int base_sprite_id, int offset, int frame_count)
{
    int id = jo_create_sprite_anim(base_sprite_id + offset, frame_count, 4);
    return id;
}

static char *read_tga_from_dirs(const char *filename, int *file_len)
{
    // Tenta SPT, raiz, e caminho embutido, com debug
    char *buf = NULL;
    buf = jo_fs_read_file_in_dir(filename, "SPT", file_len);
    if (buf && *file_len >= 18) {
        if (is_debug_overlay_enabled())
            jo_printf(16, 230, "loaded %s from SPT", filename);
        return buf;
    }
    if (buf) jo_free(buf);

    buf = jo_fs_read_file_in_dir(filename, JO_ROOT_DIR, file_len);
    if (buf && *file_len >= 18) {
        if (is_debug_overlay_enabled())
            jo_printf(16, 230, "loaded %s from root", filename);
        return buf;
    }
    if (buf) jo_free(buf);

    char pathbuf[64];
    snprintf(pathbuf, sizeof(pathbuf), "SPT/%s", filename);
    buf = jo_fs_read_file_in_dir(pathbuf, JO_ROOT_DIR, file_len);
    if (buf && *file_len >= 18) {
        if (is_debug_overlay_enabled())
            jo_printf(16, 230, "loaded %s via SPT/filename", filename);
        return buf;
    }
    if (buf) jo_free(buf);

    if (is_debug_overlay_enabled())
        jo_printf(16, 230, "failed load %s (SPT, root, SPT/filename)", filename);
    return NULL;
}

static void ensure_runtime_buffers(void)
{
    if (character_row_cache == NULL)
    {
        character_row_cache = (jo_color *)jo_malloc(sizeof(jo_color) * CHARACTER_ROW_WIDTH * CHARACTER_ROW_HEIGHT);
        if (character_row_cache != NULL)
            wram_bytes_allocated += CHARACTER_ROW_WIDTH * CHARACTER_ROW_HEIGHT * sizeof(jo_color);
        else
            character_set_last_error("char:row malloc");
    }

    if (character_frame_cache == NULL)
    {
        character_frame_cache = (jo_color *)jo_malloc(sizeof(jo_color) * CHARACTER_FRAME_MAX_WIDTH * CHARACTER_ROW_HEIGHT);
        if (character_frame_cache != NULL)
            wram_bytes_allocated += CHARACTER_FRAME_MAX_WIDTH * CHARACTER_ROW_HEIGHT * sizeof(jo_color);
        else
            character_set_last_error("char:frame malloc");
    }
}

static void release_runtime_buffers(void)
{
    if (character_row_cache != NULL)
    {
        jo_free(character_row_cache);
        character_row_cache = NULL;
    }
    if (character_frame_cache != NULL)
    {
        jo_free(character_frame_cache);
        character_frame_cache = NULL;
    }
    wram_bytes_allocated = 0;
}

static bool character_upload_frame_from_wram(int frame)
{
    jo_img img;

    if (character_row_cache == NULL || character_frame_cache == NULL)
    {
        character_set_last_error("char:frame no cache");
        return false;
    }
    if (frame < 0 || frame >= CHARACTER_FRAME_COUNT)
    {
        character_set_last_error("char:frame idx");
        return false;
    }

    int frame_width = character_get_frame_width(active_row, frame);
    int frame_offset = character_get_frame_offset(active_row, frame);
    int dst_offset = (CHARACTER_FRAME_MAX_WIDTH - frame_width) / 2;

    for (int y = 0; y < CHARACTER_ROW_HEIGHT; ++y)
    {
        const jo_color *src = character_row_cache + y * CHARACTER_ROW_WIDTH + frame_offset;
        jo_color *dst = character_frame_cache + y * CHARACTER_FRAME_MAX_WIDTH;

        for (int x = 0; x < dst_offset; ++x)
            dst[x] = JO_COLOR_Transparent;

        for (int x = 0; x < frame_width; ++x)
            dst[dst_offset + x] = src[x];

        for (int x = dst_offset + frame_width; x < CHARACTER_FRAME_MAX_WIDTH; ++x)
            dst[x] = JO_COLOR_Transparent;
    }

    img.width = CHARACTER_FRAME_MAX_WIDTH;
    img.height = CHARACTER_ROW_HEIGHT;
    img.data = character_frame_cache;

    if (character_frame_id < 0)
    {
        character_frame_id = jo_sprite_add(&img);
        if (character_frame_id >= 0)
            sprite_bytes_allocated = CHARACTER_FRAME_WIDTH * CHARACTER_ROW_HEIGHT * sizeof(jo_color);
        else
            character_set_last_error("char:sprite add");
    }
    else
    {
        jo_sprite_replace(&img, character_frame_id);
    }

    if (character_frame_id >= 0)
        character_set_last_error("char:frame ok");
    return character_frame_id >= 0;
}

static void character_load_row(int row)
{
    if (row < 0 || row >= 17)
    {
        character_set_last_error("char:row idx");
        return;
    }

    if (active_row == row)
        return;

    ensure_runtime_buffers();
    if (character_row_cache == NULL || character_frame_cache == NULL)
        return;

    if (!character_sheet_in_cart)
    {
        character_set_last_error("char:cart off");
        return;
    }

    if (!ram_cart_load_region(character_tga_file[current_character],
            character_sheet_width, character_sheet_height,
            0, row * CHARACTER_ROW_HEIGHT,
            CHARACTER_ROW_WIDTH, CHARACTER_ROW_HEIGHT,
            character_row_cache,
            sizeof(jo_color) * CHARACTER_ROW_WIDTH * CHARACTER_ROW_HEIGHT))
    {
        character_set_last_error(ram_cart_get_last_error());
        return;
    }

    selected_row = row;
    character_compute_frame_bottoms();
    active_row = row;
    character_set_last_error("char:row ok");
}

static void character_select_row_frame(int row, int frame)
{
    if (row < 0 || row >= 17)
    {
        character_frame_id = -1;
        return;
    }

    int max_frame = 8;
    if (row == CHARACTER_ROW_PUNCH)
        max_frame = 6; // índice 0..5

    if (frame < 0 || frame >= max_frame)
    {
        character_frame_id = -1;
        return;
    }

    selected_row = row;
    selected_frame = frame;

    character_load_row(row);
    if (!character_upload_frame_from_wram(frame))
        character_frame_id = -1;
}

void character_test_prepare(void)
{
    character_unload();
    ram_cart_cancel_store_tga();
    ram_cart_delete_sprite(character_current_sheet_name());
    character_sheet_in_cart = false;
    active_row = -1;
    character_set_last_error("char:test ready");
}

bool character_test_begin_store_sheet_to_cart(void)
{
    character_sheet_in_cart = false;

    if (!character_get_sheet_dimensions(character_current_sheet_name(), &character_sheet_width, &character_sheet_height))
    {
        character_set_last_error("char:get dims fail");
        return false;
    }

    if (!ram_cart_begin_store_tga(character_current_sheet_name(), character_current_sheet_name(), "SPT"))
    {
        character_set_last_error(ram_cart_get_last_error());
        return false;
    }

    character_set_last_error("char:cart begin");

    return true;
}

bool character_test_process_store_sheet_to_cart(void)
{
    ram_cart_tga_debug_status_t status = ram_cart_step_store_tga(ram_cart_is_verbose_pixel_logging_enabled() ? 4 : 12);

    if (status == RamCartTgaDebugDone)
    {
        ram_cart_set_sprite_dimensions(character_current_sheet_name(), character_sheet_width, character_sheet_height);
        character_sheet_in_cart = true;
        character_set_last_error("char:sheet cart ok");
        return true;
    }

    if (status == RamCartTgaDebugFailed)
    {
        character_set_last_error(ram_cart_get_last_error());
        return false;
    }

    character_set_last_error("char:cart progress");
    return false;
}

bool character_test_is_store_sheet_in_progress(void)
{
    return ram_cart_step_store_tga(0) == RamCartTgaDebugInProgress;
}

int character_test_cart_current_row(void)
{
    return ram_cart_tga_debug_current_row();
}

int character_test_cart_total_rows(void)
{
    return ram_cart_tga_debug_total_rows();
}

const char *character_test_cart_stage(void)
{
    return ram_cart_tga_debug_stage();
}

int character_test_cart_current_pixel(void)
{
    return ram_cart_tga_debug_current_pixel();
}

bool character_test_load_row(int row)
{
    if (!ram_cart_has_entry(character_current_sheet_name()))
    {
        character_set_last_error("char:test no cart");
        return false;
    }

    if (!ram_cart_get_sprite_dimensions(character_current_sheet_name(), &character_sheet_width, &character_sheet_height))
    {
        character_set_last_error("char:cart dims fail");
        return false;
    }

    character_sheet_in_cart = true;
    character_load_row(row);
    return active_row == row;
}

bool character_test_upload_frame(int frame)
{
    if (active_row < 0)
    {
        character_set_last_error("char:test no row");
        return false;
    }

    selected_frame = frame;
    return character_upload_frame_from_wram(frame);
}

void character_test_draw_preview(int x, int y)
{
    if (character_frame_id < 0)
        return;

    jo_sprite_change_sprite_scale(4.0f);
    jo_sprite_draw3D2(character_frame_id, x, y, 450);
    jo_sprite_restore_sprite_scale();
}

bool    character_load(void)
{
    character_set_last_error("char:load start");
    character_unload();
    character_sheet_in_cart = false;

    if (!character_get_sheet_dimensions(character_tga_file[current_character], &character_sheet_width, &character_sheet_height))
    {
        character_frame_id = -1;
        character_set_last_error("char:get dims fail");
        return false;
    }

    if (!ram_cart_has_entry(character_tga_file[current_character]))
    {
        character_set_last_error("char:store cart");
        if (!character_store_sheet_to_cart(character_tga_file[current_character]))
        {
            if (is_debug_overlay_enabled())
                jo_printf(1, 3, "Cart load failed");
            character_frame_id = -1;
            return false;
        }
    }

    if (!ram_cart_get_sprite_dimensions(character_tga_file[current_character], &character_sheet_width, &character_sheet_height))
    {
        character_frame_id = -1;
        character_set_last_error("char:cart dims fail");
        return false;
    }

    character_sheet_in_cart = true;
    character_set_last_error("char:alloc");
    ensure_runtime_buffers();
    if (character_row_cache == NULL || character_frame_cache == NULL)
    {
        character_frame_id = -1;
        return false;
    }

    character_set_last_error("char:first frame");
    character_select_row_frame(CHARACTER_ROW_IDLE, 0);
    if (character_frame_id < 0)
        return false;

    character_state = CHARACTER_ROW_IDLE;
    character_frame = 0;
    character_tick = 0;
    current_anim_state = CHARACTER_ANIM_STATE_IDLE;
    character_move_left = false;
    character_move_right = false;
    character_action_pressed = false;
    character_was_in_air = false;
    character_max_fall_speed = 0.0f;

    idle_anim = run_anim = jump_anim = current_anim = -1;
    character_set_last_error("char:ready");
    return true;
}



void    character_unload(void)
{
    if (idle_anim != -1)
        jo_remove_sprite_anim(idle_anim);
    if (run_anim != -1)
        jo_remove_sprite_anim(run_anim);
    if (jump_anim != -1)
        jo_remove_sprite_anim(jump_anim);
    idle_anim = run_anim = jump_anim = -1;
    current_anim = -1;

    if (character_frame_id != -1)
    {
        jo_sprite_free_from(character_frame_id);
        character_frame_id = -1;
    }

    active_row = -1;
    character_reference_bottom = CHARACTER_ROW_HEIGHT - 1;
    selected_row = 0;
    selected_frame = 1;
    character_state = CHARACTER_ROW_IDLE;
    character_frame = 0;
    character_tick = 0;
    current_anim_state = CHARACTER_ANIM_STATE_IDLE;
    character_move_left = false;
    character_move_right = false;
    character_action_pressed = false;
    character_was_in_air = false;
    character_max_fall_speed = 0.0f;

    release_runtime_buffers();

    character_sheet_in_cart = false;
    character_sheet_width = 0;
    character_sheet_height = 0;

    sprite_bytes_allocated = 0;
}


void    character_draw(void)
{
    int draw_y;

    if (is_debug_overlay_enabled())
    {
        jo_printf(1, 1, "ROW %d / FRAME %d (id=%d)", selected_row, selected_frame, character_frame_id);
        jo_printf(1, 2, "CHAR state=%d tick=%d", character_state, character_tick);
    }
    else
    {
        clear_text_line(1);
        clear_text_line(2);
        clear_text_line(3);
    }

    draw_y = player.y;
    if (selected_row >= 0 && selected_row < 17 && selected_frame >= 0 && selected_frame < 8)
        draw_y += character_reference_bottom - character_frame_bottom[selected_row][selected_frame];

    if (character_frame_id != -1)
    {
        int frame_width = character_get_frame_width(selected_row, selected_frame);
    int draw_x = player.x - CHARACTER_FRAME_MAX_WIDTH / 2;
        if (player.flip_sonic)
            jo_sprite_enable_horizontal_flip();

        jo_sprite_draw3D2(character_frame_id, draw_x, draw_y, 450);

        if (player.flip_sonic)
            jo_sprite_disable_horizontal_flip();
    }
    else
    {
        if (is_debug_overlay_enabled())
            jo_printf(1, 2, "frame not loaded");
    }
}

void    character_set_idle(void)
{
    character_start_state(CHARACTER_ANIM_STATE_IDLE, CHARACTER_ROW_IDLE, 0);
}

void    character_refresh_sprite(void)
{
    if (active_row < 0 || selected_frame < 0)
        return;

    character_select_row_frame(active_row, selected_frame);
}

void    character_set_run(void)
{
    character_start_state(CHARACTER_ANIM_STATE_RUN, CHARACTER_ROW_RUN, 0);
}

void    character_set_jump(void)
{
    character_start_state(CHARACTER_ANIM_STATE_JUMP, CHARACTER_ROW_JUMP, 0);
}

void    character_update_movement(bool left, bool right, bool jump_pressed, bool punch_pressed)
{
    character_move_left = left;
    character_move_right = right;
    character_action_pressed = left || right || jump_pressed || punch_pressed;

    if (punch_pressed && !character_punch_requested && current_anim_state != CHARACTER_ANIM_STATE_PUNCH)
        character_punch_requested = true;

    if (left)
        player.flip_sonic = true;
    else if (right)
        player.flip_sonic = false;
}

void    character_sync_animation(bool in_air, float speed, float speed_y)
{
    bool hard_landing = false;
    bool should_run;

    if (in_air)
    {
        if (speed_y > character_max_fall_speed)
            character_max_fall_speed = speed_y;

        if (speed_y < 0.0f)
            character_update_jump_rise_animation();
        else
            character_update_fall_animation();

        character_was_in_air = true;
        return;
    }

    if (character_punch_requested)
    {
        character_punch_requested = false;
        character_update_punch_animation();
        return;
    }

    if (current_anim_state == CHARACTER_ANIM_STATE_PUNCH)
    {
        character_update_punch_animation();
        return;
    }

    if (character_was_in_air)
    {
        hard_landing = character_max_fall_speed >= CHARACTER_HARD_LANDING_SPEED;
        character_was_in_air = false;
        character_max_fall_speed = 0.0f;

        if (hard_landing)
        {
            character_update_landing_animation(false);
            return;
        }
    }

    if (current_anim_state == CHARACTER_ANIM_STATE_LAND)
    {
        character_update_landing_animation(character_action_pressed);
        return;
    }

    should_run = character_move_left || character_move_right || character_absf(speed) >= 1.0f;
    if (should_run)
        character_update_run_animation(speed);
    else
        character_update_idle_animation();
}
unsigned int    character_sprites_memory_used(void)
{
    return sprite_bytes_allocated;
}

bool character_is_ready(void)
{
    return (character_frame_id >= 0);
}

unsigned int    character_dynamic_memory_used(void)
{
    return wram_bytes_allocated;
}

