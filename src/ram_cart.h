#ifndef __RAM_CART_H__
# define __RAM_CART_H__

# include <stdbool.h>
# include <stdint.h>
# include <stddef.h>
# include <jo/jo.h>

typedef enum
{
    RamCartStatusNotDetected,
    RamCartStatus1MB,
    RamCartStatus4MB
} ram_cart_status_t;

typedef enum
{
    RamCartTgaDebugIdle,
    RamCartTgaDebugInProgress,
    RamCartTgaDebugDone,
    RamCartTgaDebugFailed
} ram_cart_tga_debug_status_t;

ram_cart_status_t ram_cart_detect(void);
ram_cart_status_t ram_cart_get_status(void);
uint32_t          ram_cart_get_total_size(void);
uint32_t          ram_cart_get_used_size(void);

bool ram_cart_has_entry(const char *name);
bool ram_cart_get_sprite_dimensions(const char *name, int *width, int *height);
bool ram_cart_set_sprite_dimensions(const char *name, int width, int height);

bool ram_cart_store_sprite(const char *name, const void *data, size_t size);
bool ram_cart_store_tga(const char *name, const char *filename, const char *dir);
bool ram_cart_begin_store_tga(const char *name, const char *filename, const char *dir);
ram_cart_tga_debug_status_t ram_cart_step_store_tga(int max_rows);
void ram_cart_cancel_store_tga(void);
void ram_cart_set_verbose_pixel_logging(bool enabled);
bool ram_cart_is_verbose_pixel_logging_enabled(void);
int ram_cart_tga_debug_current_row(void);
int ram_cart_tga_debug_total_rows(void);
const char *ram_cart_tga_debug_stage(void);
int ram_cart_tga_debug_current_pixel(void);
bool ram_cart_store_sprite_img(const char *name, const jo_img *img);
bool ram_cart_load_sprite(const char *name, void *out_buffer, size_t size);
bool ram_cart_load_region(const char *name, int sheet_width, int sheet_height,
    int start_x, int start_y, int width, int height, void *out_buffer, size_t out_size);
bool ram_cart_delete_sprite(const char *name);
void ram_cart_clear(void);
bool ram_cart_stream_frame(const char *name, int sheet_width, int sheet_height,
    int frame_x, int frame_y, int frame_width, int frame_height,
    int target_width, int sprite_id);
bool ram_cart_draw_frame(const char *name, int sheet_width, int sheet_height,
    int frame_x, int frame_y, int frame_width, int frame_height,
    int target_width, int sprite_id);
const char      *ram_cart_get_last_error(void);

/* Compatibility wrappers used by Sonic SS demo UI */
bool            check_cartridge_ram(void);
bool            ram_cart_is_ok(void);
unsigned int    ram_cart_size_bytes(void);
unsigned int    ram_cart_used_bytes(void);
void            ram_cart_add_used_bytes(unsigned int bytes);
void            ram_cart_subtract_used_bytes(unsigned int bytes);
void            ram_cart_reset_used_bytes(void);
void            ram_cart_draw_boot_screen(void);
void            ram_cart_defocus_boot_text(void);

#endif /* !__RAM_CART_H__ */
