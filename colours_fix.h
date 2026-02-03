#ifndef COLOURS_FIX_H
#define COLOURS_FIX_H

/* Initialize ncurses colors with safe fallbacks across terminals. */
void colours_fix_init(int *menu_bg, int *editor_bg, int *cursor_fg);

#endif
