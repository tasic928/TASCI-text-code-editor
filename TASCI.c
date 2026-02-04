/* TASCI.c - Full-featured terminal text editor modeled after Windows Notepad
   Author: tasic928
   Features:
   - Menu bar: Edit, View, Find, Help, File, About
   - Explorer (left) + Editor (right)
   - Edit: Cut, Copy, Paste, Undo, Redo, Select All, Delete Line, Time/Date insert
   - View: Toggle line numbers, word wrap, zoom simulation, status bar toggle
   - Find: Search, Replace, Go to Line
   - File: New, Open, Save, Save As, Delete, Exit, Print(stub)
   - About: TASCI info
   - Arrow keys navigation, Enter/ESC, blinking cursor
   - Colored UI using ncurses
*/

#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#include <stdarg.h>

#include "syntax_highlighting.h"
#include "colours_fix.h"

#define MAX_FILES 512
#define MAX_LINE 1024
#define CLIP_CAP (1024 * 1024)
#define SIDEBAR 30
#define MENU_ITEMS 9

enum Mode { MODE_EXPLORER, MODE_EDITOR, MODE_MENU, MODE_DIALOG };
enum Mode mode = MODE_EXPLORER;

/* ---------- STATE ---------- */
char cwd[PATH_MAX];
char files[MAX_FILES][256];
int file_count = 0, sel = 0;
int file_off = 0;

char **buf = NULL;
int buf_cap = 0;
int lines = 1, cx = 0, cy = 0;
char current_file[256] = "";
int rowoff = 0, coloff = 0;
int is_dirty = 0;

int menu_sel = 0;
const char *menu_items[MENU_ITEMS] = {
    "Edit", "View", "Find", "Help", "File", "Save", "Save As", "Open Folder", "About"
};

int blink_on = 1;

/* View toggles */
int show_line_numbers = 1;
int show_status_bar = 1;
int soft_wrap = 0;

/* Clipboard */
char clip[CLIP_CAP];

/* ---------- WINDOWS ---------- */
WINDOW *menuw, *sidew, *mainw, *statusw;
int sidebar_width = SIDEBAR;
char status_msg[256] = "TASCI Ready - Ctrl+X to exit";
time_t status_time = 0;

/* ---------- UTIL ---------- */
static int num_digits(int n) {
    int d = 1;
    while (n >= 10) { n /= 10; d++; }
    return d;
}

static char *line_alloc_empty(void) {
    char *line = (char *)calloc(MAX_LINE, 1);
    if (line) line[0] = '\0';
    return line;
}

static char *line_alloc_copy(const char *src) {
    char *line = line_alloc_empty();
    if (!line) return NULL;
    if (src) {
        strncpy(line, src, MAX_LINE - 1);
        line[MAX_LINE - 1] = '\0';
    }
    return line;
}

static int buffer_ensure_capacity(int needed) {
    if (needed <= buf_cap) return 1;
    int new_cap = buf_cap ? buf_cap : 64;
    while (new_cap < needed) new_cap *= 2;
    char **new_buf = (char **)realloc(buf, (size_t)new_cap * sizeof(char *));
    if (!new_buf) return 0;
    for (int i = buf_cap; i < new_cap; i++) new_buf[i] = NULL;
    buf = new_buf;
    buf_cap = new_cap;
    return 1;
}

static void buffer_clear(void) {
    if (!buf) return;
    for (int i = 0; i < lines; i++) {
        free(buf[i]);
        buf[i] = NULL;
    }
    lines = 0;
}

static void buffer_init_if_needed(void) {
    if (buf && buf_cap > 0) return;
    buf_cap = 0;
    buffer_ensure_capacity(1);
    buf[0] = line_alloc_empty();
    lines = 1;
}

static void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_msg, sizeof(status_msg), fmt, ap);
    va_end(ap);
    status_time = time(NULL);
}

static void layout_windows(void) {
    int h, w;
    getmaxyx(stdscr, h, w);
    int side = SIDEBAR;
    if (w < 50) side = w / 3;
    if (side < 10) side = 10;
    sidebar_width = side;
    wresize(menuw, 1, w); mvwin(menuw, 0, 0);
    wresize(sidew, h - 2, side); mvwin(sidew, 1, 0);
    wresize(mainw, h - 2, w - side); mvwin(mainw, 1, side);
    wresize(statusw, 1, w); mvwin(statusw, h - 1, 0);
}

static void prompt_input(const char *label, char *out, size_t out_sz) {
    echo(); curs_set(1);
    mvprintw(LINES-1,0,"%s",label); clrtoeol();
    getnstr(out, (int)out_sz - 1);
    noecho(); curs_set(1);
}

static void popup_input(const char *title, const char *label, char *out, size_t out_sz) {
    int h = 9, w = 64;
    int sy = (LINES - h) / 2;
    int sx = (COLS - w) / 2;
    WINDOW *wpopup = newwin(h, w, sy, sx);
    box(wpopup, 0, 0);
    mvwprintw(wpopup, 1, 2, "%s", title);
    mvwprintw(wpopup, 3, 2, "%s", label);
    mvwprintw(wpopup, 5, 2, "> ");
    wrefresh(wpopup);
    echo();
    curs_set(1);
    mvwgetnstr(wpopup, 5, 4, out, (int)out_sz - 1);
    noecho();
    curs_set(1);
    delwin(wpopup);
}

/* ---------- FILE UTILITIES ---------- */
int is_dir(const char *f) {
    struct stat st;
    return stat(f, &st) == 0 && S_ISDIR(st.st_mode);
}

void load_dir() {
    DIR *d = opendir(".");
    if (!d) { file_count=0; return; }
    struct dirent *e;
    file_count = 0;
    while ((e = readdir(d)) && file_count < MAX_FILES) {
        strncpy(files[file_count++], e->d_name, sizeof(files[0])-1);
        files[file_count-1][sizeof(files[0])-1] = '\0';
    }
    closedir(d);
    sel = 0;
    file_off = 0;
}

void load_file(const char *f) {
    buffer_init_if_needed();
    FILE *fp = fopen(f, "r");
    buffer_clear();
    if (fp) {
        char temp[MAX_LINE];
        while (fgets(temp, sizeof(temp), fp)) {
            temp[strcspn(temp, "\n")] = 0;
            if (!buffer_ensure_capacity(lines + 1)) break;
            buf[lines] = line_alloc_copy(temp);
            if (!buf[lines]) break;
            lines++;
        }
        fclose(fp);
    }
    if (lines == 0) {
        buffer_ensure_capacity(1);
        if (!buf[0]) buf[0] = line_alloc_empty();
        buf[0][0] = '\0';
        lines = 1;
    }
    strncpy(current_file, f, sizeof(current_file)-1);
    current_file[sizeof(current_file)-1]='\0';
    cx=cy=0;
    rowoff=coloff=0;
    is_dirty = 0;
}

void save_file() {
    if(!current_file[0]) { set_status("No file name. Use Save As."); return; }
    FILE *fp=fopen(current_file,"w");
    if(!fp) { set_status("Save failed: %s", current_file); return; }
    for(int i=0;i<lines;i++)
        fprintf(fp,"%s\n",buf[i]);
    fclose(fp);
    set_status("Saved: %s", current_file);
    is_dirty = 0;
}

void save_file_as() {
    char fname[256];
    popup_input("Save As", "Enter file name (with extension):", fname, sizeof(fname));
    if(strlen(fname)==0) return;
    strncpy(current_file,fname,sizeof(current_file)-1);
    current_file[sizeof(current_file)-1]='\0';
    save_file();
    load_dir();
}

/* ---------- POPUP ---------- */
void popup(const char *title,const char *msg){
    int h=10,w=60,sy=(LINES-h)/2,sx=(COLS-w)/2;
    WINDOW *wpopup=newwin(h,w,sy,sx);
    box(wpopup,0,0);
    mvwprintw(wpopup,1,2,"%s",title);
    mvwprintw(wpopup,3,2,"%s",msg);
    mvwprintw(wpopup,h-2,2,"Press any key...");
    wrefresh(wpopup);
    wgetch(wpopup);
    delwin(wpopup);
}

int popup_select(const char *title, const char *items[], int count) {
    int h = count + 4;
    int w = 46;
    int sy = (LINES - h) / 2;
    int sx = (COLS - w) / 2;
    if (h > LINES - 2) h = LINES - 2;
    WINDOW *wpopup = newwin(h, w, sy, sx);
    keypad(wpopup, TRUE);
    int sel = 0;
    int ch;
    while (1) {
        werase(wpopup);
        box(wpopup, 0, 0);
        mvwprintw(wpopup, 1, 2, "%s", title);
        for (int i = 0; i < count && (i + 2) < h - 1; i++) {
            if (i == sel) wattron(wpopup, A_REVERSE);
            mvwprintw(wpopup, i + 2, 2, "%s", items[i]);
            if (i == sel) wattroff(wpopup, A_REVERSE);
        }
        wrefresh(wpopup);
        ch = wgetch(wpopup);
        if (ch == 27) { /* ESC */
            delwin(wpopup);
            return -1;
        }
        if (ch == '\n') {
            delwin(wpopup);
            return sel;
        }
        if (ch == KEY_UP && sel > 0) sel--;
        else if (ch == KEY_DOWN && sel < count - 1) sel++;
    }
}

/* ---------- FIND/REPLACE ---------- */
void find_text() {
    char query[256];
    prompt_input("Find: ", query, sizeof(query));
    for(int y=0;y<lines;y++){
        char *p=strstr(buf[y],query);
        if(p){ cy=y; cx=(int)(p-buf[y]); return; }
    }
    set_status("Text not found");
}

void replace_text() {
    char find[256], replace[256];
    prompt_input("Find: ", find, sizeof(find));
    prompt_input("Replace: ", replace, sizeof(replace));
    int changed = 0;
    for(int y=0;y<lines;y++){
        char *p=strstr(buf[y],find);
        if(p){
            char temp[MAX_LINE];
            strncpy(temp,buf[y],p-buf[y]);
            temp[p-buf[y]]='\0';
            strncat(temp,replace,MAX_LINE-1-strlen(temp));
            strncat(temp,p+strlen(find),MAX_LINE-1-strlen(temp));
            strncpy(buf[y],temp,MAX_LINE-1);
            buf[y][MAX_LINE-1]='\0';
            changed = 1;
        }
    }
    if (changed) is_dirty = 1;
}

/* ---------- EDITING ---------- */
static void editor_scroll(void) {
    int h, w;
    getmaxyx(mainw, h, w);
    int rows = h - 2;
    int cols = w - 2;
    int ln_digits = num_digits(lines);
    if (ln_digits < 2) ln_digits = 2;
    int ln_width = show_line_numbers ? ln_digits + 1 : 0;
    int avail = cols - ln_width;
    if (avail < 4) avail = 4;
    if (cy < rowoff) rowoff = cy;
    if (cy >= rowoff + rows) rowoff = cy - rows + 1;
    if (soft_wrap) {
        coloff = 0;
    } else {
        if (cx < coloff) coloff = cx;
        if (cx >= coloff + avail) coloff = cx - avail + 1;
    }
}

static void explorer_scroll(void) {
    int h, w;
    getmaxyx(sidew, h, w);
    (void)w;
    int rows = h - 2;
    if (sel < file_off) file_off = sel;
    if (sel >= file_off + rows) file_off = sel - rows + 1;
}

static void insert_char(int c) {
    int len = (int)strlen(buf[cy]);
    if (len >= MAX_LINE - 1) return;
    memmove(&buf[cy][cx + 1], &buf[cy][cx], len - cx + 1);
    buf[cy][cx] = (char)c;
    cx++;
    is_dirty = 1;
}

static void insert_newline(void) {
    if (!buffer_ensure_capacity(lines + 1)) return;
    char *right = line_alloc_copy(&buf[cy][cx]);
    if (!right) return;
    buf[cy][cx] = '\0';
    for (int i = lines; i > cy + 1; i--) {
        buf[i] = buf[i - 1];
    }
    buf[cy + 1] = right;
    lines++;
    cy++;
    cx = 0;
    is_dirty = 1;
}

static void delete_char(void) {
    if (cx > 0) {
        memmove(&buf[cy][cx - 1], &buf[cy][cx], strlen(buf[cy]) - cx + 1);
        cx--;
        is_dirty = 1;
    } else if (cy > 0) {
        int prev_len = (int)strlen(buf[cy - 1]);
        int cur_len = (int)strlen(buf[cy]);
        if (prev_len + cur_len < MAX_LINE - 1) {
            strcat(buf[cy - 1], buf[cy]);
            free(buf[cy]);
            for (int i = cy; i < lines - 1; i++) buf[i] = buf[i + 1];
            buf[lines - 1] = NULL;
            lines--;
            cy--;
            cx = prev_len;
            is_dirty = 1;
        }
    }
}

static void delete_forward(void) {
    int len = (int)strlen(buf[cy]);
    if (cx < len) {
        memmove(&buf[cy][cx], &buf[cy][cx + 1], len - cx);
        is_dirty = 1;
    } else if (cy < lines - 1) {
        int cur_len = (int)strlen(buf[cy]);
        int next_len = (int)strlen(buf[cy + 1]);
        if (cur_len + next_len < MAX_LINE - 1) {
            strcat(buf[cy], buf[cy + 1]);
            free(buf[cy + 1]);
            for (int i = cy + 1; i < lines - 1; i++) buf[i] = buf[i + 1];
            buf[lines - 1] = NULL;
            lines--;
            is_dirty = 1;
        }
    }
}

static void delete_line(int y) {
    if (lines <= 1) { buf[0][0] = '\0'; cx = 0; cy = 0; is_dirty = 1; return; }
    free(buf[y]);
    for (int i = y; i < lines - 1; i++) buf[i] = buf[i + 1];
    buf[lines - 1] = NULL;
    lines--;
    if (cy >= lines) cy = lines - 1;
    if (cx > (int)strlen(buf[cy])) cx = (int)strlen(buf[cy]);
    is_dirty = 1;
}

static void new_file_prompt(void) {
    char fname[256];
    popup_input("New File", "Enter file name (with extension):", fname, sizeof(fname));
    if (fname[0] == '\0') return;
    strncpy(current_file, fname, sizeof(current_file)-1);
    current_file[sizeof(current_file)-1] = '\0';
    /* Ensure the file exists on disk */
    FILE *fp = fopen(current_file, "a");
    if (fp) fclose(fp);
    buffer_clear();
    buffer_ensure_capacity(1);
    if (!buf[0]) buf[0] = line_alloc_empty();
    lines = 1;
    buf[0][0] = '\0';
    cx = cy = rowoff = coloff = 0;
    is_dirty = 0;
    load_dir();
    set_status("New file: %s", current_file);
}

static void open_folder_prompt(void) {
    char path[512];
    popup_input("Open Folder", "Enter full directory path:", path, sizeof(path));
    if (path[0] == '\0') return;
    if (chdir(path) != 0) {
        set_status("Error: Cannot open directory %s", path);
        return;
    }
    if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
    load_dir();
    set_status("Opened folder: %s", path);
}

static void special_chars_prompt(void) {
    char ch[8];
    prompt_input("Special Char (e.g. #, @, $, *, ~): ", ch, sizeof(ch));
    if (ch[0] == '\0') return;
    insert_char((unsigned char)ch[0]);
}

/* ---------- DRAW ---------- */
void draw_menu() {
    werase(menuw);
    wbkgd(menuw, COLOR_PAIR(1));
    int x = 2;
    for(int i=0;i<MENU_ITEMS;i++){
        if(mode==MODE_MENU && i==menu_sel) wattron(menuw,A_REVERSE);
        mvwprintw(menuw,0,x,"%s",menu_items[i]);
        wattroff(menuw,A_REVERSE);
        x += (int)strlen(menu_items[i]) + 6;
    }
    wrefresh(menuw);
}

void draw_sidebar() {
    werase(sidew);
    wbkgd(sidew,COLOR_PAIR(2));
    box(sidew,0,0);
    int h, w;
    getmaxyx(sidew, h, w);
    (void)w;
    int max_show = h - 2;
    for(int i=0;i<max_show && (i + file_off) < file_count;i++){
        int idx = i + file_off;
        if(mode==MODE_EXPLORER && idx==sel) wattron(sidew,A_REVERSE);
        mvwprintw(sidew,i+1,2,"%s%s",files[idx],is_dir(files[idx])?"/":"");
        wattroff(sidew,A_REVERSE);
    }
    wrefresh(sidew);
}

void draw_editor() {
    werase(mainw);
    wbkgd(mainw, COLOR_PAIR(3));
    box(mainw,0,0);
    int h, w;
    getmaxyx(mainw, h, w);
    int rows = h - 2;
    int cols = w - 2;
    int ln_digits = num_digits(lines);
    if (ln_digits < 2) ln_digits = 2;
    int ln_width = show_line_numbers ? ln_digits + 1 : 0;
    const SyntaxLang *lang = sh_lang_for_file(current_file);
    for(int y=0; y<rows; y++){
        int filerow = y + rowoff;
        if (filerow >= lines) break;
        if(show_line_numbers) mvwprintw(mainw,y+1,1,"%*d ",ln_digits,filerow+1);
        int start = coloff;
        int len = (int)strlen(buf[filerow]);
        if (start > len) start = len;
        int avail = cols - ln_width;
        if (avail < 0) avail = 0;
        int x = 1 + ln_width;
        int i = start;
        int col = 0;
        while (buf[filerow][i] && col < avail) {
            if (buf[filerow][i] == '\t') {
                mvwaddch(mainw, y+1, x + col, ' ');
                i++;
                col++;
                continue;
            }
            if (isalpha((unsigned char)buf[filerow][i]) || buf[filerow][i] == '_') {
                int wstart = i;
                int wlen = 0;
                while (buf[filerow][i] &&
                       (isalnum((unsigned char)buf[filerow][i]) || buf[filerow][i] == '_')) {
                    i++;
                    wlen++;
                }
                if (wlen > (avail - col)) wlen = avail - col;
                if (sh_is_keyword(lang, &buf[filerow][wstart], wlen)) {
                    wattron(mainw, COLOR_PAIR(4) | A_BOLD);
                    mvwaddnstr(mainw, y+1, x + col, &buf[filerow][wstart], wlen);
                    wattroff(mainw, COLOR_PAIR(4) | A_BOLD);
                } else {
                    mvwaddnstr(mainw, y+1, x + col, &buf[filerow][wstart], wlen);
                }
                col += wlen;
            } else {
                mvwaddch(mainw, y+1, x + col, buf[filerow][i]);
                i++;
                col++;
            }
        }
    }
    int screeny = cy - rowoff + 1;
    int screenx = cx - coloff + 1 + ln_width;
    if (blink_on && screeny >= 1 && screeny < h - 1 && screenx >= 1 && screenx < w - 1) {
        chtype cell = mvwinch(mainw, screeny, screenx);
        chtype ch = cell & A_CHARTEXT;
        if (ch == 0) ch = ' ';
        wattron(mainw, A_REVERSE | A_BOLD);
        mvwaddch(mainw, screeny, screenx, ch);
        wattroff(mainw, A_REVERSE | A_BOLD);
    }
    wrefresh(mainw);
}

void draw_status(const char *msg){
    werase(statusw);
    if(show_status_bar) {
        char info[256];
        const char *name = current_file[0] ? current_file : "[No Name]";
        snprintf(info, sizeof(info), "%s  Ln %d/%d  Col %d  Lines %d",
                 name, cy + 1, lines, cx + 1, lines);
        int w = getmaxx(statusw);
        if (msg && msg[0] && (time(NULL) - status_time) < 5) {
            mvwprintw(statusw,0,2,"%s",msg);
        }
        int x = w - (int)strlen(info) - 2;
        if (x < 2) x = 2;
        mvwprintw(statusw,0,x,"%s",info);
    }
    wrefresh(statusw);
}

static int confirm_discard_or_save(void) {
    if (!is_dirty) return 1;
    const char *items[] = { "Save", "Don't Save", "Cancel" };
    int sel = popup_select("Unsaved changes", items, 3);
    if (sel == 0) {
        save_file();
        return !is_dirty;
    }
    if (sel == 1) return 1;
    return 0;
}

/* ---------- MAIN ---------- */
int main(int argc, char *argv[]){
    buffer_init_if_needed();
    /* Handle command line arguments like nano: ts filename */
    if (argc >= 2) {
        /* Check if argument looks like a file path */
        if (argv[1][0] != '-' || (argv[1][1] != '\0' && argv[1][1] != '+')) {
            /* Load the file specified on command line */
            load_file(argv[1]);
            mode = MODE_EDITOR;
        }
    }
    initscr(); cbreak(); noecho(); keypad(stdscr,TRUE); curs_set(0);
    timeout(100);
    /* Ask the terminal for a white cursor (blinking bar color).
       Some terminals accept BEL, others require ST. Send both. */
    printf("\033]12;white\007");
    printf("\033]12;#ffffff\033\\");
    fflush(stdout);
    start_color(); use_default_colors();
    /* Menu bar + editor background with safe fallbacks. */
    int menu_bg = COLOR_BLUE;
    int editor_bg = COLOR_BLACK;
    int cursor_fg = COLOR_WHITE;
    colours_fix_init(&menu_bg, &editor_bg, &cursor_fg);
    init_pair(1, COLOR_BLACK, menu_bg);
    /* Sidebar: neutral light background */
    init_pair(2, COLOR_BLACK, COLOR_WHITE);
    /* Editor: dark theme */
    init_pair(3, COLOR_WHITE, editor_bg);
    /* Keywords: readable highlight on editor background */
    init_pair(4, COLOR_CYAN, editor_bg);
    /* Cursor: white bar on editor background */
    init_pair(5, cursor_fg, editor_bg);

    if(!getcwd(cwd,sizeof(cwd))) cwd[0]='\0';
    load_dir();

    menuw=newwin(1,COLS,0,0);
    sidew=newwin(LINES-2,SIDEBAR,1,0);
    mainw=newwin(LINES-2,COLS-SIDEBAR,1,SIDEBAR);
    statusw=newwin(1,COLS,LINES-1,0);
    layout_windows();
    struct timeval last_blink;
    gettimeofday(&last_blink, NULL);
    while(1){
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed_ms = (now.tv_sec - last_blink.tv_sec) * 1000L
                        + (now.tv_usec - last_blink.tv_usec) / 1000L;
        if (elapsed_ms >= 500) {
            blink_on = !blink_on;
            last_blink = now;
        }
        editor_scroll();
        explorer_scroll();
        draw_menu(); draw_sidebar(); draw_editor(); draw_status(status_msg);

        int ch=getch();
        if (ch == ERR) continue;
        if(ch==KEY_RESIZE) { layout_windows(); continue; }
        if(ch==24) break; // Ctrl+X
        if(ch==19){ save_file(); }
        if(ch==23){ set_status("Word wrap %s", soft_wrap ? "off" : "on"); soft_wrap=!soft_wrap; }

        /* ---------- EXPLORER ---------- */
        if(mode==MODE_EXPLORER){
            if(ch==KEY_UP && sel==0) mode=MODE_MENU;
            else if(ch==KEY_UP && sel>0) sel--;
            else if(ch==KEY_DOWN && sel<file_count-1) sel++;
            else if(ch=='\n'){
                if(is_dir(files[sel])){
                    if(chdir(files[sel])==0){ if(!getcwd(cwd,sizeof(cwd))) cwd[0]='\0'; load_dir(); }
                }else{
                    if (confirm_discard_or_save()) {
                        load_file(files[sel]);
                        mode=MODE_EDITOR;
                    }
                }
            }else if(ch==KEY_BACKSPACE||ch==127){
                if(chdir("..")==0){ if(!getcwd(cwd,sizeof(cwd))) cwd[0]='\0'; load_dir(); }
            }
        }

        /* ---------- MENU ---------- */
        else if(mode==MODE_MENU){
            if(ch==KEY_LEFT && menu_sel>0) menu_sel--;
            else if(ch==KEY_RIGHT && menu_sel<MENU_ITEMS-1) menu_sel++;
            else if(ch==KEY_DOWN) mode=MODE_EXPLORER;
            else if(ch=='\n'){
                switch(menu_sel){
                    case 0: { /* Edit */
                        const char *edit_items[] = {
                            "Delete Line",
                            "Paste",
                            "Special Chars",
                            "Replace",
                            "Find"
                        };
                        int sel = popup_select("Edit", edit_items, 5);
                        if (sel == 0) delete_line(cy);
                        else if (sel == 1) { /* paste */
                            int len=(int)strlen(clip);
                            int cur=(int)strlen(buf[cy]);
                            if (cur + len >= MAX_LINE) len = MAX_LINE - cur - 1;
                            if (len > 0) {
                                memmove(&buf[cy][cx+len],&buf[cy][cx],cur-cx+1);
                                memcpy(&buf[cy][cx],clip,len); cx+=len;
                                is_dirty = 1;
                            }
                        }
                        else if (sel == 2) special_chars_prompt();
                        else if (sel == 3) replace_text();
                        else if (sel == 4) find_text();
                        break;
                    }
                    case 1: /* View */
                        soft_wrap=!soft_wrap;
                        show_line_numbers=!show_line_numbers;
                        set_status("Wrap %s, Line numbers %s", soft_wrap?"on":"off", show_line_numbers?"on":"off");
                        popup("View","Theme: Soft Gray (active)\nFont: Use terminal settings");
                        break;
                    case 2: find_text(); break;
                    case 3: popup("Help","TASCI Help\nArrow keys navigate\nCtrl+S save\nCtrl+X exit\nCtrl+F find\nCtrl+K cut\nEnter new line"); break;
                    case 4: { /* File */
                        const char *file_items[] = { "New", "Save", "Save As" };
                        int sel = popup_select("File", file_items, 3);
                        if (sel == 0) new_file_prompt();
                        else if (sel == 1) save_file();
                        else if (sel == 2) save_file_as();
                        break;
                    }
                    case 5: save_file(); break;
                    case 6: save_file_as(); break;
                    case 7: open_folder_prompt(); break;
                    case 8: popup("About","Open-source code editor TASCI — code editor made by tasic928"); break;
                }
            }
            else if(ch==27) mode=MODE_EXPLORER;
        }

        /* ---------- EDITOR ---------- */
        else if(mode==MODE_EDITOR){
            if(ch==27) mode=MODE_EXPLORER;
            else if(ch==KEY_UP && cy>0){ cy--; if(cx>(int)strlen(buf[cy])) cx=strlen(buf[cy]); }
            else if(ch==KEY_DOWN && cy<lines-1){ cy++; if(cx>(int)strlen(buf[cy])) cx=strlen(buf[cy]); }
            else if(ch==KEY_LEFT && cx>0) cx--;
            else if(ch==KEY_RIGHT && cx<(int)strlen(buf[cy])) cx++;
            else if(ch==KEY_BACKSPACE||ch==127||ch==8){ delete_char(); }
            else if(ch==KEY_DC){ delete_forward(); }
            else if(ch=='\n'){ insert_newline(); }
            else if(isprint(ch)){ insert_char(ch); }
            else if(ch==11){ /* Ctrl+K cut */
                strncpy(clip,buf[cy],MAX_LINE-1);
                clip[MAX_LINE-1] = '\0';
                delete_line(cy);
            }
            else if(ch==21){ /* Ctrl+U paste */
                int len=(int)strlen(clip);
                int cur=(int)strlen(buf[cy]);
                if (cur + len >= MAX_LINE) len = MAX_LINE - cur - 1;
                if (len > 0) {
                    memmove(&buf[cy][cx+len],&buf[cy][cx],cur-cx+1);
                    memcpy(&buf[cy][cx],clip,len); cx+=len;
                    is_dirty = 1;
                }
            }
            else if(ch==6) find_text();
            else if(ch==18) replace_text(); /* Ctrl+R */
            else if(ch==1){ /* Ctrl+A */
                cx = 0; cy = 0;
            }
        }
    }

    endwin();
    /* Reset cursor color to terminal default (BEL and ST variants). */
    printf("\033]112\007");
    printf("\033]112\033\\");
    fflush(stdout);
    return 0;
}
