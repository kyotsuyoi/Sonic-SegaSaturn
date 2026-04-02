#ifndef __MENU_TEXT_H__
# define __MENU_TEXT_H__

void    clear_text_line(int line);
void    clear_text_lines(int *lines, int count);
void    draw_text(int col, int line, const char * fmt, ...);

#endif /* !__MENU_TEXT_H__ */
