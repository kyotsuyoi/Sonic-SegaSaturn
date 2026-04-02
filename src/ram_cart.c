#include <jo/jo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ram_cart.h"
#include "menu_text.h"
#include "jo/fs.h"

void cart_ram_log(const char *fmt, ...)
{
    (void)fmt;
}

void runtime_log(const char *fmt, ...)
{
    (void)fmt;
}

#define CART_RAM_DIR_OFFSET          (0)
#define CART_RAM_DIR_SIZE            (64 * 32)
#define CART_RAM_DATA_OFFSET         (CART_RAM_DIR_SIZE)
#define CART_RAM_MAX_ENTRIES         64
#define CART_RAM_BASE_ADDR           (0x02400000)
#define CART_RAM_1MB_BANK_SIZE       (0x80000)
#define CART_RAM_1MB_BANK1_ADDR      (CART_RAM_BASE_ADDR)
#define CART_RAM_1MB_BANK2_ADDR      (CART_RAM_BASE_ADDR + 0x00200000)
#define CART_RAM_4MB_LAST_WORD_ADDR  (CART_RAM_BASE_ADDR + 0x003FFFFE)

typedef struct
{
    bool      used;
    char      name[20];
    uint32_t  offset;
    uint32_t  width;
    uint32_t  height;
    uint32_t  size;
} ram_cart_entry_t;

static ram_cart_status_t g_ram_cart_status = RamCartStatusNotDetected;
static uint32_t          g_ram_cart_total_size = 0;
static uint32_t          g_ram_cart_next_free = CART_RAM_DATA_OFFSET;
static ram_cart_entry_t  g_ram_cart_entries[CART_RAM_MAX_ENTRIES];
static char              g_ram_cart_last_error[96] = "cart:init";
static bool              g_ram_cart_verbose_pixel_logging = false;

typedef struct
{
    ram_cart_tga_debug_status_t status;
    jo_file file;
    bool file_open;
    ram_cart_entry_t *entry;
    unsigned char *src_row;
    unsigned short *decoded_row;
    char name[20];
    char filename[20];
    char dir[16];
    int width;
    int height;
    int pixel_depth;
    int bytes_per_pixel;
    int id_length;
    int current_row;
    int current_pixel;
    int phase;
    bool top_left;
    char stage[32];
} ram_cart_tga_debug_state_t;

static ram_cart_tga_debug_state_t g_ram_cart_tga_debug = { RamCartTgaDebugIdle };

static void ram_cart_set_last_error(const char *message)
{
    if (message == JO_NULL)
        return;

    strncpy(g_ram_cart_last_error, message, sizeof(g_ram_cart_last_error) - 1);
    g_ram_cart_last_error[sizeof(g_ram_cart_last_error) - 1] = '\0';
}

const char *ram_cart_get_last_error(void)
{
    return g_ram_cart_last_error;
}

void ram_cart_set_verbose_pixel_logging(bool enabled)
{
    g_ram_cart_verbose_pixel_logging = enabled;
}

bool ram_cart_is_verbose_pixel_logging_enabled(void)
{
    return g_ram_cart_verbose_pixel_logging;
}

static void ram_cart_tga_debug_set_stage(const char *stage)
{
    if (stage == JO_NULL)
        return;

    strncpy(g_ram_cart_tga_debug.stage, stage, sizeof(g_ram_cart_tga_debug.stage) - 1);
    g_ram_cart_tga_debug.stage[sizeof(g_ram_cart_tga_debug.stage) - 1] = '\0';
}

static void ram_cart_tga_debug_reset_state(void)
{
    g_ram_cart_tga_debug.status = RamCartTgaDebugIdle;
    g_ram_cart_tga_debug.file_open = false;
    g_ram_cart_tga_debug.entry = JO_NULL;
    g_ram_cart_tga_debug.src_row = JO_NULL;
    g_ram_cart_tga_debug.decoded_row = JO_NULL;
    g_ram_cart_tga_debug.name[0] = '\0';
    g_ram_cart_tga_debug.filename[0] = '\0';
    g_ram_cart_tga_debug.dir[0] = '\0';
    g_ram_cart_tga_debug.width = 0;
    g_ram_cart_tga_debug.height = 0;
    g_ram_cart_tga_debug.pixel_depth = 0;
    g_ram_cart_tga_debug.bytes_per_pixel = 0;
    g_ram_cart_tga_debug.id_length = 0;
    g_ram_cart_tga_debug.current_row = 0;
    g_ram_cart_tga_debug.current_pixel = 0;
    g_ram_cart_tga_debug.phase = 0;
    g_ram_cart_tga_debug.top_left = false;
    ram_cart_tga_debug_set_stage("idle");
}

static void ram_cart_tga_debug_finish(bool keep_entry)
{
    if (g_ram_cart_tga_debug.decoded_row != JO_NULL)
        jo_free(g_ram_cart_tga_debug.decoded_row);
    if (g_ram_cart_tga_debug.src_row != JO_NULL)
        jo_free(g_ram_cart_tga_debug.src_row);
    if (g_ram_cart_tga_debug.file_open)
        jo_fs_close(&g_ram_cart_tga_debug.file);

    if (!keep_entry && g_ram_cart_tga_debug.name[0] != '\0')
        ram_cart_delete_sprite(g_ram_cart_tga_debug.name);

    ram_cart_tga_debug_reset_state();
}

int ram_cart_tga_debug_current_row(void)
{
    return g_ram_cart_tga_debug.current_row;
}

int ram_cart_tga_debug_total_rows(void)
{
    return g_ram_cart_tga_debug.height;
}

int ram_cart_tga_debug_current_pixel(void)
{
    return g_ram_cart_tga_debug.current_pixel;
}

const char *ram_cart_tga_debug_stage(void)
{
    return g_ram_cart_tga_debug.stage;
}

void ram_cart_cancel_store_tga(void)
{
    ram_cart_tga_debug_finish(false);
}

static int ram_cart_align_sprite_dimension(int value)
{
    if (value <= 0)
        return 0;
    if (value <= 8)
        return 8;
    if (value <= 16)
        return 16;
    if (value <= 32)
        return 32;
    if (value <= 64)
        return 64;
    if (value <= 128)
        return 128;
    return 256;
}

static bool ram_cart_resolve_address(uint32_t offset, uint32_t *address)
{
    if (address == JO_NULL)
        return false;

    if (g_ram_cart_status == RamCartStatus1MB)
    {
        if (offset >= 1024 * 1024)
            return false;

        if (offset < CART_RAM_1MB_BANK_SIZE)
            *address = CART_RAM_1MB_BANK1_ADDR + offset;
        else
            *address = CART_RAM_1MB_BANK2_ADDR + (offset - CART_RAM_1MB_BANK_SIZE);

        return true;
    }

    if (g_ram_cart_status == RamCartStatus4MB)
    {
        if (offset >= 4 * 1024 * 1024)
            return false;

        *address = CART_RAM_BASE_ADDR + offset;
        return true;
    }

    return false;
}

static uint32_t ram_cart_contiguous_chunk_size(uint32_t offset, size_t remaining)
{
    if (g_ram_cart_status != RamCartStatus1MB)
        return (uint32_t)remaining;

    if (offset < CART_RAM_1MB_BANK_SIZE)
    {
        uint32_t available = CART_RAM_1MB_BANK_SIZE - offset;
        return (remaining < available) ? (uint32_t)remaining : available;
    }

    if (offset < 1024 * 1024)
    {
        uint32_t available = (1024 * 1024) - offset;
        return (remaining < available) ? (uint32_t)remaining : available;
    }

    return 0;
}

static size_t ram_cart_normalized_name_length(const char *name)
{
    if (name == JO_NULL)
        return 0;

    size_t len = strlen(name);
    if (len >= 4 && strcasecmp(name + len - 4, ".TGA") == 0)
        return len - 4;

    return len;
}

static bool ram_cart_names_match(const char *left, const char *right)
{
    if (left == JO_NULL || right == JO_NULL)
        return false;

    size_t left_len = ram_cart_normalized_name_length(left);
    size_t right_len = ram_cart_normalized_name_length(right);

    if (left_len != right_len)
        return false;

    return strncasecmp(left, right, left_len) == 0;
}

static bool ram_cart_probe_word(volatile unsigned short *addr, unsigned short pattern)
{
    unsigned short old = *addr;
    *addr = pattern;
    bool ok = (*addr == pattern);
    *addr = old;
    return ok;
}

static bool ram_cart_probe_independent_words(volatile unsigned short *addr_a,
                                             volatile unsigned short *addr_b,
                                             unsigned short pattern)
{
    unsigned short old_a = *addr_a;
    unsigned short old_b = *addr_b;

    *addr_a = pattern;
    *addr_b = pattern ^ 0xFFFF;

    bool ok = (*addr_a == pattern && *addr_b == (unsigned short)(pattern ^ 0xFFFF));

    *addr_a = old_a;
    *addr_b = old_b;

    return ok;
}

static bool ram_cart_copy_to_cart(uint32_t offset, const void *src, size_t size)
{
    if (offset + size > g_ram_cart_total_size)
    {
        ram_cart_set_last_error("cart:write oob");
        return false;
    }

    if ((size & 1) != 0)
    {
        ram_cart_set_last_error("cart:write odd");
        return false;
    }

    const uint16_t *src_words = (const uint16_t *)src;
    size_t remaining = size;
    uint32_t current_offset = offset;

    while (remaining > 0)
    {
        uint32_t address;
        uint32_t chunk_size = ram_cart_contiguous_chunk_size(current_offset, remaining);
        volatile uint16_t *dst;
        size_t words;

        if (chunk_size == 0 || !ram_cart_resolve_address(current_offset, &address))
        {
            ram_cart_set_last_error("cart:write map");
            return false;
        }

        dst = (volatile uint16_t *)address;
        words = chunk_size / 2;

        for (size_t i = 0; i < words; ++i)
            dst[i] = src_words[i];

        src_words += words;
        current_offset += chunk_size;
        remaining -= chunk_size;
    }

    return true;
}

static bool ram_cart_copy_from_cart(uint32_t offset, void *dst, size_t size)
{
    if (offset + size > g_ram_cart_total_size)
    {
        ram_cart_set_last_error("cart:read oob");
        return false;
    }

    if ((size & 1) != 0)
    {
        ram_cart_set_last_error("cart:read odd");
        return false;
    }

    uint16_t *dst_words = (uint16_t *)dst;
    size_t remaining = size;
    uint32_t current_offset = offset;

    while (remaining > 0)
    {
        uint32_t address;
        uint32_t chunk_size = ram_cart_contiguous_chunk_size(current_offset, remaining);
        volatile uint16_t *src_words;
        size_t words;

        if (chunk_size == 0 || !ram_cart_resolve_address(current_offset, &address))
        {
            ram_cart_set_last_error("cart:read map");
            return false;
        }

        src_words = (volatile uint16_t *)address;
        words = chunk_size / 2;

        for (size_t i = 0; i < words; ++i)
            dst_words[i] = src_words[i];

        dst_words += words;
        current_offset += chunk_size;
        remaining -= chunk_size;
    }

    return true;
}

static ram_cart_entry_t *ram_cart_entry_table(void)
{
    return g_ram_cart_entries;
}

static ram_cart_entry_t *ram_cart_find_entry(const char *name)
{
    ram_cart_entry_t *table = ram_cart_entry_table();

    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
    {
        if (!table[i].used)
            continue;

        if (ram_cart_names_match(table[i].name, name))
            return &table[i];
    }

    return JO_NULL;
}

bool ram_cart_has_entry(const char *name)
{
    return ram_cart_find_entry(name) != JO_NULL;
}

bool ram_cart_get_sprite_dimensions(const char *name, int *width, int *height)
{
    if (name == JO_NULL || width == JO_NULL || height == JO_NULL)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
        return false;

    *width = (int)entry->width;
    *height = (int)entry->height;
    return true;
}

bool ram_cart_set_sprite_dimensions(const char *name, int width, int height)
{
    if (name == JO_NULL || width <= 0 || height <= 0)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
        return false;

    entry->width = (uint32_t)width;
    entry->height = (uint32_t)height;
    return true;
}

static ram_cart_entry_t *ram_cart_alloc_entry(const char *name)
{
    if (name == JO_NULL)
        return JO_NULL;

    ram_cart_entry_t *table = ram_cart_entry_table();

    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
    {
        if (!table[i].used)
        {
            table[i].used = true;
            table[i].offset = 0;
            table[i].size = 0;
            table[i].width = 0;
            table[i].height = 0;
            strncpy(table[i].name, name, sizeof(table[i].name) - 1);
            table[i].name[sizeof(table[i].name) - 1] = '\0';
            return &table[i];
        }
    }

    return JO_NULL;
}

static bool ram_cart_reserve_entry(const char *name, uint32_t size, ram_cart_entry_t **out_entry)
{
    ram_cart_entry_t *existing;
    ram_cart_entry_t *entry;

    if (name == JO_NULL || out_entry == JO_NULL || size == 0)
    {
        ram_cart_set_last_error("cart:reserve args");
        return false;
    }

    if ((size & 1) != 0)
    {
        ram_cart_set_last_error("cart:reserve odd");
        return false;
    }

    if (ram_cart_get_status() == RamCartStatusNotDetected)
    {
        ram_cart_set_last_error("cart:not detected");
        return false;
    }

    if (size + g_ram_cart_next_free > g_ram_cart_total_size)
    {
        ram_cart_clear();
        if (size + g_ram_cart_next_free > g_ram_cart_total_size)
        {
            ram_cart_set_last_error("cart:no space");
            return false;
        }
    }

    existing = ram_cart_find_entry(name);
    if (existing != JO_NULL)
    {
        if (existing->size >= size)
        {
            existing->size = size;
            existing->width = 0;
            existing->height = 0;
            *out_entry = existing;
            return true;
        }

        existing->used = false;
    }

    entry = ram_cart_alloc_entry(name);
    if (entry == JO_NULL)
    {
        ram_cart_set_last_error("cart:no dir slot");
        return false;
    }

    entry->offset = g_ram_cart_next_free;
    entry->size = size;
    g_ram_cart_next_free += size;
    *out_entry = entry;
    return true;
}

ram_cart_status_t ram_cart_detect(void)
{
    if (ram_cart_probe_word((volatile unsigned short *)CART_RAM_BASE_ADDR, 0x55AA))
    {
        if (ram_cart_probe_independent_words((volatile unsigned short *)CART_RAM_BASE_ADDR,
                                             (volatile unsigned short *)CART_RAM_4MB_LAST_WORD_ADDR,
                                             0x5AA5))
        {
            g_ram_cart_status = RamCartStatus4MB;
            g_ram_cart_total_size = 4 * 1024 * 1024;
            g_ram_cart_next_free = CART_RAM_DATA_OFFSET;
            memset(g_ram_cart_entries, 0, sizeof(g_ram_cart_entries));
            return g_ram_cart_status;
        }

        if (ram_cart_probe_independent_words((volatile unsigned short *)CART_RAM_BASE_ADDR,
                                             (volatile unsigned short *)CART_RAM_1MB_BANK2_ADDR,
                                             0xA55A))
        {
            g_ram_cart_status = RamCartStatus1MB;
            g_ram_cart_total_size = 1 * 1024 * 1024;
            g_ram_cart_next_free = CART_RAM_DATA_OFFSET;
            memset(g_ram_cart_entries, 0, sizeof(g_ram_cart_entries));
            return g_ram_cart_status;
        }
    }

    g_ram_cart_status = RamCartStatusNotDetected;
    g_ram_cart_total_size = 0;
    memset(g_ram_cart_entries, 0, sizeof(g_ram_cart_entries));
    return g_ram_cart_status;
}

ram_cart_status_t ram_cart_get_status(void)
{
    if (g_ram_cart_status == RamCartStatusNotDetected)
    {
        g_ram_cart_status = ram_cart_detect();
    }
    return g_ram_cart_status;
}

uint32_t ram_cart_get_total_size(void)
{
    if (g_ram_cart_status == RamCartStatusNotDetected)
        ram_cart_detect();
    return g_ram_cart_total_size;
}

uint32_t ram_cart_get_used_size(void)
{
    if (g_ram_cart_next_free <= CART_RAM_DATA_OFFSET)
        return 0;

    return g_ram_cart_next_free - CART_RAM_DATA_OFFSET;
}

bool ram_cart_store_sprite(const char *name, const void *data, size_t size)
{
    if (name == JO_NULL || data == JO_NULL || size == 0)
        return false;

    if ((size & 1) != 0)
    {
        cart_ram_log("ram_cart_store: %s bad size %u (must be even)", name, (unsigned)size);
        return false;
    }

    if (ram_cart_get_status() == RamCartStatusNotDetected)
    {
        runtime_log("ram_cart_store: not detected");
        return false;
    }

    if (size + g_ram_cart_next_free > g_ram_cart_total_size)
    {
        cart_ram_log("ram_cart_store: %s fails, no space (%u/%u), evicting old entries", name, (unsigned)g_ram_cart_next_free, (unsigned)g_ram_cart_total_size);
        ram_cart_clear();
        if (size + g_ram_cart_next_free > g_ram_cart_total_size)
        {
            cart_ram_log("ram_cart_store: %s still no space after eviction (%u/%u)", name, (unsigned)g_ram_cart_next_free, (unsigned)g_ram_cart_total_size);
            return false;
        }
    }

    ram_cart_entry_t *existing = ram_cart_find_entry(name);
    if (existing != JO_NULL)
    {
        if (existing->size >= size)
        {
            existing->size = (uint32_t)size;
            existing->width = 0;
            existing->height = 0;
            return ram_cart_copy_to_cart(existing->offset, data, size);
        }

        existing->used = false;
    }

    ram_cart_entry_t *entry = ram_cart_alloc_entry(name);
    if (entry == JO_NULL)
    {
        cart_ram_log("ram_cart_store: no free directory entry for %s", name);
        return false;
    }

    entry->offset = g_ram_cart_next_free;
    entry->size = (uint32_t)size;

    if (!ram_cart_copy_to_cart(entry->offset, data, size))
    {
        entry->used = false;
        cart_ram_log("ram_cart_store: %s at %u failed copy", name, (unsigned)entry->offset);
        return false;
    }

    g_ram_cart_next_free += (uint32_t)size;
    cart_ram_log("ram_cart_store: %s at %u size %u", name, (unsigned)entry->offset, (unsigned)size);
    runtime_log("CART store: %s offset=%u sz=%u (used %u/%u)", name, (unsigned)entry->offset, (unsigned)size, (unsigned)ram_cart_get_used_size(), (unsigned)ram_cart_get_total_size());
    return true;
}

bool ram_cart_store_tga(const char *name, const char *filename, const char *dir)
{
    if (!ram_cart_begin_store_tga(name, filename, dir))
        return false;

    while (ram_cart_step_store_tga(32) == RamCartTgaDebugInProgress)
    {
    }

    return g_ram_cart_tga_debug.status == RamCartTgaDebugDone;
}

bool ram_cart_begin_store_tga(const char *name, const char *filename, const char *dir)
{
    if (name == JO_NULL || filename == JO_NULL || dir == JO_NULL)
    {
        ram_cart_set_last_error("cart:tga args");
        return false;
    }

    ram_cart_cancel_store_tga();
    ram_cart_tga_debug_reset_state();
    strncpy(g_ram_cart_tga_debug.name, name, sizeof(g_ram_cart_tga_debug.name) - 1);
    g_ram_cart_tga_debug.name[sizeof(g_ram_cart_tga_debug.name) - 1] = '\0';
    strncpy(g_ram_cart_tga_debug.filename, filename, sizeof(g_ram_cart_tga_debug.filename) - 1);
    g_ram_cart_tga_debug.filename[sizeof(g_ram_cart_tga_debug.filename) - 1] = '\0';
    strncpy(g_ram_cart_tga_debug.dir, dir, sizeof(g_ram_cart_tga_debug.dir) - 1);
    g_ram_cart_tga_debug.dir[sizeof(g_ram_cart_tga_debug.dir) - 1] = '\0';

    g_ram_cart_tga_debug.status = RamCartTgaDebugInProgress;
    g_ram_cart_tga_debug.phase = 0;
    ram_cart_tga_debug_set_stage("queued");
    ram_cart_set_last_error("cart:tga queued");
    return true;
}

ram_cart_tga_debug_status_t ram_cart_step_store_tga(int max_rows)
{
    unsigned char header[18];
    int image_type;

    if (g_ram_cart_tga_debug.status != RamCartTgaDebugInProgress)
        return g_ram_cart_tga_debug.status;

    if (max_rows == 0)
        return g_ram_cart_tga_debug.status;

    if (max_rows <= 0)
        max_rows = 1;

    if (g_ram_cart_tga_debug.phase == 0)
    {
        ram_cart_tga_debug_set_stage("open");
        ram_cart_set_last_error("cart:tga open");
        jo_fs_cd(g_ram_cart_tga_debug.dir);
        if (!jo_fs_open(&g_ram_cart_tga_debug.file, g_ram_cart_tga_debug.filename))
        {
            jo_fs_cd(JO_PARENT_DIR);
            ram_cart_set_last_error("cart:tga open fail");
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_tga_debug_set_stage("failed open");
            return g_ram_cart_tga_debug.status;
        }
        jo_fs_cd(JO_PARENT_DIR);
        g_ram_cart_tga_debug.file_open = true;
        g_ram_cart_tga_debug.phase = 1;
        return g_ram_cart_tga_debug.status;
    }

    if (g_ram_cart_tga_debug.phase == 1)
    {
        ram_cart_tga_debug_set_stage("header");
        ram_cart_set_last_error("cart:tga header");
        if (jo_fs_read_next_bytes(&g_ram_cart_tga_debug.file, (char *)header, sizeof(header)) != (int)sizeof(header))
        {
            ram_cart_set_last_error("cart:tga header");
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_cancel_store_tga();
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_tga_debug_set_stage("failed header");
            return g_ram_cart_tga_debug.status;
        }

        g_ram_cart_tga_debug.id_length = header[0];
        image_type = header[2];
        g_ram_cart_tga_debug.width = header[12] | (header[13] << 8);
        g_ram_cart_tga_debug.height = header[14] | (header[15] << 8);
        g_ram_cart_tga_debug.pixel_depth = header[16];
        g_ram_cart_tga_debug.bytes_per_pixel = g_ram_cart_tga_debug.pixel_depth / 8;
        g_ram_cart_tga_debug.top_left = (header[17] & 0x20) != 0;

        if (image_type != 2 || g_ram_cart_tga_debug.width <= 0 || g_ram_cart_tga_debug.height <= 0 ||
            (g_ram_cart_tga_debug.pixel_depth != 24 && g_ram_cart_tga_debug.pixel_depth != 32))
        {
            ram_cart_set_last_error("cart:tga format");
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_cancel_store_tga();
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_tga_debug_set_stage("failed format");
            return g_ram_cart_tga_debug.status;
        }

        g_ram_cart_tga_debug.phase = (g_ram_cart_tga_debug.id_length > 0) ? 2 : 3;
        return g_ram_cart_tga_debug.status;
    }

    if (g_ram_cart_tga_debug.phase == 2)
    {
        ram_cart_tga_debug_set_stage("seek");
        ram_cart_set_last_error("cart:tga seek");
        if (!jo_fs_seek_forward(&g_ram_cart_tga_debug.file, (unsigned int)g_ram_cart_tga_debug.id_length))
        {
            ram_cart_set_last_error("cart:tga seek");
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_cancel_store_tga();
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_tga_debug_set_stage("failed seek");
            return g_ram_cart_tga_debug.status;
        }
        g_ram_cart_tga_debug.phase = 3;
        return g_ram_cart_tga_debug.status;
    }

    if (g_ram_cart_tga_debug.phase == 3)
    {
        ram_cart_tga_debug_set_stage("reserve");
        ram_cart_set_last_error("cart:tga reserve");
        if (!ram_cart_reserve_entry(g_ram_cart_tga_debug.name,
                (uint32_t)((size_t)g_ram_cart_tga_debug.width * (size_t)g_ram_cart_tga_debug.height * sizeof(unsigned short)),
                &g_ram_cart_tga_debug.entry))
        {
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_tga_debug_set_stage("failed reserve");
            return g_ram_cart_tga_debug.status;
        }
        g_ram_cart_tga_debug.phase = 4;
        return g_ram_cart_tga_debug.status;
    }

    if (g_ram_cart_tga_debug.phase == 4)
    {
        ram_cart_tga_debug_set_stage("malloc");
        ram_cart_set_last_error("cart:tga malloc");
        g_ram_cart_tga_debug.src_row = (unsigned char *)jo_malloc((size_t)g_ram_cart_tga_debug.width * (size_t)g_ram_cart_tga_debug.bytes_per_pixel);
        g_ram_cart_tga_debug.decoded_row = (unsigned short *)jo_malloc((size_t)g_ram_cart_tga_debug.width * sizeof(unsigned short));
        if (g_ram_cart_tga_debug.src_row == JO_NULL || g_ram_cart_tga_debug.decoded_row == JO_NULL)
        {
            ram_cart_set_last_error("cart:tga malloc");
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_cancel_store_tga();
            g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
            ram_cart_tga_debug_set_stage("failed malloc");
            return g_ram_cart_tga_debug.status;
        }
        g_ram_cart_tga_debug.current_row = 0;
        g_ram_cart_tga_debug.current_pixel = 0;
        g_ram_cart_tga_debug.phase = 5;
        ram_cart_tga_debug_set_stage("row read");
        ram_cart_set_last_error("cart:row read 0");
        return g_ram_cart_tga_debug.status;
    }

    for (int rows_done = 0; rows_done < max_rows && g_ram_cart_tga_debug.current_row < g_ram_cart_tga_debug.height; ++rows_done)
    {
        int y = g_ram_cart_tga_debug.current_row;
        char progress[32];

        if (g_ram_cart_tga_debug.phase == 5)
        {
            snprintf(progress, sizeof(progress), "cart:row read %d", y);
            ram_cart_set_last_error(progress);
            ram_cart_tga_debug_set_stage("row read");
            if (jo_fs_read_next_bytes(&g_ram_cart_tga_debug.file, (char *)g_ram_cart_tga_debug.src_row,
                    (unsigned int)((size_t)g_ram_cart_tga_debug.width * (size_t)g_ram_cart_tga_debug.bytes_per_pixel)) !=
                (int)((size_t)g_ram_cart_tga_debug.width * (size_t)g_ram_cart_tga_debug.bytes_per_pixel))
            {
                ram_cart_set_last_error("cart:tga row read");
                g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
                ram_cart_tga_debug_set_stage("failed read");
                ram_cart_cancel_store_tga();
                g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
                ram_cart_tga_debug_set_stage("failed read");
                return g_ram_cart_tga_debug.status;
            }

            g_ram_cart_tga_debug.current_pixel = 0;
            g_ram_cart_tga_debug.phase = 6;
            if (g_ram_cart_verbose_pixel_logging)
                return g_ram_cart_tga_debug.status;
        }

        if (g_ram_cart_tga_debug.phase == 6)
        {
            ram_cart_tga_debug_set_stage("row decode");
            int pixels_per_step = g_ram_cart_verbose_pixel_logging ? 8 : g_ram_cart_tga_debug.width;

            for (int i = 0; i < pixels_per_step && g_ram_cart_tga_debug.current_pixel < g_ram_cart_tga_debug.width; ++i, ++g_ram_cart_tga_debug.current_pixel)
            {
                int x = g_ram_cart_tga_debug.current_pixel;
                const uint8_t *src = g_ram_cart_tga_debug.src_row + (size_t)x * (size_t)g_ram_cart_tga_debug.bytes_per_pixel;
                uint8_t b = src[0];
                uint8_t g = src[1];
                uint8_t r = src[2];
                uint8_t a = (g_ram_cart_tga_debug.pixel_depth == 32) ? src[3] : 0xFF;

                if (a == 0)
                    g_ram_cart_tga_debug.decoded_row[x] = JO_COLOR_Transparent;
                else
                    g_ram_cart_tga_debug.decoded_row[x] = JO_COLOR_RGB(r, g, b);
            }

            snprintf(progress, sizeof(progress), "cart:row decode %d:%d", y, g_ram_cart_tga_debug.current_pixel);
            ram_cart_set_last_error(progress);

            if (g_ram_cart_tga_debug.current_pixel < g_ram_cart_tga_debug.width)
                return g_ram_cart_tga_debug.status;

            g_ram_cart_tga_debug.phase = 7;
            if (g_ram_cart_verbose_pixel_logging)
                return g_ram_cart_tga_debug.status;
        }

        if (g_ram_cart_tga_debug.phase == 7)
        {
            int dst_y = g_ram_cart_tga_debug.top_left ? y : (g_ram_cart_tga_debug.height - 1 - y);
            uint32_t dst_offset = g_ram_cart_tga_debug.entry->offset + ((uint32_t)dst_y * (uint32_t)g_ram_cart_tga_debug.width * sizeof(unsigned short));

            snprintf(progress, sizeof(progress), "cart:row write %d", y);
            ram_cart_set_last_error(progress);
            ram_cart_tga_debug_set_stage("row write");
            if (!ram_cart_copy_to_cart(dst_offset, g_ram_cart_tga_debug.decoded_row, (size_t)g_ram_cart_tga_debug.width * sizeof(unsigned short)))
            {
                g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
                ram_cart_tga_debug_set_stage("failed write");
                ram_cart_cancel_store_tga();
                g_ram_cart_tga_debug.status = RamCartTgaDebugFailed;
                ram_cart_tga_debug_set_stage("failed write");
                return g_ram_cart_tga_debug.status;
            }

            ++g_ram_cart_tga_debug.current_row;
            g_ram_cart_tga_debug.current_pixel = 0;
            g_ram_cart_tga_debug.phase = 5;

            if (g_ram_cart_verbose_pixel_logging)
                return g_ram_cart_tga_debug.status;
        }
    }

    if (g_ram_cart_tga_debug.current_row >= g_ram_cart_tga_debug.height)
    {
        g_ram_cart_tga_debug.entry->width = (uint32_t)g_ram_cart_tga_debug.width;
        g_ram_cart_tga_debug.entry->height = (uint32_t)g_ram_cart_tga_debug.height;
        ram_cart_set_last_error("cart:tga ok");
        g_ram_cart_tga_debug.status = RamCartTgaDebugDone;
        ram_cart_tga_debug_set_stage("done");
        if (g_ram_cart_tga_debug.decoded_row != JO_NULL)
            jo_free(g_ram_cart_tga_debug.decoded_row);
        if (g_ram_cart_tga_debug.src_row != JO_NULL)
            jo_free(g_ram_cart_tga_debug.src_row);
        g_ram_cart_tga_debug.decoded_row = JO_NULL;
        g_ram_cart_tga_debug.src_row = JO_NULL;
        if (g_ram_cart_tga_debug.file_open)
        {
            jo_fs_close(&g_ram_cart_tga_debug.file);
            g_ram_cart_tga_debug.file_open = false;
        }
    }

    return g_ram_cart_tga_debug.status;
}

bool ram_cart_store_sprite_img(const char *name, const jo_img *img)
{
    if (name == JO_NULL || img == JO_NULL || img->data == JO_NULL || img->width <= 0 || img->height <= 0)
        return false;

    size_t image_size = (size_t)img->width * (size_t)img->height * sizeof(unsigned short);
    if (!ram_cart_store_sprite(name, img->data, image_size))
        return false;

    ram_cart_set_sprite_dimensions(name, img->width, img->height);
    return true;
}

bool ram_cart_load_sprite(const char *name, void *out_buffer, size_t size)
{
    if (name == JO_NULL || out_buffer == JO_NULL || size == 0)
        return false;

    if (ram_cart_get_status() == RamCartStatusNotDetected)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
    {
        cart_ram_log("ram_cart_load: %s missing", name);
        return false;
    }

    if (size < entry->size)
    {
        cart_ram_log("ram_cart_load: %s buffer too small (%u<%u)", name, (unsigned)size, (unsigned)entry->size);
        return false;
    }

    cart_ram_log("ram_cart_load: %s at %u size %u", name, (unsigned)entry->offset, (unsigned)entry->size);
    return ram_cart_copy_from_cart(entry->offset, out_buffer, entry->size);
}

bool ram_cart_load_region(const char *name, int sheet_width, int sheet_height,
    int start_x, int start_y, int width, int height, void *out_buffer, size_t out_size)
{
    ram_cart_entry_t *entry;
    unsigned short *dst;

    if (name == JO_NULL || out_buffer == JO_NULL)
    {
        ram_cart_set_last_error("cart:region args");
        return false;
    }
    if (sheet_width <= 0 || sheet_height <= 0 || width <= 0 || height <= 0)
    {
        ram_cart_set_last_error("cart:region dim");
        return false;
    }
    if (start_x < 0 || start_y < 0 || start_x + width > sheet_width || start_y + height > sheet_height)
    {
        ram_cart_set_last_error("cart:region bounds");
        return false;
    }
    if (out_size < (size_t)width * (size_t)height * sizeof(unsigned short))
    {
        ram_cart_set_last_error("cart:region outsize");
        return false;
    }
    if (ram_cart_get_status() == RamCartStatusNotDetected)
    {
        ram_cart_set_last_error("cart:not detected");
        return false;
    }

    entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
    {
        ram_cart_set_last_error("cart:region missing");
        return false;
    }

    dst = (unsigned short *)out_buffer;
    for (int y = 0; y < height; ++y)
    {
        uint32_t src_offset = entry->offset + ((uint32_t)(start_y + y) * (uint32_t)sheet_width + (uint32_t)start_x) * sizeof(unsigned short);
        unsigned short *dst_row = dst + (size_t)y * (size_t)width;

        if (!ram_cart_copy_from_cart(src_offset, dst_row, (size_t)width * sizeof(unsigned short)))
            return false;
    }

    ram_cart_set_last_error("cart:region ok");
    return true;
}

#define STREAM_BUFFER_WIDTH 96
#define STREAM_BUFFER_HEIGHT 64
#define STREAM_BUFFER_PIXELS (STREAM_BUFFER_WIDTH * STREAM_BUFFER_HEIGHT)
static unsigned short stream_buffer[STREAM_BUFFER_PIXELS];

bool ram_cart_stream_frame(const char *name, int sheet_width, int sheet_height, int frame_x, int frame_y, int frame_width, int frame_height, int target_width, int sprite_id)
{
    if (name == JO_NULL || sheet_width <= 0 || sheet_height <= 0 || frame_width <= 0 || frame_height <= 0 || target_width <= 0 || sprite_id < 0)
        return false;

    if (frame_x < 0 || frame_y < 0 || frame_x + frame_width > sheet_width || frame_y + frame_height > sheet_height)
        return false;

    int sprite_width = ram_cart_align_sprite_dimension(target_width);
    int sprite_height = ram_cart_align_sprite_dimension(frame_height);
    int copy_width = frame_width;
    if (copy_width > target_width)
        copy_width = target_width;

    if ((size_t)sprite_width * (size_t)sprite_height > STREAM_BUFFER_PIXELS)
        return false;

    if (ram_cart_get_status() == RamCartStatusNotDetected)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
        return false;

    for (size_t i = 0; i < (size_t)sprite_width * (size_t)sprite_height; ++i)
        stream_buffer[i] = JO_COLOR_Transparent;

    for (int y = 0; y < frame_height; ++y)
    {
        uint32_t src_offset = entry->offset + ((uint32_t)(frame_y + y) * (uint32_t)sheet_width + (uint32_t)frame_x) * sizeof(unsigned short);
        unsigned short *dst_row = stream_buffer + (size_t)y * (size_t)sprite_width;

        if (!ram_cart_copy_from_cart(src_offset, dst_row, (size_t)copy_width * sizeof(unsigned short)))
            return false;
    }

    jo_img img = {0};
    img.width = sprite_width;
    img.height = sprite_height;
    img.data = stream_buffer;

    jo_sprite_replace(&img, sprite_id);
    return true;
}

bool ram_cart_draw_frame(const char *name, int sheet_width, int sheet_height, int frame_x, int frame_y, int frame_width, int frame_height, int target_width, int sprite_id)
{
    if (name == JO_NULL || sheet_width <= 0 || sheet_height <= 0 || frame_width <= 0 || frame_height <= 0 || target_width <= 0 || sprite_id < 0)
        return false;

    if (ram_cart_get_status() == RamCartStatusNotDetected)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
        return false;

    size_t expected_size = (size_t)sheet_width * (size_t)sheet_height * sizeof(unsigned short);
    if (entry->size != expected_size)
        cart_ram_log("ram_cart_draw_frame: size mismatch %s (%u != %u)", name, (unsigned)entry->size, (unsigned)expected_size);

    if (frame_x < 0 || frame_y < 0 || frame_x + frame_width > sheet_width || frame_y + frame_height > sheet_height)
        return false;

    if ((size_t)frame_width * (size_t)frame_height <= STREAM_BUFFER_PIXELS)
        return ram_cart_stream_frame(name, sheet_width, sheet_height, frame_x, frame_y, frame_width, frame_height, target_width, sprite_id);

    int sprite_width = ram_cart_align_sprite_dimension(target_width);
    int sprite_height = ram_cart_align_sprite_dimension(frame_height);
    size_t frame_size = (size_t)sprite_width * (size_t)sprite_height * sizeof(unsigned short);

    unsigned short *dst;
    bool used_static = false;
    static unsigned short cart_tmp[128 * 128];

    if (frame_size <= sizeof(cart_tmp))
    {
        dst = cart_tmp;
        used_static = true;
    }
    else
    {
        dst = (unsigned short *)jo_malloc(frame_size);
        if (dst == JO_NULL)
            return false;
    }

    for (size_t i = 0; i < (size_t)sprite_width * (size_t)sprite_height; ++i)
        dst[i] = JO_COLOR_Transparent;

    int copy_width = frame_width;
    if (copy_width > target_width)
        copy_width = target_width;

    for (int y = 0; y < frame_height; ++y)
    {
        uint32_t src_offset = entry->offset + ((uint32_t)(frame_y + y) * (uint32_t)sheet_width + (uint32_t)frame_x) * sizeof(unsigned short);
        unsigned short *dst_row = dst + (size_t)y * (size_t)sprite_width;

        if (!ram_cart_copy_from_cart(src_offset, dst_row, (size_t)copy_width * sizeof(unsigned short)))
        {
            if (!used_static)
                jo_free(dst);
            return false;
        }
    }

    jo_img tmp = {0};
    tmp.width = sprite_width;
    tmp.height = sprite_height;
    tmp.data = dst;
    jo_sprite_replace(&tmp, sprite_id);

    if (!used_static)
        jo_free(dst);

    return true;
}

static void ram_cart_recompute_next_free(void)
{
    ram_cart_entry_t *table = ram_cart_entry_table();
    uint32_t max_end = CART_RAM_DATA_OFFSET;

    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
    {
        if (!table[i].used)
            continue;

        uint32_t end = table[i].offset + table[i].size;
        if (end > max_end)
            max_end = end;
    }

    g_ram_cart_next_free = max_end;
}

bool ram_cart_delete_sprite(const char *name)
{
    if (name == JO_NULL)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
        return false;

    entry->used = false;
    entry->name[0] = '\0';
    entry->offset = 0;
    entry->size = 0;
    entry->width = 0;
    entry->height = 0;
    ram_cart_recompute_next_free();
    return true;
}

void ram_cart_clear(void)
{
    ram_cart_entry_t *table = ram_cart_entry_table();
    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
        table[i].used = false;
    g_ram_cart_next_free = CART_RAM_DATA_OFFSET;
}

/* Legacy compatibility wrappers */

bool check_cartridge_ram(void)
{
    ram_cart_status_t status = ram_cart_get_status();
    return status != RamCartStatusNotDetected;
}

bool ram_cart_is_ok(void)
{
    return ram_cart_get_status() != RamCartStatusNotDetected;
}

unsigned int ram_cart_size_bytes(void)
{
    return (unsigned int)ram_cart_get_total_size();
}

unsigned int ram_cart_used_bytes(void)
{
    return (unsigned int)ram_cart_get_used_size();
}

void ram_cart_add_used_bytes(unsigned int bytes)
{
    (void)bytes;
}

void ram_cart_subtract_used_bytes(unsigned int bytes)
{
    (void)bytes;
}

void ram_cart_reset_used_bytes(void)
{
    /* no-op, cart backend tracks used memory */
}

void ram_cart_draw_boot_screen(void)
{
    draw_text(7, 10, "--- Sonic Starter ---");

    if (ram_cart_is_ok())
    {
        unsigned int total_mb = ram_cart_size_bytes() / 1024 / 1024;
        draw_text(7, 12, "Cartridge RAM: OK (%u MB)", total_mb);
        draw_text(7, 13, "Cart RAM used: %u KB / %u KB", ram_cart_used_bytes() / 1024, total_mb * 1024);
        draw_text(7, 15, "Press START to continue");
    }
    else
    {
        draw_text(7, 12, "Cartridge RAM: NOT AVAILABLE");
        draw_text(7, 13, "Requires 4MB Cart RAM");
        draw_text(7, 15, "Insert a compatible cart and reset");
    }
}

void ram_cart_defocus_boot_text(void)
{
    int lines[] = {10, 12, 13, 14, 15};
    clear_text_lines(lines, 5);
}

