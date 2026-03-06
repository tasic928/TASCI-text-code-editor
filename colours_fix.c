#include <ncurses.h>

#include "colours_fix.h"


void colours_fix_init(int *menu_bg, int *editor_bg, int *cursor_fg) {
    
    int mbg = -1;   /* Menu: terminal default */
    int ebg = -1;   /* Editor: terminal default */
    int cfg = -1;   /* Cursor: terminal default */

   //A basic colur fix so that the syntax highlightning works!

    if (COLORS >= 8) {
       
        
        
        if (COLORS >= 256) {
         
            mbg = 12;  /* Bright blue in 256-color palette */
            ebg = 234; /* Dark gray (Solarized/Dark style) in 256-color palette */
        } else {
            
            mbg = -1;  /* Use terminal default */
            ebg = -1;  /* Use terminal default */
        }
    }

    if (menu_bg) *menu_bg = mbg;
    if (editor_bg) *editor_bg = ebg;
    if (cursor_fg) *cursor_fg = cfg;
}
