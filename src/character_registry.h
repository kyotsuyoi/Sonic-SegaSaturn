#ifndef CHARACTER_REGISTRY_H
#define CHARACTER_REGISTRY_H

#include "ui_control.h"
#include "character.h"

typedef struct
{
    void (*load)(void);
    void (*unload)(void);
    void (*update_animation)(void);
    void (*display)(void);
} character_handlers_t;

typedef struct
{
    int move_count;
    int stand_count;
    int punch_count;
    int kick_count;
} character_animation_profile_t;

typedef struct
{
    int hit_range_punch1;
    int hit_range_punch2;
    int hit_range_kick1;
    int hit_range_kick2;
    float attack_forward_impulse_light;
    float attack_forward_impulse_heavy;
    float knockback_punch1;
    float knockback_punch2;
    float knockback_kick1;
    float knockback_kick2;
    bool charged_kick_enabled;
    int damage_punch1;
    int damage_punch2;
    int damage_kick1;
    int damage_kick2;
    int charged_kick_damage;
    int charged_kick_range_bonus;
    int charged_kick_stun_bonus;
    float charged_kick_knockback_mult;
} character_combat_profile_t;

character_handlers_t character_registry_get(ui_character_choice_t choice);
character_animation_profile_t character_registry_get_animation_profile(ui_character_choice_t choice);
character_combat_profile_t character_registry_get_combat_profile(ui_character_choice_t choice);
character_combat_profile_t character_registry_get_combat_profile_by_character_id(int character_id);
void character_registry_apply_combat_profile(character_t *character, ui_character_choice_t choice);

// Helpers for bot asset loading (character sprite definitions are owned by the character system).
const jo_tile *character_get_move_tiles(ui_character_choice_t choice, int *out_count);
const jo_tile *character_get_stand_tiles(ui_character_choice_t choice, int *out_count);
const jo_tile *character_get_punch_tiles(ui_character_choice_t choice, int *out_count);
const jo_tile *character_get_kick_tiles(ui_character_choice_t choice, int *out_count);
const jo_tile *character_get_tail_tiles(ui_character_choice_t choice, int *out_count);

const char *character_get_walk_tga(ui_character_choice_t choice);
const char *character_get_run1_tga(ui_character_choice_t choice);
const char *character_get_run2_tga(ui_character_choice_t choice);
const char *character_get_stand_tga(ui_character_choice_t choice);
const char *character_get_jump_tga(ui_character_choice_t choice);
const char *character_get_spin_tga(ui_character_choice_t choice);
const char *character_get_damage_tga(ui_character_choice_t choice);
const char *character_get_defeated_tga(ui_character_choice_t choice);
const jo_tile *character_get_defeated_tiles(int *out_count);
const char *character_get_punch_tga(ui_character_choice_t choice);
const char *character_get_kick_tga(ui_character_choice_t choice);
const char *character_get_tail_tga(ui_character_choice_t choice);

#endif
