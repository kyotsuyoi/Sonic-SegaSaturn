#include <jo/jo.h>
#include <stdarg.h>
#include "ram_cart.h"
#include "menu_text.h"

void    clear_text_line(int line)
{
    jo_printf(0, line, "                                                            ");
}

void    clear_text_lines(int *lines, int count)
{
    for (int i = 0; i < count; ++i)
        clear_text_line(lines[i]);
}

void    draw_text(int col, int line, const char * fmt, ...)
{
    clear_text_line(line);
    char buffer[128];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    jo_printf(col, line, "%s", buffer);
}
