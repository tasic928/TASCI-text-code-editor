#include <ncurses.h>

#include "colours_fix.h"

/* Adaptive color initialization that respects Linux terminal theme.
   This makes TASCI use colors defined by the terminal's color scheme,
   ensuring blue appears as blue (not pink) based on system settings. */
void colours_fix_init(int *menu_bg, int *editor_bg, int *cursor_fg) {
    /* Use terminal's default colors (-1) which adapt to the theme.
       This ensures TASCI respects the Linux distro's color settings. */
    int mbg = -1;   /* Menu: terminal default */
    int ebg = -1;   /* Editor: terminal default */
    int cfg = -1;   /* Cursor: terminal default */

    /* For ncurses, -1 means "use terminal's default color".
       This delegates color decisions to the terminal profile/theme,
       which is what the user has configured in their Linux distro. */

    if (COLORS >= 8) {
        /* Terminal supports at least 8 colors, we can use theme colors.
           Try to detect and use a proper blue from the terminal palette. */
        
        /* Check if we have 256-color support */
        if (COLORS >= 256) {
            /* In 256-color mode, we can use colors that are more reliably blue.
               Color 12 is typically a nice bright blue in most terminal palettes. */
            mbg = 12;  /* Bright blue in 256-color palette */
            ebg = 234; /* Dark gray (Solarized/Dark style) in 256-color palette */
        } else {
            /* Standard 8-color mode - use whatever the theme defines.
               This adapts to whatever the user has configured. */
            mbg = -1;  /* Use terminal default */
            ebg = -1;  /* Use terminal default */
        }
    }

    if (menu_bg) *menu_bg = mbg;
    if (editor_bg) *editor_bg = ebg;
    if (cursor_fg) *cursor_fg = cfg;
}
