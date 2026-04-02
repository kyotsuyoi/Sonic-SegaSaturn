/*
** Jo Sega Saturn Engine
** Copyright (c) 2012-2017, Johannes Fetz (johannesfetz@gmail.com)
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the Johannes Fetz nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL Johannes Fetz BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <jo/jo.h>
#include "character.h"
#include "ram_cart.h"
#include "menu_text.h"

/* fallback declarations for compatibility in case different header is picked */
extern bool ram_cart_is_ok(void);
extern unsigned int ram_cart_used_bytes(void);
extern unsigned int ram_cart_size_bytes(void);
extern void ram_cart_draw_boot_screen(void);
extern void ram_cart_defocus_boot_text(void);
extern bool check_cartridge_ram(void);
extern jo_color *nbg1_bitmap;
/*
** SPECIAL NOTE: It's not the Sonic that I'm working on, but you can now write your own :)
*/

jo_sidescroller_physics_params  physics;
static int map_pos_x = WORLD_DEFAULT_X;
static int map_pos_y = WORLD_DEFAULT_Y;

static bool is_boot_screen = true;
static bool is_main_menu = false;
static bool is_start_game_menu = false;
static bool is_options_menu = false;
static bool is_character_select = false;
static bool is_character_test = false;
static bool is_game_paused = false;
static bool show_memory_stats = false;
static bool show_debug_overlay = false;
static bool game_started = false;
static bool pending_resume_sprite_refresh = false;
static bool map_tiles_loaded = false;
static bool map_content_loaded = false;
static bool character_test_store_active = false;
static bool character_test_action_pending = false;
static bool is_multiplayer_mode = false;
static int  main_menu_idx = 0;
static int  start_game_menu_idx = 0;
static int  character_select_active_player = 1; // 1 = P1, 2 = P2 in multiplayer
static int  selected_character_p1 = CHARACTER_SONIC;
static int  selected_character_p2 = CHARACTER_AMY;
static jo_img cached_menu_background = {JO_TV_WIDTH, JO_TV_HEIGHT, JO_NULL};
static bool cached_menu_background_valid = false;
static jo_img saved_game_background = {JO_TV_WIDTH, JO_TV_HEIGHT, JO_NULL};
static bool saved_game_background_valid = false;
static int loading_phase = 0;
static int selected_character = CHARACTER_SONIC;
static int pause_menu_idx = 0;
static int character_test_step = 0;
static int character_test_pending_step = -1;
static int character_test_action_delay = 0;

bool            is_debug_overlay_enabled(void)
{
    return show_debug_overlay;
}

static void enter_character_test_screen(void)
{
    int lines[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

    clear_text_lines(lines, 13);
    is_character_select = false;
    is_character_test = true;
    game_started = false;
    loading_phase = 0;
    character_test_step = 0;
    character_test_store_active = false;
    character_test_action_pending = false;
    character_test_pending_step = -1;
    character_test_action_delay = 0;
    character_choose(selected_character);
    character_test_prepare();
}

static void process_character_test_screen(void)
{
    if (character_test_action_pending)
    {
        if (character_test_action_delay > 0)
        {
            --character_test_action_delay;
            return;
        }

        if (character_test_pending_step == 0)
        {
            if (character_test_begin_store_sheet_to_cart())
                character_test_store_active = true;
        }
        else if (character_test_pending_step == 1)
        {
            if (character_test_load_row(0))
                character_test_step = 2;
        }
        else if (character_test_pending_step == 2)
        {
            if (character_test_upload_frame(0))
                character_test_step = 3;
        }

        character_test_action_pending = false;
        character_test_pending_step = -1;
    }

    if (!character_test_store_active)
        return;

    if (character_test_process_store_sheet_to_cart())
    {
        character_test_store_active = false;
        character_test_step = 1;
    }
    else if (!character_test_is_store_sheet_in_progress())
    {
        character_test_store_active = false;
    }
}

/* Cartridge RAM detection now in src/ram_cart.c and exposed via ram_cart.h */

void            reset_demo(void)
{
    map_pos_x = WORLD_DEFAULT_X;
    map_pos_y = WORLD_DEFAULT_Y;
    player.x = 160;
    player.y = 70;
    player.angle = 0;
    player.flip_sonic = false;
    player.spin = false;
    player.can_jump = true;
}

bool          has_vertical_collision(void)
{
    int dist;

    player.can_jump = false;
    if (physics.speed_y < 0.0f)
    {
        physics.is_in_air = true;
        return false;
    }

    dist = jo_map_per_pixel_vertical_collision(WORLD_MAP_ID,
        map_pos_x + player.x + SONIC_WIDTH_2,
        map_pos_y + player.y + SONIC_HEIGHT,
        JO_NULL);

    if (dist == JO_MAP_NO_COLLISION || dist > 0)
    {
        if (dist != JO_MAP_NO_COLLISION && dist < SONIC_JUMP_PER_PIXEL_TOLERANCE)
            player.can_jump = true;
        physics.is_in_air = true;
        return false;
    }

    if (dist < 0 && jo_is_float_equals_zero(physics.speed_y))
        player.y += dist;

    player.can_jump = true;
    physics.is_in_air = false;
    return true;
}

static inline bool      has_horizontal_collision(void)
{
    int         probe_x;
    int         attr;

    probe_x = jo_physics_is_going_on_the_right(&physics) ? player.x + 4 :
              jo_physics_is_going_on_the_left(&physics) ? player.x - 4 :
              player.x;

    attr = jo_map_hitbox_detection_custom_boundaries(
        WORLD_MAP_ID,
        map_pos_x + probe_x + SONIC_WIDTH_2,
        map_pos_y + player.y,
        4,
        20);
    if (attr == JO_MAP_NO_COLLISION)
        return false;
    if (attr != MAP_TILE_BLOCK_ATTR)
        return false;
    return true;
}

void     sonic_collision_handling(void)
{
    if (has_vertical_collision())
        physics.speed_y = 0.0f;
    else
    {
        jo_physics_apply_gravity(&physics);
        player.y += physics.speed_y;
    }

    if (has_horizontal_collision())
        physics.speed = 0.0f;
    else if (physics.speed > 0.0f)
        map_pos_x += physics.speed < 1.0f ? 1.0f : physics.speed;
    else if (physics.speed < 0.0f)
        map_pos_x += physics.speed > -1.0f ? -1.0f : physics.speed;
}

static const char * const character_name[CHARACTER_COUNT] =
{
    "Sonic",
    "Amy",
    "Knuckles",
    "Shadow",
    "Tails"
};

static inline void     draw_main_menu(void)
{
    int lines[] = {8, 9, 10, 11, 12};
    clear_text_lines(lines, 5);

    draw_text(8, 8, "Main Menu");
    draw_text(8, 10, "%s Start Game", main_menu_idx == 0 ? ">" : " ");
    draw_text(8, 11, "%s Options", main_menu_idx == 1 ? ">" : " ");
}

static inline void     draw_start_game_menu(void)
{
    int lines[] = {8, 9, 10, 11, 12};
    clear_text_lines(lines, 5);

    draw_text(8, 8, "Start Game");
    draw_text(8, 10, "%s Single Player", start_game_menu_idx == 0 ? ">" : " ");
    draw_text(8, 11, "%s Multiplayer", start_game_menu_idx == 1 ? ">" : " ");
    draw_text(8, 13, "Use A to confirm");
}

static inline void     draw_character_select(void)
{
    const int base_line = 8;

    if (is_multiplayer_mode)
    {
        int lines[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
        clear_text_lines(lines, 10);

        draw_text(8, base_line, "Character Select (Multiplayer)");
        for (int i = 0; i < CHARACTER_COUNT; ++i)
        {
            const char *tag = "   ";
            if (selected_character_p1 == i)
                tag = "P1 ";
            if (selected_character_p2 == i)
                tag = "P2 ";
            if (selected_character_p1 == i && selected_character_p2 == i)
                tag = "P1P2";

            draw_text(8, base_line + 1 + i, "%s %s", tag, character_name[i]);
        }

        draw_text(8, base_line + 6, "P% d selected: %s", character_select_active_player, character_name[(character_select_active_player == 1) ? selected_character_p1 : selected_character_p2]);
        draw_text(8, base_line + 7, "UP/DOWN: switch P1/P2  L/R: choose char");
        draw_text(8, base_line + 8, "A: confirm  B: back");
    }
    else
    {
        int lines[] = {8, 9, 10, 11, 12, 13, 14};
        clear_text_lines(lines, 7);

        draw_text(8, base_line, "Character Select (Single)");
        for (int i = 0; i < CHARACTER_COUNT; ++i)
        {
            draw_text(8, base_line + 1 + i, "%s %s", i == selected_character ? ">" : " ", character_name[i]);
        }

        draw_text(8, base_line + 6, "LEFT/RIGHT: change char  A: start  B: back");
    }
}

static inline void     draw_character_test_screen(void)
{
    unsigned int cart_total_kb = ram_cart_is_ok() ? ram_cart_size_bytes() / 1024 : 0;
    unsigned int cart_used_kb = ram_cart_is_ok() ? ram_cart_used_bytes() / 1024 : 0;
    unsigned int dynamic_kb = character_dynamic_memory_used() / 1024;
    unsigned int sprite_kb = character_sprites_memory_used() / 1024;
    int current_row = character_test_cart_current_row();
    int total_rows = character_test_cart_total_rows();
    int current_pixel = character_test_cart_current_pixel();

    draw_text(0, 3, "Test: %s", character_name[selected_character]);
    draw_text(0, 4, "RAM Cart: %u / %u KB", cart_used_kb, cart_total_kb);
    draw_text(0, 5, "Dynamic: %u KB (%d%%)", dynamic_kb, jo_memory_usage_percent());
    draw_text(0, 6, "Sprite: %u KB (%d%%)", sprite_kb, jo_sprite_usage_percent());
    draw_text(0, 8, "Step 0: idle");
    draw_text(0, 9, "Step 1: TGA -> RAM Cart");
    draw_text(0, 10, "Step 2: Row 0 -> WRAM");
    draw_text(0, 11, "Step 3: Frame 0 -> VRAM");
    draw_text(0, 12, "Step1 log: %s", ram_cart_is_verbose_pixel_logging_enabled() ? "detailed" : "fast");
    draw_text(0, 13, "Current step: %d", character_test_step);
    draw_text(0, 14, "%s", character_get_last_error());
    draw_text(0, 15, "%s", ram_cart_get_last_error());
    draw_text(0, 16, "Stage: %s %d/%d", character_test_cart_stage(), current_row, total_rows);
    draw_text(0, 17, "Pixel: %d", current_pixel);
    draw_text(0, 18, "Pending: %s", character_test_action_pending ? "yes" : "no ");
    draw_text(0, 19, "A: next step  B: reset");
    draw_text(0, 20, "X: toggle log  START: back");

    if (character_test_step >= 3)
        character_test_draw_preview(184, 44);
}

static inline void     draw_pause_menu(void)
{
    int lines[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    clear_text_lines(lines, 10);

    draw_text(8, 8, "--- Paused ---");
    draw_text(8, 10, "%s Resume", pause_menu_idx == 0 ? ">" : " ");
    draw_text(8, 11, "%s Character select", pause_menu_idx == 1 ? ">" : " ");
    draw_text(8, 12, "%s RAM metrics %s", pause_menu_idx == 2 ? ">" : " ", show_memory_stats ? "On" : "Off");
    draw_text(8, 13, "%s Debug overlay %s", pause_menu_idx == 3 ? ">" : " ", show_debug_overlay ? "On" : "Off");
    if (show_memory_stats)
    {
        unsigned int sprite_kb = character_sprites_memory_used() / 1024;
        unsigned int cart_used_kb = ram_cart_is_ok() ? ram_cart_used_bytes() / 1024 : 0;
        unsigned int cart_total_kb = ram_cart_is_ok() ? ram_cart_size_bytes() / 1024 : 0;

        draw_text(8, 15, "Sprite mem: %u%%", jo_sprite_usage_percent());
        //draw_text(8, 15, "Sprite mem: %u KB", sprite_kb);
        draw_text(8, 16, "Dynamic mem: %u%%", jo_memory_usage_percent());
        draw_text(8, 17, "Cart RAM: %u KB / %u KB", cart_used_kb, cart_total_kb);
    }
}

static inline void     camera_handling(int prev_y)
{
    int         delta;

    delta = JO_ABS(player.y - prev_y);
    if (player.y > 100)
    {
        map_pos_y += delta;
        player.y -= delta;
    }
    else if (player.y < 50)
    {
        map_pos_y -= delta;
        player.y += delta;
    }
}


static inline void     game_draw(void)
{
    int         prev_y;
    bool        left;
    bool        right;
    bool        jump_button;
    bool        action_button;

    if (!show_debug_overlay)
    {
        int overlay_lines[] = {0, 1, 2, 3};

        clear_text_lines(overlay_lines, 4);
    }

    if (show_debug_overlay)
    {
        jo_printf(0, 2, "GAME mx=%d my=%d", map_pos_x, map_pos_y);
        jo_printf(0, 3, "PLY  x=%d y=%d", player.x, player.y);
    }

    jo_map_draw(WORLD_MAP_ID, map_pos_x, map_pos_y);
    prev_y = player.y;
    sonic_collision_handling();
    camera_handling(prev_y);

    left = jo_is_pad1_key_pressed(JO_KEY_LEFT);
    right = jo_is_pad1_key_pressed(JO_KEY_RIGHT);
    jump_button = jo_is_pad1_key_pressed(JO_KEY_A);
    bool x_button = jo_is_pad1_key_pressed(JO_KEY_X);
    action_button = left || right || jump_button || x_button ||
                    jo_is_pad1_key_pressed(JO_KEY_B) ||
                    jo_is_pad1_key_pressed(JO_KEY_C) ||
                    jo_is_pad1_key_pressed(JO_KEY_Y) ||
                    jo_is_pad1_key_pressed(JO_KEY_Z);

    character_update_movement(left, right, jump_button, x_button);
    character_sync_animation(physics.is_in_air, physics.speed, physics.speed_y);

    character_draw();
}

/* forward declarations to avoid implicit declaration warnings */
static bool      load_game_background(void);
static bool      restore_game_background(void);
static bool      load_menu_background(void);
static bool      cache_game_background(void);
static void      save_current_game_background(void);
static void      draw_loading_screen(void);
void            load_map(void);
void            load_background(void);

static void process_game_loading(void)
{
    if (loading_phase == 1)
    {
        load_map();
        loading_phase = 2;
    }
    else if (loading_phase == 2)
    {
        if (character_load())
            loading_phase = 3;
        else
        {
            draw_text(8, 10, "Character load failed");
            draw_text(8, 11, "%s", character_get_last_error());
            draw_text(8, 12, "%s", ram_cart_get_last_error());
        }
    }
    else if (loading_phase == 3)
    {
        int loading_lines[] = {24, 25, 26, 27};

        if (!restore_game_background() && show_debug_overlay)
            draw_text(0, 20, "[SONIC] ERRO: Nao foi possivel restaurar BG.TGA");

        jo_physics_init_for_sonic(&physics);
        reset_demo();
        clear_text_lines(loading_lines, 4);
        game_started = true;
        loading_phase = 0;
    }
}

static inline void     my_draw(void)
{
    static int   fps = 0;
    static int   frame_count = 0;
    static int   last_tick = 0;
    int          tick = jo_get_ticks();

    frame_count++;
    if (tick - last_tick >= 1000)
    {
        fps = frame_count;
        frame_count = 0;
        last_tick = tick;
    }

    if (show_debug_overlay)
        jo_printf(0, 0, "FPS:%03d", fps);

    if (is_boot_screen)
    {
        ram_cart_draw_boot_screen();
        return;
    }

    if (is_main_menu)
    {
        draw_main_menu();
        return;
    }

    if (is_start_game_menu)
    {
        draw_start_game_menu();
        return;
    }

    if (is_options_menu)
    {
        int lines[] = {8, 9, 10, 11};
        clear_text_lines(lines, 4);
        draw_text(8, 8, "Options menu (placeholder)");
        draw_text(8, 10, "B: Back");
        return;
    }

    if (is_character_select)
    {
        draw_character_select();
        return;
    }

    if (is_character_test)
    {
        process_character_test_screen();
        draw_character_test_screen();
        return;
    }

    if (is_game_paused)
    {
        draw_pause_menu();
        return;
    }

    if (loading_phase != 0)
    {
        draw_loading_screen();

        if (show_debug_overlay)
        {
            draw_text(0, 26, "Loading phase: %d", loading_phase);
            if (loading_phase == 3)
            {
                draw_text(0, 24, "%s", character_get_last_error());
                draw_text(0, 25, "%s", ram_cart_get_last_error());
            }
        }
        process_game_loading();
        return;
    }

    if (pending_resume_sprite_refresh)
    {
        character_refresh_sprite();
        pending_resume_sprite_refresh = false;
    }

    game_draw();
}

static inline void     make_sonic_jump(void)
{
    player.can_jump = false;
    player.spin = true;
    jo_physics_jump(&physics);
}

static inline void     my_input(void)
{
    static bool last_up = false;
    static bool last_down = false;
    static bool last_left = false;
    static bool last_right = false;
    static bool last_a = false;
    static bool last_b = false;
    static bool last_c = false;
    static bool last_x = false;
    static bool last_y = false;
    static bool last_z = false;
    static bool last_start = false;

    bool up = jo_is_pad1_key_pressed(JO_KEY_UP);
    bool down = jo_is_pad1_key_pressed(JO_KEY_DOWN);
    bool left = jo_is_pad1_key_pressed(JO_KEY_LEFT);
    bool right = jo_is_pad1_key_pressed(JO_KEY_RIGHT);
    bool a = jo_is_pad1_key_pressed(JO_KEY_A);
    bool b = jo_is_pad1_key_pressed(JO_KEY_B);
    bool c = jo_is_pad1_key_pressed(JO_KEY_C);
    bool x = jo_is_pad1_key_pressed(JO_KEY_X);
    bool y = jo_is_pad1_key_pressed(JO_KEY_Y);
    bool z = jo_is_pad1_key_pressed(JO_KEY_Z);
    bool start = jo_is_pad1_key_pressed(JO_KEY_START);

    bool up_edge = up && !last_up;
    bool down_edge = down && !last_down;
    bool left_edge = left && !last_left;
    bool right_edge = right && !last_right;
    bool a_edge = a && !last_a;
    bool x_edge = x && !last_x;
    bool start_edge = start && !last_start;

    if (is_boot_screen)
    {
        if (start_edge)
        {
            if (!ram_cart_is_ok() && show_debug_overlay)
                draw_text(0, 17, "[SONIC] Sem Cart RAM, forcando start");

            ram_cart_defocus_boot_text();
            is_boot_screen = false;
            is_main_menu = true;
            load_menu_background();
            main_menu_idx = 0;
        }

        last_up = up;
        last_down = down;
        last_left = left;
        last_right = right;
        last_a = a;
        last_start = start;
        return;
    }

    if (is_main_menu)
    {
        if (up_edge)
            main_menu_idx = (main_menu_idx + 1) % 2;
        else if (down_edge)
            main_menu_idx = (main_menu_idx + 1) % 2;

        if (a_edge)
        {
            if (main_menu_idx == 0)
            {
                is_main_menu = false;
                is_start_game_menu = true;
                start_game_menu_idx = 0;
            }
            else
            {
                // Options menu not implemented yet
                is_options_menu = true;
            }
        }

        last_up = up;
        last_down = down;
        last_left = left;
        last_right = right;
        last_a = a;
        last_start = start;
        return;
    }

    if (is_start_game_menu)
    {
        if (up_edge)
            start_game_menu_idx = (start_game_menu_idx + 1) % 2;
        else if (down_edge)
            start_game_menu_idx = (start_game_menu_idx + 1) % 2;

        if (a_edge)
        {
            is_start_game_menu = false;
            is_character_select = true;
            if (start_game_menu_idx == 0)
            {
                is_multiplayer_mode = false;
                selected_character = CHARACTER_SONIC;
            }
            else
            {
                is_multiplayer_mode = true;
                selected_character_p1 = CHARACTER_SONIC;
                selected_character_p2 = CHARACTER_AMY;
                character_select_active_player = 1;
            }
            character_choose(selected_character);
        }

        if (b)
        {
            is_start_game_menu = false;
            is_main_menu = true;
            main_menu_idx = 0;
        }

        last_up = up;
        last_down = down;
        last_left = left;
        last_right = right;
        last_a = a;
        last_b = b;
        last_start = start;
        return;
    }

    if (is_character_select)
    {
        if (b)
        {
            is_character_select = false;
            is_start_game_menu = true;
            is_main_menu = false;
            is_options_menu = false;
            main_menu_idx = 0;
            start_game_menu_idx = 0;
            last_up = up;
            last_down = down;
            last_left = left;
            last_right = right;
            last_a = a;
            last_b = b;
            last_start = start;
            return;
        }

        if (is_multiplayer_mode)
        {
            if (up_edge || down_edge)
                character_select_active_player = (character_select_active_player == 1) ? 2 : 1;

            if (up_edge)
            {
                if (character_select_active_player == 1)
                    selected_character_p1 = (selected_character_p1 + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
                else
                    selected_character_p2 = (selected_character_p2 + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
            }
            else if (down_edge)
            {
                if (character_select_active_player == 1)
                    selected_character_p1 = (selected_character_p1 + 1) % CHARACTER_COUNT;
                else
                    selected_character_p2 = (selected_character_p2 + 1) % CHARACTER_COUNT;
            }

            if (a_edge)
            {
                int lines[] = {8, 10, 11, 12};
                clear_text_lines(lines, 4);
                draw_text(8, 8, "Multiplayer selection pending implementation.");
            }
        }
        else
        {
            if (up_edge)
            {
                selected_character = (selected_character + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
                character_choose(selected_character);
            }
            else if (down_edge)
            {
                selected_character = (selected_character + 1) % CHARACTER_COUNT;
                character_choose(selected_character);
            }
            else if (a_edge)
            {
                int lines[] = {8, 9, 10, 11, 12, 13, 14, 15, 16};
                clear_text_lines(lines, 9);
                is_character_select = false;
                is_game_paused = false;
                game_started = false;
                pending_resume_sprite_refresh = false;
                if (!cache_game_background() && show_debug_overlay)
                    draw_text(0, 20, "[SONIC] ERRO: Nao foi possivel carregar BG.TGA");
                loading_phase = 1;
            }
        }

        last_up = up;
        last_down = down;
        last_left = left;
        last_right = right;
        last_a = a;
        last_b = b;
        last_start = start;
        return;
    }

    if (is_character_test)
    {
        if (a_edge)
        {
            if (!character_test_action_pending && !character_test_store_active && character_test_step < 3)
            {
                character_test_action_pending = true;
                character_test_pending_step = character_test_step;
                character_test_action_delay = 1;
            }
        }
        else if (b)
        {
            character_test_step = 0;
            character_test_store_active = false;
            character_test_action_pending = false;
            character_test_pending_step = -1;
            character_test_action_delay = 0;
            character_test_prepare();
        }
        else if (x_edge)
        {
            ram_cart_set_verbose_pixel_logging(!ram_cart_is_verbose_pixel_logging_enabled());
        }
        else if (start_edge)
        {
            int lines[] = {3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

            clear_text_lines(lines, 17);
            is_character_test = false;
            is_character_select = true;
            load_menu_background();
            character_test_step = 0;
            character_test_store_active = false;
            character_test_action_pending = false;
            character_test_pending_step = -1;
            character_test_action_delay = 0;
            character_test_prepare();
        }

        last_up = up;
        last_down = down;
        last_left = left;
        last_right = right;
        last_a = a;
        last_b = b;
        last_c = c;
        last_x = x;
        last_y = y;
        last_z = z;
        last_start = start;
        return;
    }

    if (is_game_paused)
    {
        if (up_edge)
            pause_menu_idx = (pause_menu_idx + 3) % 4;
        else if (down_edge)
            pause_menu_idx = (pause_menu_idx + 1) % 4;

        if (a_edge)
        {
            int pause_lines[] = {8, 10, 11, 12, 13, 15, 16, 17};
            if (pause_menu_idx == 0)
            {
                clear_text_lines(pause_lines, 8);
                is_game_paused = false;

                if (!restore_game_background() && show_debug_overlay)
                    draw_text(0, 23, "[SONIC] Aviso: falha restore_game_background() no resume");

                pending_resume_sprite_refresh = true;
            }
            else if (pause_menu_idx == 1)
            {
                clear_text_lines(pause_lines, 8);
                is_game_paused = false;
                is_character_select = true;
                game_started = false;
                pending_resume_sprite_refresh = false;
                character_unload();
                load_menu_background();
            }
            else if (pause_menu_idx == 2)
            {
                show_memory_stats = !show_memory_stats;
            }
            else if (pause_menu_idx == 3)
            {
                show_debug_overlay = !show_debug_overlay;
            }
        }

        last_up = up;
        last_down = down;
        last_left = left;
        last_right = right;
        last_a = a;
        last_start = start;
        return;
    }

    if (start_edge)
    {
        save_current_game_background();
        pending_resume_sprite_refresh = false;
        is_game_paused = true;
        pause_menu_idx = 0;
        load_menu_background();
    }

    if (physics.is_in_air)
    {
        if (jo_is_pad1_key_pressed(JO_KEY_DOWN))
            player.spin = true;
        if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
        {
            player.flip_sonic = true;
            jo_physics_accelerate_left(&physics);
        }
        else if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
        {
            player.flip_sonic = false;
            jo_physics_accelerate_right(&physics);
        }
    }

    if (a_edge && player.can_jump && !is_game_paused && loading_phase == 0)
        make_sonic_jump();

    if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
    {
        player.flip_sonic = true;
        if (jo_physics_is_going_on_the_right(&physics) || jo_physics_should_brake(&physics))
            jo_physics_decelerate_left(&physics);
        else
            jo_physics_accelerate_left(&physics);
    }
    else if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
    {
        player.flip_sonic = false;
        if (jo_physics_is_going_on_the_left(&physics) || jo_physics_should_brake(&physics))
            jo_physics_decelerate_right(&physics);
        else
            jo_physics_accelerate_right(&physics);
    }
    else
    {
        jo_physics_apply_friction(&physics);
    }

    if (jo_is_pad1_key_down(JO_KEY_START))
        reset_demo();

    last_up = up;
    last_down = down;
    last_left = left;
    last_right = right;
    last_a = a;
    last_b = b;
    last_c = c;
    last_x = x;
    last_y = y;
    last_z = z;
    last_start = start;
}

static bool try_map_load(const char *subdir, const char *filename)
{
    if (!jo_map_load_from_file(WORLD_MAP_ID, 500, subdir, filename))
        return (false);
    if (show_debug_overlay)
        draw_text(0, 19, "[SONIC] Map carregado: %s/%s", subdir ? subdir : "root", filename);
    return (true);
}

static bool try_map_load_fullpath(const char *path)
{
    if (!jo_map_load_from_file(WORLD_MAP_ID, 500, JO_ROOT_DIR, path))
        return (false);
    if (show_debug_overlay)
        draw_text(0, 19, "[SONIC] Map carregado: %s", path);
    return (true);
}

static bool ensure_background_buffer(jo_img *img)
{
    if (img == JO_NULL)
        return false;

    if (img->data == JO_NULL)
        img->data = (jo_color *)jo_malloc(JO_TV_WIDTH * JO_TV_HEIGHT * sizeof(jo_color));

    return img->data != JO_NULL;
}

static bool load_bg_image_from_cd(jo_img *img, const char *filename)
{
    bool load_ok = false;
    const char *ext;

    if (img == JO_NULL || filename == JO_NULL)
        return false;

    img->data = JO_NULL;
    ext = filename + strlen(filename);
    while (ext > filename && *ext != '.')
        --ext;

    if (ext > filename && strcasecmp(ext, ".TGA") == 0)
        load_ok = jo_tga_loader(img, "BG", filename, JO_COLOR_Transparent) == JO_TGA_OK;
    else
        load_ok = jo_bin_loader(img, "BG", filename, JO_COLOR_Transparent);

    return load_ok;
}

static bool blit_background_to_screen(const jo_img *img)
{
    int copy_width;
    int copy_height;
    int src_x;
    int src_y;
    int dst_x;
    int dst_y;

    if (img == JO_NULL || img->data == JO_NULL || nbg1_bitmap == JO_NULL)
        return false;

    jo_clear_background(JO_COLOR_Black);

    copy_width = img->width < JO_TV_WIDTH ? img->width : JO_TV_WIDTH;
    copy_height = img->height < JO_TV_HEIGHT ? img->height : JO_TV_HEIGHT;
    src_x = img->width > JO_TV_WIDTH ? (img->width - JO_TV_WIDTH) / 2 : 0;
    src_y = img->height > JO_TV_HEIGHT ? (img->height - JO_TV_HEIGHT) / 2 : 0;
    dst_x = img->width < JO_TV_WIDTH ? (JO_TV_WIDTH - img->width) / 2 : 0;
    dst_y = img->height < JO_TV_HEIGHT ? (JO_TV_HEIGHT - img->height) / 2 : 0;

    for (int y = 0; y < copy_height; ++y)
    {
        jo_color *src_row = img->data + (src_y + y) * img->width + src_x;
        jo_color *dst_row = nbg1_bitmap + (dst_y + y) * JO_VDP2_WIDTH + dst_x;

        for (int x = 0; x < copy_width; ++x)
            dst_row[x] = src_row[x];
    }

    return true;
}

static bool copy_background_to_visible_buffer(const jo_img *src, jo_img *dst)
{
    int copy_width;
    int copy_height;
    int src_x;
    int src_y;
    int dst_x;
    int dst_y;

    if (src == JO_NULL || src->data == JO_NULL || !ensure_background_buffer(dst))
        return false;

    dst->width = JO_TV_WIDTH;
    dst->height = JO_TV_HEIGHT;

    for (int index = 0; index < JO_TV_WIDTH * JO_TV_HEIGHT; ++index)
        dst->data[index] = JO_COLOR_Black;

    copy_width = src->width < JO_TV_WIDTH ? src->width : JO_TV_WIDTH;
    copy_height = src->height < JO_TV_HEIGHT ? src->height : JO_TV_HEIGHT;
    src_x = src->width > JO_TV_WIDTH ? (src->width - JO_TV_WIDTH) / 2 : 0;
    src_y = src->height > JO_TV_HEIGHT ? (src->height - JO_TV_HEIGHT) / 2 : 0;
    dst_x = src->width < JO_TV_WIDTH ? (JO_TV_WIDTH - src->width) / 2 : 0;
    dst_y = src->height < JO_TV_HEIGHT ? (JO_TV_HEIGHT - src->height) / 2 : 0;

    for (int y = 0; y < copy_height; ++y)
    {
        jo_color *src_row = src->data + (src_y + y) * src->width + src_x;
        jo_color *dst_row = dst->data + (dst_y + y) * JO_TV_WIDTH + dst_x;

        for (int x = 0; x < copy_width; ++x)
            dst_row[x] = src_row[x];
    }

    return true;
}

static void save_current_game_background(void)
{
    if (nbg1_bitmap == JO_NULL)
        return;

    if (!ensure_background_buffer(&saved_game_background))
        return;

    saved_game_background.width = JO_TV_WIDTH;
    saved_game_background.height = JO_TV_HEIGHT;

    for (int y = 0; y < JO_TV_HEIGHT; ++y)
    {
        jo_color *src_row = nbg1_bitmap + y * JO_VDP2_WIDTH;
        jo_color *dst_row = saved_game_background.data + y * JO_TV_WIDTH;

        for (int x = 0; x < JO_TV_WIDTH; ++x)
            dst_row[x] = src_row[x];
    }

    saved_game_background_valid = true;
}

static bool restore_saved_game_background(void)
{
    if (!saved_game_background_valid)
        return (false);

    return blit_background_to_screen(&saved_game_background);
}

static bool restore_game_background(void)
{
    if (saved_game_background_valid)
        return restore_saved_game_background();

    return cache_game_background() && restore_saved_game_background();
}

static bool cache_game_background(void)
{
    static jo_img bg = {0};
    bool load_ok;

    if (saved_game_background_valid)
        return true;

    if (bg.data != JO_NULL)
    {
        jo_free_img(&bg);
        bg.width = 0;
        bg.height = 0;
        bg.data = JO_NULL;
    }

    bg.data = JO_NULL;
    load_ok = load_bg_image_from_cd(&bg, "BG.TGA");
    if (!load_ok)
        return false;

    if (!copy_background_to_visible_buffer(&bg, &saved_game_background))
    {
        jo_free_img(&bg);
        bg.width = 0;
        bg.height = 0;
        bg.data = JO_NULL;
        saved_game_background_valid = false;
        return false;
    }

    saved_game_background_valid = true;
    jo_free_img(&bg);
    bg.width = 0;
    bg.height = 0;
    bg.data = JO_NULL;
    return true;
}

static void draw_loading_screen(void)
{
    jo_clear_background(JO_COLOR_Black);
    jo_clear_screen();
    draw_text(0, 27, "Loading...");
}

static bool try_bg_load_to_screen(const char *filename, bool save_for_resume)
{
    static jo_img bg = {0};
    bool load_ok;

    if (bg.data != JO_NULL)
    {
        jo_free_img(&bg);
        bg.width = 0;
        bg.height = 0;
        bg.data = JO_NULL;
    }

    bg.data = JO_NULL;
    load_ok = load_bg_image_from_cd(&bg, filename);

    if (!load_ok)
    {
        if (show_debug_overlay)
            draw_text(0, 21, "BG load failed: BG/%s", filename);
        return false;
    }

    if (!blit_background_to_screen(&bg))
    {
        jo_free_img(&bg);
        bg.width = 0;
        bg.height = 0;
        return false;
    }

    if (save_for_resume)
    {
        if (copy_background_to_visible_buffer(&bg, &saved_game_background))
            saved_game_background_valid = true;
        else
            saved_game_background_valid = false;
    }

    jo_free_img(&bg);
    bg.width = 0;
    bg.height = 0;

    if (show_debug_overlay)
        draw_text(0, 20, "[SONIC] BG carregado: BG/%s", filename);

    return true;
}

static bool load_game_background(void)
{
    return cache_game_background() && restore_saved_game_background();
}

static bool load_menu_background(void)
{
    static jo_img bg = {0};

    if (cached_menu_background_valid && cached_menu_background.data != JO_NULL)
    {
        if (!blit_background_to_screen(&cached_menu_background))
            return false;

        if (show_debug_overlay)
            draw_text(0, 20, "[SONIC] Menu BG carregado: BGG_MEN.TGA");
        return true;
    }

    if (bg.data != JO_NULL)
    {
        jo_free_img(&bg);
        bg.width = 0;
        bg.height = 0;
        bg.data = JO_NULL;
    }

    bg.data = JO_NULL;
    if (!load_bg_image_from_cd(&bg, "BGG_MEN.TGA"))
        return load_game_background();

    if (!ensure_background_buffer(&cached_menu_background) ||
        !copy_background_to_visible_buffer(&bg, &cached_menu_background) ||
        !blit_background_to_screen(&cached_menu_background))
    {
        jo_free_img(&bg);
        bg.width = 0;
        bg.height = 0;
        cached_menu_background_valid = false;
        return false;
    }

    cached_menu_background_valid = true;
    jo_free_img(&bg);
    bg.width = 0;
    bg.height = 0;

    if (cached_menu_background.data != JO_NULL)
    {
        if (show_debug_overlay)
            draw_text(0, 20, "[SONIC] Menu BG carregado: BGG_MEN.TGA");
        return true;
    }

    return load_game_background();
}

void		load_map(void)
{
    if (map_content_loaded)
        return;

    if (!map_tiles_loaded &&
        jo_sprite_add_image_pack("BLK", "BLK.TEX", JO_COLOR_Red) < 0 &&
        jo_sprite_add_image_pack("cd/BLK", "BLK.TEX", JO_COLOR_Red) < 0)
    {
        if (show_debug_overlay)
            draw_text(0, 18, "[SONIC] ERRO: Nao foi possivel carregar BLK.TEX");
        return;
    }

    map_tiles_loaded = true;

    if (try_map_load("MAP", "DEMO2.MAP") ||
        try_map_load("MAP", "DEMO.MAP") ||
        try_map_load("cd/MAP", "DEMO2.MAP") ||
        try_map_load("cd/MAP", "DEMO.MAP") ||
        try_map_load_fullpath("MAP/DEMO2.MAP") ||
        try_map_load_fullpath("MAP/DEMO.MAP") ||
        try_map_load_fullpath("cd/MAP/DEMO2.MAP") ||
        try_map_load_fullpath("cd/MAP/DEMO.MAP"))
    {
        map_content_loaded = true;
        return;
    }

    if (show_debug_overlay)
        draw_text(0, 19, "[SONIC] ERRO: Nao foi possivel carregar mapa");
}

void		load_background(void)
{
    if (!load_game_background() && show_debug_overlay)
        draw_text(0, 20, "[SONIC] ERRO: Nao foi possivel carregar BG.TGA");
}


void			jo_main(void)
{
	jo_core_init(JO_COLOR_Black);

    check_cartridge_ram();
    character_choose(CHARACTER_SONIC);

    jo_core_add_callback(my_draw);
    jo_core_add_callback(my_input);
    jo_core_run();
}

/*
** END OF FILE
*/
