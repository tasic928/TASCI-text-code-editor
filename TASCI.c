/* TASCI.c - Full-featured terminal text editor modeled after Windows Notepad
   Author: tasic928
   Features:
   - Menu bar: Edit, View, Find, Shortcuts, File, About
   - Explorer (left) + Editor (right)
   - Edit: Cut, Copy, Paste, Undo, Redo, Select All, Delete Line, Time/Date insert
   - View: Toggle line numbers, word wrap, zoom simulation, status bar toggle
   - Find: Search, Replace, Go to Line
   - File: New, Open, Save, Save As, Delete, Exit, Print(stub)
   - About: TASCI info
   - Arrow keys navigation, Enter/ESC, blinking cursor
   - Colored UI using ncurses
*/

#define _POSIX_C_SOURCE 200809L

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
#include <termios.h>
#include <errno.h>
#include <fcntl.h>

void draw_menu();
void draw_tabs();
void draw_sidebar();
void draw_editor();
void draw_status(const char *msg);
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#include <stdarg.h>

#include "syntax_highlighting.h"
#include "lsp_autocomplete.h"
#include "colours_fix.h"

#define MAX_FILES 512
#define MAX_LINE 1024
#define CLIP_CAP (1024 * 1024)
#define SIDEBAR 30
#define MENU_ITEMS 12
#define MAIN_LOOP_TIMEOUT_MS 100

enum Mode { MODE_EXPLORER, MODE_EDITOR, MODE_MENU, MODE_TABS, MODE_DIALOG };
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

#define MAX_TABS 16
typedef struct {
    char path[PATH_MAX];
    char **buf;
    int buf_cap;
    int lines;
    int cx, cy;
    int rowoff, coloff;
    int is_dirty;
    unsigned char *hl_open_comment;
    int hl_open_comment_cap;
} Tab;

static Tab tabs[MAX_TABS];
static int tab_count = 0;
static int tab_current = 0;
static int tab_sel = 0;

/* Syntax state: whether each line ends inside a block comment (for multiline comment highlighting) */
unsigned char *hl_open_comment = NULL;
int hl_open_comment_cap = 0;

int menu_sel = 0;
const char *menu_items[MENU_ITEMS] = {
    "Edit", "View", "Settings", "Find", "Shortcuts", "File", "Terminal", "Save", "Save As", "Open Folder", "Theme", "About"
};

int blink_on = 1;
static struct termios saved_termios;
static int termios_saved = 0;

/* View toggles */
int show_line_numbers = 1;
int show_status_bar = 1;
int soft_wrap = 0;

/* Clipboard */
char clip[CLIP_CAP];

/* Theme import */
static char current_theme_path[PATH_MAX] = "";
static int theme_menu_bg = COLOR_BLUE;
static int theme_menu_fg = COLOR_WHITE;
static int theme_sidebar_bg = COLOR_WHITE;
static int theme_sidebar_fg = COLOR_BLACK;
static int theme_editor_bg = COLOR_BLACK;
static int theme_editor_fg = COLOR_WHITE;
static int theme_keyword_fg = COLOR_CYAN;
static int theme_comment_fg = COLOR_GREEN;
static int theme_string_fg = COLOR_YELLOW;
static int theme_number_fg = COLOR_MAGENTA;
static int theme_preproc_fg = COLOR_BLUE;
static int theme_status_bg = COLOR_BLUE;
static int theme_status_fg = COLOR_WHITE;
static int theme_next_color = 16;
static int default_theme_menu_bg = COLOR_BLUE;
static int default_theme_menu_fg = COLOR_WHITE;
static int default_theme_sidebar_bg = COLOR_WHITE;
static int default_theme_sidebar_fg = COLOR_BLACK;
static int default_theme_editor_bg = COLOR_BLACK;
static int default_theme_editor_fg = COLOR_WHITE;
static int default_theme_keyword_fg = COLOR_CYAN;
static int default_theme_comment_fg = COLOR_GREEN;
static int default_theme_string_fg = COLOR_YELLOW;
static int default_theme_number_fg = COLOR_MAGENTA;
static int default_theme_preproc_fg = COLOR_BLUE;
static int default_theme_status_bg = COLOR_BLUE;
static int default_theme_status_fg = COLOR_WHITE;

typedef struct {
    int has_background;
    int has_menu;
    int has_sidebar;
    int has_status;
    int has_editor_bg;
    int has_editor_text;
    int has_keyword;
    int has_line_numbers;
    int has_accent;
    int bg_r, bg_g, bg_b;
    int menu_r, menu_g, menu_b;
    int sidebar_r, sidebar_g, sidebar_b;
    int status_r, status_g, status_b;
    int editor_bg_r, editor_bg_g, editor_bg_b;
    int editor_text_r, editor_text_g, editor_text_b;
    int keyword_r, keyword_g, keyword_b;
    int line_r, line_g, line_b;
    int accent_r, accent_g, accent_b;
} ThemeParsed;

/* ---------- WINDOWS ---------- */
WINDOW *menuw, *tabw, *sidew, *mainw, *statusw;
int sidebar_width = SIDEBAR;
int sidebar_on_right = 0;
char status_msg[256] = "TASCI Ready - Ctrl+X to exit";
time_t status_time = 0;

/* ---------- STATE (PERSISTED) ---------- */
static char session_restore_cwd[PATH_MAX] = "";
static char session_restore_file[PATH_MAX] = "";
static int session_restore_cx = 0;
static int session_restore_cy = 0;
static int session_restore_has_cwd = 0;
static int session_restore_has_file = 0;
static int session_restore_has_cursor = 0;
static char session_restore_theme_path[PATH_MAX] = "";
static int session_restore_has_theme = 0;

/* ---------- AUTOCOMPLETE ---------- */
#define MAX_COMPLETIONS 64
#define MAX_COMPLETION_LABEL 64

static int completion_active = 0;
static int completion_from_lsp = 0;
static int completion_sel = 0;
static int completion_count = 0;
static char completion_items[MAX_COMPLETIONS][MAX_COMPLETION_LABEL];
static char lsp_request_prefix[MAX_COMPLETION_LABEL];

static void completion_clear(void) {
    completion_active = 0;
    completion_from_lsp = 0;
    completion_sel = 0;
    completion_count = 0;
}

/* ---------- LSP ---------- */
typedef struct {
    int running;
    int initialized;
    int needs_open;
    int doc_version;
    int init_id;
    int pending_completion_id;
    pid_t pid;
    int in_fd;
    int out_fd;
    char server_name[32];
    char language_id[16];
    char doc_uri[PATH_MAX * 2];
    char root_uri[PATH_MAX * 2];
    char read_buf[16384];
    size_t read_len;
} LspClient;

static LspClient lsp = {0};

/* ---------- UTIL ---------- */
int popup_select(const char *title, const char *items[], int count);
void load_dir(void);
static void syntax_recalc_all(void);
static void lsp_prepare_for_file(const char *file, const SyntaxLang *lang);
static void set_status(const char *fmt, ...);
static void state_save(void);
void load_file(const char *f);
void save_file(void);
void save_file_as(void);
static void tab_switch(int idx);
static int tab_create_with_file(const char *path);
static void tab_restore(int idx);
static void get_mem_usage_cached(long *rss_kb_out, long *vsz_kb_out);

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
    int old_buf_cap = buf_cap;
    int old_hl_cap = hl_open_comment_cap;

    /* Keep syntax state capacity in sync with buffer capacity. */
    if (new_cap > hl_open_comment_cap) {
        unsigned char *new_hl = (unsigned char *)realloc(hl_open_comment, (size_t)new_cap);
        if (!new_hl) return 0;
        hl_open_comment = new_hl;
        hl_open_comment_cap = new_cap;
        for (int i = old_hl_cap; i < new_cap; i++) hl_open_comment[i] = 0;
    }

    char **new_buf = (char **)realloc(buf, (size_t)new_cap * sizeof(char *));
    if (!new_buf) return 0;
    buf = new_buf;
    for (int i = old_buf_cap; i < new_cap; i++) buf[i] = NULL;
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

static void tab_store_current(void) {
    if (tab_current < 0 || tab_current >= tab_count) return;
    Tab *t = &tabs[tab_current];
    strncpy(t->path, current_file, sizeof(t->path) - 1);
    t->path[sizeof(t->path) - 1] = '\0';
    t->buf = buf;
    t->buf_cap = buf_cap;
    t->lines = lines;
    t->cx = cx;
    t->cy = cy;
    t->rowoff = rowoff;
    t->coloff = coloff;
    t->is_dirty = is_dirty;
    t->hl_open_comment = hl_open_comment;
    t->hl_open_comment_cap = hl_open_comment_cap;
}

static void tab_free_buffers(Tab *t) {
    if (!t) return;
    if (t->buf) {
        for (int i = 0; i < t->lines; i++) {
            free(t->buf[i]);
        }
        free(t->buf);
        t->buf = NULL;
    }
    if (t->hl_open_comment) {
        free(t->hl_open_comment);
        t->hl_open_comment = NULL;
    }
    t->lines = 0;
    t->buf_cap = 0;
    t->hl_open_comment_cap = 0;
}

static int prompt_save_changes(void) {
    const char *items[] = { "Yes", "No", "Cancel" };
    int sel = popup_select("Do you want to save this file?", items, 3);
    if (sel == 0) return 1;
    if (sel == 1) return 0;
    return -1;
}

static int tab_close(int idx) {
    if (idx < 0 || idx >= tab_count) return 0;
    tab_store_current();
    int prev = tab_current;
    if (tabs[idx].is_dirty) {
        if (idx != tab_current) tab_switch(idx);
        int choice = prompt_save_changes();
        if (choice < 0) {
            if (idx != prev) tab_switch(prev);
            return 0;
        }
        if (choice == 1) {
            if (current_file[0]) save_file();
            else save_file_as();
        }
    }

    tab_free_buffers(&tabs[idx]);
    for (int i = idx; i < tab_count - 1; i++) {
        tabs[i] = tabs[i + 1];
    }
    tab_count--;
    if (tab_count <= 0) {
        tab_count = 0;
        tab_current = 0;
        tab_sel = 0;
        tab_create_with_file(NULL);
        return 1;
    }
    if (idx <= tab_current && tab_current > 0) tab_current--;
    if (tab_current >= tab_count) tab_current = tab_count - 1;
    if (tab_sel >= tab_count) tab_sel = tab_count - 1;
    tab_restore(tab_current);
    return 1;
}

static int confirm_exit_all(void) {
    tab_store_current();
    int start = tab_current;
    for (int i = 0; i < tab_count; i++) {
        if (!tabs[i].is_dirty) continue;
        tab_switch(i);
        int choice = prompt_save_changes();
        if (choice < 0) {
            tab_switch(start);
            return 0;
        }
        if (choice == 1) {
            if (current_file[0]) save_file();
            else save_file_as();
        }
    }
    tab_switch(start);
    return 1;
}

static void tab_restore(int idx) {
    if (idx < 0 || idx >= tab_count) return;
    Tab *t = &tabs[idx];
    buf = t->buf;
    buf_cap = t->buf_cap;
    lines = t->lines;
    cx = t->cx;
    cy = t->cy;
    rowoff = t->rowoff;
    coloff = t->coloff;
    is_dirty = t->is_dirty;
    hl_open_comment = t->hl_open_comment;
    hl_open_comment_cap = t->hl_open_comment_cap;
    strncpy(current_file, t->path, sizeof(current_file) - 1);
    current_file[sizeof(current_file) - 1] = '\0';
    if (!buf) buffer_init_if_needed();
    const SyntaxLang *lang = sh_lang_for_file(current_file);
    lsp_prepare_for_file(current_file, lang);
    syntax_recalc_all();
    state_save();
}

static void tabs_init_from_current(void) {
    if (tab_count > 0) return;
    tab_count = 1;
    tab_current = 0;
    tab_sel = 0;
    tab_store_current();
}

static int tab_find_by_path(const char *path) {
    if (!path || !path[0]) return -1;
    for (int i = 0; i < tab_count; i++) {
        if (tabs[i].path[0] && strcmp(tabs[i].path, path) == 0) return i;
    }
    return -1;
}

static int tab_create_with_file(const char *path) {
    if (tab_count >= MAX_TABS) {
        set_status("Max tabs reached");
        return -1;
    }
    if (tab_count > 0) tab_store_current();

    buf = NULL;
    buf_cap = 0;
    lines = 0;
    hl_open_comment = NULL;
    hl_open_comment_cap = 0;
    buffer_init_if_needed();

    if (path && path[0]) {
        load_file(path);
    } else {
        buffer_clear();
        buffer_ensure_capacity(1);
        if (!buf[0]) buf[0] = line_alloc_empty();
        buf[0][0] = '\0';
        lines = 1;
        cx = cy = rowoff = coloff = 0;
        is_dirty = 0;
        current_file[0] = '\0';
        if (hl_open_comment) memset(hl_open_comment, 0, (size_t)hl_open_comment_cap);
        const SyntaxLang *lang = sh_lang_for_file(current_file);
        lsp_prepare_for_file(current_file, lang);
        syntax_recalc_all();
        state_save();
    }

    tab_current = tab_count;
    tab_count++;
    tab_store_current();
    return tab_current;
}

static void tab_switch(int idx) {
    if (idx < 0 || idx >= tab_count || idx == tab_current) return;
    completion_clear();
    tab_store_current();
    tab_current = idx;
    tab_sel = idx;
    tab_restore(tab_current);
}

static void tab_next(void) {
    if (tab_count < 2) return;
    int next = tab_current + 1;
    if (next >= tab_count) next = 0;
    tab_switch(next);
}

static void tab_prev(void) {
    if (tab_count < 2) return;
    int prev = tab_current - 1;
    if (prev < 0) prev = tab_count - 1;
    tab_switch(prev);
}

static void tab_open_file(const char *path) {
    int idx = tab_find_by_path(path);
    if (idx >= 0) {
        tab_switch(idx);
        return;
    }
    tab_create_with_file(path);
}

static void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_msg, sizeof(status_msg), fmt, ap);
    va_end(ap);
    status_time = time(NULL);
}

static void disable_flow_control(void) {
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) != 0) return;
    if (!termios_saved) {
        saved_termios = t;
        termios_saved = 1;
    }
    t.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void restore_flow_control(void) {
    if (!termios_saved) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
}

static void get_mem_usage_cached(long *rss_kb_out, long *vsz_kb_out) {
#ifdef __linux__
    static struct timeval last_check = {0, 0};
    static long last_rss_kb = 0;
    static long last_vsz_kb = 0;
    struct timeval now;
    gettimeofday(&now, NULL);
    long elapsed_ms = (now.tv_sec - last_check.tv_sec) * 1000L
                    + (now.tv_usec - last_check.tv_usec) / 1000L;
    if (elapsed_ms < 1000) {
        if (rss_kb_out) *rss_kb_out = last_rss_kb;
        if (vsz_kb_out) *vsz_kb_out = last_vsz_kb;
        return;
    }
    last_check = now;

    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) {
        if (rss_kb_out) *rss_kb_out = last_rss_kb;
        if (vsz_kb_out) *vsz_kb_out = last_vsz_kb;
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            long kb = 0;
            if (sscanf(line + 6, "%ld", &kb) == 1 && kb > 0) {
                last_rss_kb = kb;
            }
            continue;
        }
        if (strncmp(line, "VmSize:", 7) == 0) {
            long kb = 0;
            if (sscanf(line + 7, "%ld", &kb) == 1 && kb > 0) {
                last_vsz_kb = kb;
            }
            continue;
        }
    }
    fclose(fp);
    if (rss_kb_out) *rss_kb_out = last_rss_kb;
    if (vsz_kb_out) *vsz_kb_out = last_vsz_kb;
    return;
#else
    if (rss_kb_out) *rss_kb_out = 0;
    if (vsz_kb_out) *vsz_kb_out = 0;
    return;
#endif
}

/* ---------- SYNTAX (VSCode-like basics) ---------- */
static int lang_has_string_delim(const SyntaxLang *lang, char c) {
    return (lang && lang->string_delims && strchr(lang->string_delims, c) != NULL);
}

static int lang_is_c_preproc(const SyntaxLang *lang) {
    if (!lang || !lang->name) return 0;
    return (strcmp(lang->name, "C") == 0 ||
            strcmp(lang->name, "C++") == 0 ||
            strcmp(lang->name, "Objective_C") == 0);
}

static unsigned char syntax_calc_line_end_open_comment(const SyntaxLang *lang,
                                                       const char *line,
                                                       unsigned char in_comment) {
    if (!lang || !line) return 0;
    const char *lc = lang->line_comment;
    const char *bcs = lang->block_comment_start;
    const char *bce = lang->block_comment_end;
    int lc_len = lc ? (int)strlen(lc) : 0;
    int bcs_len = bcs ? (int)strlen(bcs) : 0;
    int bce_len = bce ? (int)strlen(bce) : 0;

    for (int i = 0; line[i];) {
        if (!in_comment && lang_has_string_delim(lang, line[i])) {
            char delim = line[i++];
            while (line[i]) {
                if (line[i] == '\\' && line[i + 1]) { i += 2; continue; }
                if (line[i] == delim) { i++; break; }
                i++;
            }
            continue;
        }
        if (in_comment) {
            if (bce_len && strncmp(&line[i], bce, (size_t)bce_len) == 0) {
                in_comment = 0;
                i += bce_len;
                continue;
            }
            i++;
            continue;
        }
        if (lc_len && strncmp(&line[i], lc, (size_t)lc_len) == 0) break;
        if (bcs_len && strncmp(&line[i], bcs, (size_t)bcs_len) == 0) {
            in_comment = 1;
            i += bcs_len;
            continue;
        }
        i++;
    }
    return in_comment;
}

static void syntax_recalc_all(void) {
    const SyntaxLang *lang = sh_lang_for_file(current_file);
    if (!hl_open_comment || !buf) return;
    unsigned char in_comment = 0;
    for (int i = 0; i < lines; i++) {
        in_comment = syntax_calc_line_end_open_comment(lang, buf[i], in_comment);
        hl_open_comment[i] = in_comment;
    }
}

static void syntax_recalc_from(int start_line, int min_lines) {
    const SyntaxLang *lang = sh_lang_for_file(current_file);
    if (!hl_open_comment || !buf) return;
    if (start_line < 0) start_line = 0;
    if (start_line >= lines) return;
    if (min_lines < 1) min_lines = 1;
    unsigned char in_comment = (start_line > 0) ? hl_open_comment[start_line - 1] : 0;
    int updated = 0;
    for (int i = start_line; i < lines; i++) {
        unsigned char old = hl_open_comment[i];
        in_comment = syntax_calc_line_end_open_comment(lang, buf[i], in_comment);
        hl_open_comment[i] = in_comment;
        updated++;
        if (updated >= min_lines && hl_open_comment[i] == old) break;
    }
}

static void uri_encode(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == ' ') {
            out[o++] = '%'; out[o++] = '2'; out[o++] = '0';
        } else if (c == '#') {
            out[o++] = '%'; out[o++] = '2'; out[o++] = '3';
        } else if (c == '%') {
            out[o++] = '%'; out[o++] = '2'; out[o++] = '5';
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

static void make_file_uri(const char *path, char *out, size_t out_sz) {
    char abs[PATH_MAX];
    if (path[0] == '/') {
        strncpy(abs, path, sizeof(abs) - 1);
        abs[sizeof(abs) - 1] = '\0';
    } else {
        char cwd_buf[PATH_MAX];
        if (!getcwd(cwd_buf, sizeof(cwd_buf))) cwd_buf[0] = '\0';
        snprintf(abs, sizeof(abs), "%s/%s", cwd_buf, path);
    }
    char enc[PATH_MAX * 2];
    uri_encode(abs, enc, sizeof(enc));
    snprintf(out, out_sz, "file://%s", enc);
}

static char *buffer_to_text(size_t *out_len) {
    size_t total = 0;
    for (int i = 0; i < lines; i++) {
        total += strlen(buf[i]);
        if (i < lines - 1) total += 1;
    }
    char *text = (char *)malloc(total + 1);
    if (!text) return NULL;
    size_t pos = 0;
    for (int i = 0; i < lines; i++) {
        size_t len = strlen(buf[i]);
        memcpy(text + pos, buf[i], len);
        pos += len;
        if (i < lines - 1) text[pos++] = '\n';
    }
    text[pos] = '\0';
    if (out_len) *out_len = pos;
    return text;
}

static char *json_escape_text(const char *in) {
    size_t len = strlen(in);
    size_t cap = len * 2 + 8;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = in[i];
        if (c == '\\' || c == '\"') {
            if (o + 2 >= cap) break;
            out[o++] = '\\';
            out[o++] = c;
        } else if (c == '\n') {
            if (o + 2 >= cap) break;
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            if (o + 2 >= cap) break;
            out[o++] = '\\';
            out[o++] = 'r';
        } else if (c == '\t') {
            if (o + 2 >= cap) break;
            out[o++] = '\\';
            out[o++] = 't';
        } else {
            if (o + 1 >= cap) break;
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return out;
}

static void str_rstrip(char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0) {
        unsigned char c = (unsigned char)s[n - 1];
        if (c == '\n' || c == '\r' || isspace(c)) {
            s[--n] = '\0';
            continue;
        }
        break;
    }
}

static char *str_lstrip(char *s) {
    while (s && *s && isspace((unsigned char)*s)) s++;
    return s;
}

static void str_strip_inplace(char *s) {
    if (!s) return;
    char *p = str_lstrip(s);
    if (p != s) memmove(s, p, strlen(p) + 1);
    str_rstrip(s);
}

static int ensure_dir_recursive(const char *path) {
    if (!path || !path[0]) return 0;
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return 0;
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return 0;
    return 1;
}

static int get_state_paths(char *dir_out, size_t dir_out_sz, char *file_out, size_t file_out_sz) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char base[PATH_MAX];
    if (xdg && xdg[0]) {
        snprintf(base, sizeof(base), "%s", xdg);
    } else if (home && home[0]) {
        snprintf(base, sizeof(base), "%s/.config", home);
    } else {
        return 0;
    }
    snprintf(dir_out, dir_out_sz, "%s/tasci", base);
    snprintf(file_out, file_out_sz, "%s/state.ini", dir_out);
    return 1;
}

static void state_save(void) {
    char dir[PATH_MAX];
    char path[PATH_MAX];
    if (!get_state_paths(dir, sizeof(dir), path, sizeof(path))) return;
    if (!ensure_dir_recursive(dir)) return;
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *fp = fopen(tmp, "w");
    if (!fp) return;

    char cwd_now[PATH_MAX];
    if (!getcwd(cwd_now, sizeof(cwd_now))) cwd_now[0] = '\0';

    fprintf(fp, "show_line_numbers=%d\n", show_line_numbers ? 1 : 0);
    fprintf(fp, "show_status_bar=%d\n", show_status_bar ? 1 : 0);
    fprintf(fp, "soft_wrap=%d\n", soft_wrap ? 1 : 0);
    fprintf(fp, "sidebar_right=%d\n", sidebar_on_right ? 1 : 0);
    fprintf(fp, "cwd=%s\n", cwd_now);
    fprintf(fp, "file=%s\n", current_file);
    fprintf(fp, "cx=%d\n", cx);
    fprintf(fp, "cy=%d\n", cy);
    if (current_theme_path[0]) {
        fprintf(fp, "theme=%s\n", current_theme_path);
    }

    fclose(fp);
    (void)rename(tmp, path);
}

static void state_load(void) {
    char dir[PATH_MAX];
    char path[PATH_MAX];
    if (!get_state_paths(dir, sizeof(dir), path, sizeof(path))) return;
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[PATH_MAX * 2];
    int have_cx = 0, have_cy = 0;
    session_restore_has_theme = 0;
    while (fgets(line, sizeof(line), fp)) {
        str_rstrip(line);
        char *p = str_lstrip(line);
        if (!p[0] || p[0] == '#') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        str_strip_inplace(key);
        str_strip_inplace(val);
        if (strcmp(key, "show_line_numbers") == 0) show_line_numbers = atoi(val) ? 1 : 0;
        else if (strcmp(key, "show_status_bar") == 0) show_status_bar = atoi(val) ? 1 : 0;
        else if (strcmp(key, "soft_wrap") == 0) soft_wrap = atoi(val) ? 1 : 0;
        else if (strcmp(key, "sidebar_right") == 0) sidebar_on_right = atoi(val) ? 1 : 0;
        else if (strcmp(key, "cwd") == 0) {
            if (val[0]) {
                strncpy(session_restore_cwd, val, sizeof(session_restore_cwd) - 1);
                session_restore_cwd[sizeof(session_restore_cwd) - 1] = '\0';
                session_restore_has_cwd = 1;
            }
        } else if (strcmp(key, "file") == 0) {
            if (val[0]) {
                strncpy(session_restore_file, val, sizeof(session_restore_file) - 1);
                session_restore_file[sizeof(session_restore_file) - 1] = '\0';
                session_restore_has_file = 1;
            }
        } else if (strcmp(key, "cx") == 0) {
            session_restore_cx = atoi(val);
            have_cx = 1;
        } else if (strcmp(key, "cy") == 0) {
            session_restore_cy = atoi(val);
            have_cy = 1;
        } else if (strcmp(key, "theme") == 0) {
            if (val[0]) {
                strncpy(session_restore_theme_path, val, sizeof(session_restore_theme_path) - 1);
                session_restore_theme_path[sizeof(session_restore_theme_path) - 1] = '\0';
                session_restore_has_theme = 1;
            }
        }
    }
    fclose(fp);
    if (have_cx && have_cy) session_restore_has_cursor = 1;
}

static int is_lsp_lang(const SyntaxLang *lang) {
    (void)lang;
    return 0;
}

static const char *lsp_cmd_for_lang(const char *name) {
    if (!name) return NULL;
    if (strcmp(name, "C") == 0) {
        const char *env = getenv("TASCI_LSP_C");
        return env && env[0] ? env : "clangd";
    }
    if (strcmp(name, "Assembly") == 0) {
        const char *env = getenv("TASCI_LSP_ASM");
        return env && env[0] ? env : "asm-lsp";
    }
    return NULL;
}

static int lsp_send_raw(const char *json, size_t len) {
    if (!lsp.running) return 0;
    char header[64];
    int hlen = snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", len);
    if (write(lsp.in_fd, header, (size_t)hlen) < 0) return 0;
    if (write(lsp.in_fd, json, len) < 0) return 0;
    return 1;
}

static int lsp_send_fmt(const char *fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len <= 0) return 0;
    if ((size_t)len >= sizeof(buf)) len = (int)sizeof(buf) - 1;
    return lsp_send_raw(buf, (size_t)len);
}

static void lsp_shutdown(void) {
    if (!lsp.running) return;
    lsp_send_fmt("{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"shutdown\"}", lsp.init_id + 1000);
    const char *exit_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}";
    lsp_send_raw(exit_msg, strlen(exit_msg));
    close(lsp.in_fd);
    close(lsp.out_fd);
    if (lsp.pid > 0) {
        kill(lsp.pid, SIGTERM);
        waitpid(lsp.pid, NULL, 0);
    }
    memset(&lsp, 0, sizeof(lsp));
    completion_clear();
}

static int lsp_spawn(const char *cmd, const char *server_name) {
    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) != 0) return 0;
    if (pipe(outpipe) != 0) {
        close(inpipe[0]); close(inpipe[1]);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(inpipe[0]); close(inpipe[1]);
        close(outpipe[0]); close(outpipe[1]);
        return 0;
    }
    if (pid == 0) {
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        close(inpipe[1]);
        close(outpipe[0]);
        const int max_args = 8;
        char *argv[max_args + 1];
        char cmd_copy[256];
        strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
        cmd_copy[sizeof(cmd_copy) - 1] = '\0';
        int argc = 0;
        char *tok = strtok(cmd_copy, " ");
        while (tok && argc < max_args) {
            argv[argc++] = tok;
            tok = strtok(NULL, " ");
        }
        argv[argc] = NULL;
        execvp(argv[0], argv);
        _exit(1);
    }

    close(inpipe[0]);
    close(outpipe[1]);
    int flags = fcntl(outpipe[0], F_GETFL, 0);
    fcntl(outpipe[0], F_SETFL, flags | O_NONBLOCK);

    memset(&lsp, 0, sizeof(lsp));
    lsp.running = 1;
    lsp.initialized = 0;
    lsp.needs_open = 0;
    lsp.doc_version = 0;
    lsp.pid = pid;
    lsp.in_fd = inpipe[1];
    lsp.out_fd = outpipe[0];
    lsp.read_len = 0;
    lsp.pending_completion_id = -1;
    lsp.init_id = 1;
    strncpy(lsp.server_name, server_name ? server_name : "lsp", sizeof(lsp.server_name) - 1);
    return 1;
}

static void lsp_send_initialize(void) {
    int pid = (int)getpid();
    lsp.init_id = 1;
    lsp_send_fmt("{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"initialize\",\"params\":{\"processId\":%d,\"rootUri\":\"%s\",\"capabilities\":{\"textDocument\":{\"completion\":{\"completionItem\":{\"snippetSupport\":false}}}}}}",
                 lsp.init_id, pid, lsp.root_uri);
}

static void lsp_send_initialized(void) {
    const char *msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}";
    lsp_send_raw(msg, strlen(msg));
}

static void lsp_send_did_open(void) {
    char *text = buffer_to_text(NULL);
    if (!text) return;
    char *esc = json_escape_text(text);
    free(text);
    if (!esc) return;
    lsp.doc_version = 1;
    size_t cap = strlen(esc) + strlen(lsp.doc_uri) + strlen(lsp.language_id) + 256;
    char *json = (char *)malloc(cap);
    if (json) {
        int len = snprintf(json, cap,
                           "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"%s\",\"languageId\":\"%s\",\"version\":%d,\"text\":\"%s\"}}}",
                           lsp.doc_uri, lsp.language_id, lsp.doc_version, esc);
        if (len > 0) lsp_send_raw(json, (size_t)len);
        free(json);
    }
    free(esc);
}

static void lsp_send_did_change(void) {
    if (!lsp.initialized) return;
    char *text = buffer_to_text(NULL);
    if (!text) return;
    char *esc = json_escape_text(text);
    free(text);
    if (!esc) return;
    lsp.doc_version++;
    size_t cap = strlen(esc) + strlen(lsp.doc_uri) + 256;
    char *json = (char *)malloc(cap);
    if (json) {
        int len = snprintf(json, cap,
                           "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"%s\",\"version\":%d},\"contentChanges\":[{\"text\":\"%s\"}]}}",
                           lsp.doc_uri, lsp.doc_version, esc);
        if (len > 0) lsp_send_raw(json, (size_t)len);
        free(json);
    }
    free(esc);
}

static int json_extract_id(const char *json) {
    const char *p = strstr(json, "\"id\"");
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!isdigit((unsigned char)*p) && *p != '-') return -1;
    return atoi(p);
}

static const char *json_parse_string(const char *p, char *out, size_t out_sz) {
    if (!p || *p != '"') return NULL;
    p++;
    size_t o = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            char e = *p++;
            if (e == 'n') c = '\n';
            else if (e == 't') c = '\t';
            else c = e;
        }
        if (o + 1 < out_sz) out[o++] = c;
    }
    out[o] = '\0';
    return (*p == '"') ? (p + 1) : NULL;
}

static void lsp_parse_completions(const char *json, const char *prefix) {
    completion_clear();
    completion_from_lsp = 1;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    const char *p = json;
    while ((p = strstr(p, "\"label\"")) != NULL) {
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        char label[MAX_COMPLETION_LABEL];
        const char *next = json_parse_string(p, label, sizeof(label));
        if (!next) { p++; continue; }
        if (prefix_len == 0 || strncmp(label, prefix, prefix_len) == 0) {
            if (completion_count < MAX_COMPLETIONS) {
                strncpy(completion_items[completion_count], label, MAX_COMPLETION_LABEL - 1);
                completion_items[completion_count][MAX_COMPLETION_LABEL - 1] = '\0';
                completion_count++;
            }
        }
        p = next;
    }
    if (completion_count > 0) completion_active = 1;
}

static void lsp_poll(void) {
    if (!lsp.running) return;
    char tmp[4096];
    ssize_t n = read(lsp.out_fd, tmp, sizeof(tmp));
    if (n == 0) {
        lsp_shutdown();
        return;
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        lsp_shutdown();
        return;
    }
    if (lsp.read_len + (size_t)n >= sizeof(lsp.read_buf)) lsp.read_len = 0;
    memcpy(lsp.read_buf + lsp.read_len, tmp, (size_t)n);
    lsp.read_len += (size_t)n;

    while (1) {
        char *header_end = NULL;
        for (size_t i = 0; i + 3 < lsp.read_len; i++) {
            if (lsp.read_buf[i] == '\r' && lsp.read_buf[i+1] == '\n' &&
                lsp.read_buf[i+2] == '\r' && lsp.read_buf[i+3] == '\n') {
                header_end = lsp.read_buf + i + 4;
                break;
            }
        }
        if (!header_end) break;
        int content_length = 0;
        char header[128];
        size_t header_size = (size_t)(header_end - lsp.read_buf);
        size_t copy_len = header_size < sizeof(header) - 1 ? header_size : sizeof(header) - 1;
        memcpy(header, lsp.read_buf, copy_len);
        header[copy_len] = '\0';
        if (sscanf(header, "Content-Length: %d", &content_length) != 1) {
            lsp.read_len = 0;
            break;
        }
        size_t header_len = (size_t)(header_end - lsp.read_buf);
        if (lsp.read_len < header_len + (size_t)content_length) break;
        char *json = (char *)malloc((size_t)content_length + 1);
        if (!json) return;
        memcpy(json, lsp.read_buf + header_len, (size_t)content_length);
        json[content_length] = '\0';

        int id = json_extract_id(json);
        if (id == lsp.init_id && !lsp.initialized) {
            lsp.initialized = 1;
            lsp_send_initialized();
            if (lsp.needs_open) {
                lsp_send_did_open();
                lsp.needs_open = 0;
            }
        } else if (id == lsp.pending_completion_id && lsp.pending_completion_id > 0) {
            lsp_parse_completions(json, lsp_request_prefix);
            lsp.pending_completion_id = -1;
        }

        free(json);
        size_t remain = lsp.read_len - header_len - (size_t)content_length;
        memmove(lsp.read_buf, lsp.read_buf + header_len + (size_t)content_length, remain);
        lsp.read_len = remain;
    }
}

static void lsp_prepare_for_file(const char *file, const SyntaxLang *lang) {
    if (!is_lsp_lang(lang)) {
        lsp_shutdown();
        return;
    }
    const char *cmd = lsp_cmd_for_lang(lang->name);
    if (!cmd || !cmd[0]) {
        lsp_shutdown();
        return;
    }
    if (!lsp.running || strcmp(lsp.server_name, lang->name) != 0) {
        lsp_shutdown();
        if (!lsp_spawn(cmd, lang->name)) {
            set_status("LSP start failed: %s", cmd);
            return;
        }
        char cwd_buf[PATH_MAX];
        if (!getcwd(cwd_buf, sizeof(cwd_buf))) cwd_buf[0] = '\0';
        make_file_uri(cwd_buf, lsp.root_uri, sizeof(lsp.root_uri));
        lsp_send_initialize();
    }
    make_file_uri(file, lsp.doc_uri, sizeof(lsp.doc_uri));
    if (strcmp(lang->name, "Assembly") == 0) {
        strncpy(lsp.language_id, "asm", sizeof(lsp.language_id) - 1);
    } else {
        strncpy(lsp.language_id, "c", sizeof(lsp.language_id) - 1);
    }
    lsp.language_id[sizeof(lsp.language_id) - 1] = '\0';
    lsp.needs_open = 1;
    if (lsp.initialized) {
        lsp_send_did_open();
        lsp.needs_open = 0;
    }
}

static void lsp_request_completion(void) {
    if (!lsp.running || !lsp.initialized) return;
    int id = lsp.init_id + 100 + lsp.doc_version;
    lsp.pending_completion_id = id;
    lsp_send_fmt("{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d}}}",
                 id, lsp.doc_uri, cy, cx);
}

static int get_word_prefix(char *out, size_t out_sz, int *start_out) {
    if (!buf || cy < 0 || cy >= lines) return 0;
    int start = cx;
    while (start > 0) {
        char c = buf[cy][start - 1];
        if (isalnum((unsigned char)c) || c == '_') start--;
        else break;
    }
    int len = cx - start;
    if (len <= 0) return 0;
    if ((size_t)len >= out_sz) len = (int)out_sz - 1;
    memcpy(out, &buf[cy][start], (size_t)len);
    out[len] = '\0';
    if (start_out) *start_out = start;
    return len;
}

static void keyword_completion(const SyntaxLang *lang, const char *prefix) {
    completion_clear();
    if (!lang || !lang->keywords || !prefix || !prefix[0]) return;
    if (!autocomplete_lang_enabled(lang->name)) return;
    size_t prefix_len = strlen(prefix);
    for (int i = 0; lang->keywords[i]; i++) {
        const char *kw = lang->keywords[i];
        if (strncmp(kw, prefix, prefix_len) == 0) {
            if (completion_count < MAX_COMPLETIONS) {
                strncpy(completion_items[completion_count], kw, MAX_COMPLETION_LABEL - 1);
                completion_items[completion_count][MAX_COMPLETION_LABEL - 1] = '\0';
                completion_count++;
            }
        }
    }
    if (completion_count > 0) completion_active = 1;
}

static void completion_trigger_with_char(const SyntaxLang *lang, int ch) {
    char prefix[MAX_COMPLETION_LABEL];
    int has_prefix = get_word_prefix(prefix, sizeof(prefix), NULL);
    if (!has_prefix && is_lsp_lang(lang) && lsp.running && lsp.initialized) {
        if (ch == '.' || ch == '>' || ch == ':' ) {
            lsp_request_prefix[0] = '\0';
            completion_from_lsp = 1;
            lsp_request_completion();
            return;
        }
    }
    if (!has_prefix) {
        completion_clear();
        return;
    }
    if (is_lsp_lang(lang) && lsp.running && lsp.initialized) {
        strncpy(lsp_request_prefix, prefix, sizeof(lsp_request_prefix) - 1);
        lsp_request_prefix[sizeof(lsp_request_prefix) - 1] = '\0';
        completion_from_lsp = 1;
        lsp_request_completion();
        return;
    }
    completion_from_lsp = 0;
    keyword_completion(lang, prefix);
}

static void apply_completion(void) {
    if (!completion_active || completion_count == 0) return;
    char prefix[MAX_COMPLETION_LABEL];
    int start = 0;
    int len = get_word_prefix(prefix, sizeof(prefix), &start);
    if (len <= 0) { completion_clear(); return; }
    const char *label = completion_items[completion_sel];
    int line_len = (int)strlen(buf[cy]);
    int label_len = (int)strlen(label);
    int new_len = line_len - len + label_len;
    if (new_len >= MAX_LINE) return;
    memmove(&buf[cy][start + label_len], &buf[cy][start + len], (size_t)(line_len - (start + len) + 1));
    memcpy(&buf[cy][start], label, (size_t)label_len);
    cx = start + label_len;
    is_dirty = 1;
    completion_clear();
    lsp_send_did_change();
}

static void layout_windows(void) {
    int h, w;
    getmaxyx(stdscr, h, w);
    int side = SIDEBAR;
    if (w < 50) side = w / 3;
    if (side < 10) side = 10;
    if (side > w - 1) side = (w > 1) ? (w - 1) : 1;
    sidebar_width = side;
    wresize(menuw, 1, w); mvwin(menuw, 0, 0);
    int side_x = sidebar_on_right ? (w - side) : 0;
    int main_x = sidebar_on_right ? 0 : side;
    wresize(tabw, 1, w); mvwin(tabw, 1, 0);
    int content_h = h - 3;
    if (content_h < 1) content_h = 1;
    wresize(sidew, content_h, side); mvwin(sidew, 2, side_x);
    wresize(mainw, content_h, w - side); mvwin(mainw, 2, main_x);
    wresize(statusw, 1, w); mvwin(statusw, h - 1, 0);
}

static void popup_input(const char *title, const char *label, char *out, size_t out_sz) {
    int h = 9, w = 64;
    int sy = (LINES - h) / 2;
    int sx = (COLS - w) / 2;
    WINDOW *wpopup = newwin(h, w, sy, sx);
    keypad(wpopup, TRUE);
    wtimeout(wpopup, -1);
    box(wpopup, 0, 0);
    mvwprintw(wpopup, 1, 2, "%s", title);
    mvwprintw(wpopup, 3, 2, "%s", label);
    mvwprintw(wpopup, 5, 2, "> ");
    wrefresh(wpopup);
    echo();
    curs_set(1);
    mvwgetnstr(wpopup, 5, 4, out, (int)out_sz - 1);
    noecho();
    curs_set(0);
    delwin(wpopup);
}

/* ---------- FILE UTILITIES ---------- */
int is_dir(const char *f) {
    struct stat st;
    return stat(f, &st) == 0 && S_ISDIR(st.st_mode);
}

static void delete_selected_file(void) {
    if (file_count <= 0) return;
    const char *name = files[sel];
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        set_status("Cannot delete %s", name);
        return;
    }
    if (is_dir(name)) {
        set_status("Delete failed: %s is a directory", name);
        return;
    }
    const char *items[] = { "Cancel", "Delete" };
    int choice = popup_select("Delete file?", items, 2);
    if (choice != 1) {
        set_status("Delete canceled");
        return;
    }
    if (remove(name) == 0) {
        set_status("Deleted: %s", name);
        load_dir();
        if (sel >= file_count && file_count > 0) sel = file_count - 1;
    } else {
        set_status("Delete failed: %s", name);
    }
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
    const SyntaxLang *lang = sh_lang_for_file(current_file);
    lsp_prepare_for_file(current_file, lang);
    state_save();
    syntax_recalc_all();
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
    tab_store_current();
}

void save_file_as() {
    char fname[256];
    popup_input("Save As", "Enter file name (with extension):", fname, sizeof(fname));
    if(strlen(fname)==0) return;
    strncpy(current_file,fname,sizeof(current_file)-1);
    current_file[sizeof(current_file)-1]='\0';
    save_file();
    load_dir();
    const SyntaxLang *lang = sh_lang_for_file(current_file);
    lsp_prepare_for_file(current_file, lang);
    state_save();
    syntax_recalc_all();
    tab_store_current();
}

/* ---------- POPUP ---------- */
static void popup_print_wrapped(WINDOW *wpopup, int starty, int maxw, int max_lines, const char *msg) {
    int y = starty;
    const char *p = msg;
    if (maxw <= 0 || max_lines <= 0) return;
    while (*p && y < starty + max_lines) {
        int len = 0;
        int last_space = -1;
        while (p[len] && p[len] != '\n' && len < maxw) {
            if (p[len] == ' ') last_space = len;
            len++;
        }
        int line_len = len;
        if (p[len] == '\n') {
            line_len = len;
        } else if (len == maxw && last_space > 0) {
            line_len = last_space;
        }
        if (line_len > 0) {
            mvwprintw(wpopup, y, 2, "%.*s", line_len, p);
            y++;
            p += line_len;
        }
        while (*p == ' ') p++;
        if (*p == '\n') p++;
        if (line_len == 0 && *p && *p != '\n') {
            p++;
        }
    }
}

void popup(const char *title,const char *msg){
    int h=10,w=60,sy=(LINES-h)/2,sx=(COLS-w)/2;
    WINDOW *wpopup=newwin(h,w,sy,sx);
    box(wpopup,0,0);
    mvwprintw(wpopup,1,2,"%s",title);
    popup_print_wrapped(wpopup, 3, w - 4, h - 5, msg);
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

static void settings_dialog(void) {
    int h = 12, w = 54;
    int sy = (LINES - h) / 2;
    int sx = (COLS - w) / 2;
    if (h > LINES - 2) h = LINES - 2;
    if (w > COLS - 2) w = COLS - 2;
    WINDOW *wpopup = newwin(h, w, sy, sx);
    keypad(wpopup, TRUE);
    int sel = 0;
    int ch;
    while (1) {
        char item0[64], item1[64], item2[64], item3[64];
        snprintf(item0, sizeof(item0), "Explorer Side: %s", sidebar_on_right ? "Right" : "Left");
        snprintf(item1, sizeof(item1), "Line Numbers: %s", show_line_numbers ? "On" : "Off");
        snprintf(item2, sizeof(item2), "Status Bar: %s", show_status_bar ? "On" : "Off");
        snprintf(item3, sizeof(item3), "Word Wrap: %s", soft_wrap ? "On" : "Off");
        const char *items[] = { item0, item1, item2, item3, "Close" };
        int count = (int)(sizeof(items) / sizeof(items[0]));

        werase(wpopup);
        box(wpopup, 0, 0);
        mvwprintw(wpopup, 1, 2, "Settings");
        mvwprintw(wpopup, 2, 2, "Enter=toggle  Esc=close");
        for (int i = 0; i < count && (i + 4) < h - 1; i++) {
            if (i == sel) wattron(wpopup, A_REVERSE);
            mvwprintw(wpopup, i + 4, 2, "%s", items[i]);
            if (i == sel) wattroff(wpopup, A_REVERSE);
        }
        wrefresh(wpopup);

        ch = wgetch(wpopup);
        if (ch == 27) break;
        if (ch == KEY_UP && sel > 0) sel--;
        else if (ch == KEY_DOWN && sel < count - 1) sel++;
        else if (ch == '\n') {
            if (sel == 0) { sidebar_on_right = !sidebar_on_right; layout_windows(); }
            else if (sel == 1) show_line_numbers = !show_line_numbers;
            else if (sel == 2) show_status_bar = !show_status_bar;
            else if (sel == 3) soft_wrap = !soft_wrap;
            else if (sel == 4) break;
            state_save();
        }
    }
    delwin(wpopup);
    set_status("Settings updated");
}

static void shortcuts_dialog(void) {
    static const char *items[] = {
        "Global",
        "  Ctrl+X        Exit",
        "  Ctrl+S        Save",
        "  F5/F6         Prev/Next tab",
        "",
        "Explorer (file list)",
        "  Up/Down       Move selection (Up at top opens menu)",
        "  Enter         Open file / enter folder",
        "  Backspace     Up one folder",
        "  Delete/Ctrl+D Delete selected file",
        "",
        "Menu bar",
        "  Left/Right    Move between menus",
        "  Enter         Activate menu item",
        "  Down          Back to explorer",
        "  Esc           Close menu",
        "",
        "Tabs",
        "  Up (from explorer) Focus tabs",
        "  Left/Right    Switch tab",
        "  X/Delete      Close tab",
        "  Up            Focus menu bar",
        "  Down/Esc      Back to explorer",
        "",
        "Editor",
        "  Esc           Back to explorer",
        "  Arrow keys    Move cursor",
        "  Enter         New line",
        "  Backspace     Delete left",
        "  Delete        Delete right",
        "  Ctrl+F        Find",
        "  Ctrl+R        Replace",
        "  Ctrl+K        Cut line",
        "  Ctrl+U        Paste",
        "  Ctrl+A        Jump to start (top-left)",
        "  Ctrl+W        Toggle word wrap",
        "",
        "Autocomplete",
        "  Tab/Enter     Accept suggestion",
        "  Up/Down       Select suggestion",
        "  Esc           Dismiss suggestion",
    };
    int count = (int)(sizeof(items) / sizeof(items[0]));

    int maxlen = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(items[i]);
        if (len > maxlen) maxlen = len;
    }

    int h = count + 6;
    int w = maxlen + 6;
    if (h > LINES - 2) h = LINES - 2;
    if (w > COLS - 2) w = COLS - 2;
    if (h < 12) h = 12;
    if (w < 44) w = 44;
    int sy = (LINES - h) / 2;
    int sx = (COLS - w) / 2;

    WINDOW *wpopup = newwin(h, w, sy, sx);
    keypad(wpopup, TRUE);
    wtimeout(wpopup, -1);

    int sel = 0;
    int top = 0;
    int list_rows = h - 5;
    if (list_rows < 1) list_rows = 1;

    while (1) {
        werase(wpopup);
        box(wpopup, 0, 0);
        mvwprintw(wpopup, 1, 2, "Shortcuts");
        mvwprintw(wpopup, 2, 2, "Up/Down=scroll  PgUp/PgDn=page  Enter=close  Esc=close");

        for (int i = 0; i < list_rows; i++) {
            int idx = top + i;
            if (idx >= count) break;
            if (idx == sel) wattron(wpopup, A_REVERSE);
            mvwaddnstr(wpopup, 4 + i, 2, items[idx], w - 4);
            if (idx == sel) wattroff(wpopup, A_REVERSE);
        }
        wrefresh(wpopup);

        int ch = wgetch(wpopup);
        if (ch == 27 || ch == '\n') break;
        if (ch == KEY_UP && sel > 0) sel--;
        else if (ch == KEY_DOWN && sel < count - 1) sel++;
        else if (ch == KEY_PPAGE) {
            sel -= list_rows;
            if (sel < 0) sel = 0;
        } else if (ch == KEY_NPAGE) {
            sel += list_rows;
            if (sel >= count) sel = count - 1;
        }
        if (sel < top) top = sel;
        if (sel >= top + list_rows) top = sel - list_rows + 1;
    }

    delwin(wpopup);
    set_status("Shortcuts closed");
}

/* ---------- FIND/REPLACE ---------- */
void find_text() {
    completion_clear();
    char query[256];
    query[0] = '\0';
    popup_input("Find", "Search:", query, sizeof(query));
    if (query[0] == '\0') { set_status("Find canceled"); return; }
    int starty = cy;
    int startx = cx;
    for (int pass = 0; pass < 2; pass++) {
        for (int y = starty; y < lines; y++) {
            const char *hay = buf[y];
            int off = (y == starty) ? startx : 0;
            if (off < 0) off = 0;
            if (off > (int)strlen(hay)) off = (int)strlen(hay);
            char *p = strstr(hay + off, query);
            if (p) {
                cy = y;
                cx = (int)(p - hay);
                set_status("Found: %s", query);
                state_save();
                return;
            }
        }
        starty = 0;
        startx = 0;
    }
    set_status("Not found: %s", query);
}

void replace_text() {
    completion_clear();
    char find[256], replace[256];
    find[0] = '\0';
    replace[0] = '\0';
    popup_input("Replace", "Find:", find, sizeof(find));
    if (find[0] == '\0') { set_status("Replace canceled"); return; }
    popup_input("Replace", "Replace with:", replace, sizeof(replace));
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
    if (changed) {
        is_dirty = 1;
        lsp_send_did_change();
        syntax_recalc_all();
        state_save();
        set_status("Replaced '%s'", find);
    } else {
        set_status("Not found: %s", find);
    }
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
    lsp_send_did_change();
    syntax_recalc_from(cy, 1);
}

static int is_opening_pair(int c, int *closing_out) {
    switch (c) {
        case '(': *closing_out = ')'; return 1;
        case '[': *closing_out = ']'; return 1;
        case '{': *closing_out = '}'; return 1;
        case '<': *closing_out = '>'; return 1;
        case '"': *closing_out = '"'; return 1;
        case '\'': *closing_out = '\''; return 1;
        default: return 0;
    }
}

static int is_closing_pair(int c) {
    return (c == ')' || c == ']' || c == '}' || c == '>' || c == '"' || c == '\'');
}

static int should_auto_pair(int c) {
    if (c == '"' || c == '\'') {
        if (cx > 0 && buf[cy][cx - 1] == '\\') return 0;
    }
    return 1;
}

static int handle_autopair(int c) {
    int closing = 0;
    int len = (int)strlen(buf[cy]);
    if (is_opening_pair(c, &closing) && should_auto_pair(c)) {
        if (len + 2 >= MAX_LINE) return 1;
        memmove(&buf[cy][cx + 2], &buf[cy][cx], len - cx + 1);
        buf[cy][cx] = (char)c;
        buf[cy][cx + 1] = (char)closing;
        cx++;
        is_dirty = 1;
        lsp_send_did_change();
        syntax_recalc_from(cy, 1);
        return 1;
    }
    if (is_closing_pair(c)) {
        if (cx < len && buf[cy][cx] == (char)c) {
            cx++;
            return 1;
        }
    }
    return 0;
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
    if (hl_open_comment) {
        for (int i = lines; i > cy + 1; i--) {
            hl_open_comment[i] = hl_open_comment[i - 1];
        }
        hl_open_comment[cy + 1] = 0;
    }
    int recalc_from = cy > 0 ? (cy - 1) : 0;
    lines++;
    cy++;
    cx = 0;
    is_dirty = 1;
    lsp_send_did_change();
    syntax_recalc_from(recalc_from, 2);
}

static void delete_char(void) {
    if (cx > 0) {
        memmove(&buf[cy][cx - 1], &buf[cy][cx], strlen(buf[cy]) - cx + 1);
        cx--;
        is_dirty = 1;
        lsp_send_did_change();
        syntax_recalc_from(cy, 1);
    } else if (cy > 0) {
        int prev_len = (int)strlen(buf[cy - 1]);
        int cur_len = (int)strlen(buf[cy]);
        if (prev_len + cur_len < MAX_LINE - 1) {
            strcat(buf[cy - 1], buf[cy]);
            free(buf[cy]);
            for (int i = cy; i < lines - 1; i++) buf[i] = buf[i + 1];
            buf[lines - 1] = NULL;
            if (hl_open_comment) {
                for (int i = cy; i < lines - 1; i++) hl_open_comment[i] = hl_open_comment[i + 1];
                hl_open_comment[lines - 1] = 0;
            }
            lines--;
            cy--;
            cx = prev_len;
            is_dirty = 1;
            lsp_send_did_change();
            int recalc_from = cy > 0 ? (cy - 1) : 0;
            syntax_recalc_from(recalc_from, 2);
        }
    }
}

static void delete_forward(void) {
    int len = (int)strlen(buf[cy]);
    if (cx < len) {
        memmove(&buf[cy][cx], &buf[cy][cx + 1], len - cx);
        is_dirty = 1;
        lsp_send_did_change();
        syntax_recalc_from(cy, 1);
    } else if (cy < lines - 1) {
        int cur_len = (int)strlen(buf[cy]);
        int next_len = (int)strlen(buf[cy + 1]);
        if (cur_len + next_len < MAX_LINE - 1) {
            strcat(buf[cy], buf[cy + 1]);
            free(buf[cy + 1]);
            for (int i = cy + 1; i < lines - 1; i++) buf[i] = buf[i + 1];
            buf[lines - 1] = NULL;
            if (hl_open_comment) {
                for (int i = cy + 1; i < lines - 1; i++) hl_open_comment[i] = hl_open_comment[i + 1];
                hl_open_comment[lines - 1] = 0;
            }
            lines--;
            is_dirty = 1;
            lsp_send_did_change();
            int recalc_from = cy > 0 ? (cy - 1) : 0;
            syntax_recalc_from(recalc_from, 2);
        }
    }
}

static void delete_line(int y) {
    if (lines <= 1) {
        buf[0][0] = '\0';
        cx = 0;
        cy = 0;
        is_dirty = 1;
        lsp_send_did_change();
        if (hl_open_comment) hl_open_comment[0] = 0;
        return;
    }
    free(buf[y]);
    for (int i = y; i < lines - 1; i++) buf[i] = buf[i + 1];
    buf[lines - 1] = NULL;
    if (hl_open_comment) {
        for (int i = y; i < lines - 1; i++) hl_open_comment[i] = hl_open_comment[i + 1];
        hl_open_comment[lines - 1] = 0;
    }
    lines--;
    if (cy >= lines) cy = lines - 1;
    if (cx > (int)strlen(buf[cy])) cx = (int)strlen(buf[cy]);
    is_dirty = 1;
    lsp_send_did_change();
    int recalc_from = y > 0 ? (y - 1) : 0;
    syntax_recalc_from(recalc_from, 2);
}

static void new_file_prompt(void) {
    char fname[256];
    popup_input("New File", "Enter file name (with extension):", fname, sizeof(fname));
    if (fname[0] == '\0') return;
    /* Ensure the file exists on disk */
    FILE *fp = fopen(fname, "a");
    if (fp) fclose(fp);
    tab_create_with_file(fname);
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
    state_save();
    set_status("Opened folder: %s", path);
}

static int parse_hex_color(const char *hex, int *r, int *g, int *b) {
    if (!hex || hex[0] != '#') return 0;
    if (!isxdigit((unsigned char)hex[1]) || !isxdigit((unsigned char)hex[2]) ||
        !isxdigit((unsigned char)hex[3]) || !isxdigit((unsigned char)hex[4]) ||
        !isxdigit((unsigned char)hex[5]) || !isxdigit((unsigned char)hex[6])) {
        return 0;
    }
    char buf[3] = {0, 0, 0};
    buf[0] = hex[1]; buf[1] = hex[2];
    *r = (int)strtol(buf, NULL, 16);
    buf[0] = hex[3]; buf[1] = hex[4];
    *g = (int)strtol(buf, NULL, 16);
    buf[0] = hex[5]; buf[1] = hex[6];
    *b = (int)strtol(buf, NULL, 16);
    return 1;
}

static int line_has_key(const char *line, const char *key) {
    const char *p = strstr(line, key);
    if (!p) return 0;
    if (p != line) {
        unsigned char prev = (unsigned char)p[-1];
        if (isalnum(prev) || prev == '_') return 0;
    }
    unsigned char next = (unsigned char)p[strlen(key)];
    if (isalnum(next) || next == '_') return 0;
    return 1;
}

static void parse_theme_line(const char *line, const char *key, int *has, int *r, int *g, int *b) {
    if (!line_has_key(line, key)) return;
    const char *hex = strchr(line, '#');
    if (!hex) return;
    if (parse_hex_color(hex, r, g, b)) *has = 1;
}

static int parse_theme_file(const char *path, ThemeParsed *out) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    memset(out, 0, sizeof(*out));
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        parse_theme_line(line, "background", &out->has_background, &out->bg_r, &out->bg_g, &out->bg_b);
        parse_theme_line(line, "menu", &out->has_menu, &out->menu_r, &out->menu_g, &out->menu_b);
        parse_theme_line(line, "sidebar", &out->has_sidebar, &out->sidebar_r, &out->sidebar_g, &out->sidebar_b);
        parse_theme_line(line, "status", &out->has_status, &out->status_r, &out->status_g, &out->status_b);
        parse_theme_line(line, "editorBackground", &out->has_editor_bg, &out->editor_bg_r, &out->editor_bg_g, &out->editor_bg_b);
        parse_theme_line(line, "editorText", &out->has_editor_text, &out->editor_text_r, &out->editor_text_g, &out->editor_text_b);
        parse_theme_line(line, "keyword", &out->has_keyword, &out->keyword_r, &out->keyword_g, &out->keyword_b);
        parse_theme_line(line, "lineNumbers", &out->has_line_numbers, &out->line_r, &out->line_g, &out->line_b);
        parse_theme_line(line, "accent", &out->has_accent, &out->accent_r, &out->accent_g, &out->accent_b);
    }
    fclose(fp);
    return out->has_background || out->has_menu || out->has_sidebar || out->has_status ||
           out->has_editor_bg || out->has_editor_text || out->has_keyword || out->has_line_numbers ||
           out->has_accent;
}

static int rgb_to_ansi8(int r, int g, int b) {
    static const int palette[8][3] = {
        {0, 0, 0},
        {205, 49, 49},
        {13, 188, 121},
        {229, 229, 16},
        {36, 114, 200},
        {188, 63, 188},
        {17, 168, 205},
        {229, 229, 229}
    };
    int best = 0;
    long best_dist = 1L << 30;
    for (int i = 0; i < 8; i++) {
        long dr = r - palette[i][0];
        long dg = g - palette[i][1];
        long db = b - palette[i][2];
        long dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) { best_dist = dist; best = i; }
    }
    return best;
}

static int rgb_to_ansi256(int r, int g, int b) {
    if (r == g && g == b) {
        if (r < 8) return 16;
        if (r > 248) return 231;
        return 232 + (r - 8) / 10;
    }
    int rc = (r * 5) / 255;
    int gc = (g * 5) / 255;
    int bc = (b * 5) / 255;
    return 16 + (36 * rc) + (6 * gc) + bc;
}

static int rgb_to_color_index(int r, int g, int b) {
    if (COLORS >= 256) return rgb_to_ansi256(r, g, b);
    return rgb_to_ansi8(r, g, b);
}

static int theme_color_from_rgb(int r, int g, int b) {
    if (can_change_color() && COLORS >= 16) {
        if (theme_next_color < COLORS) {
            int idx = theme_next_color++;
            int rr = (r * 1000) / 255;
            int gg = (g * 1000) / 255;
            int bb = (b * 1000) / 255;
            if (init_color(idx, rr, gg, bb) == OK) {
                return idx;
            }
        }
    }
    return rgb_to_color_index(r, g, b);
}

static void apply_theme_pairs(void) {
    init_pair(1, theme_menu_fg, theme_menu_bg);
    init_pair(2, theme_sidebar_fg, theme_sidebar_bg);
    init_pair(3, theme_editor_fg, theme_editor_bg);
    init_pair(4, theme_keyword_fg, theme_editor_bg);
    init_pair(5, theme_editor_fg, theme_editor_bg);
    init_pair(6, theme_comment_fg, theme_editor_bg);
    init_pair(7, theme_string_fg, theme_editor_bg);
    init_pair(8, theme_number_fg, theme_editor_bg);
    init_pair(9, theme_preproc_fg, theme_editor_bg);
    init_pair(10, theme_status_fg, theme_status_bg);
}

static void reset_theme_defaults(void) {
    theme_menu_bg = default_theme_menu_bg;
    theme_menu_fg = default_theme_menu_fg;
    theme_sidebar_bg = default_theme_sidebar_bg;
    theme_sidebar_fg = default_theme_sidebar_fg;
    theme_editor_bg = default_theme_editor_bg;
    theme_editor_fg = default_theme_editor_fg;
    theme_keyword_fg = default_theme_keyword_fg;
    theme_comment_fg = default_theme_comment_fg;
    theme_string_fg = default_theme_string_fg;
    theme_number_fg = default_theme_number_fg;
    theme_preproc_fg = default_theme_preproc_fg;
    theme_status_bg = default_theme_status_bg;
    theme_status_fg = default_theme_status_fg;
    apply_theme_pairs();
}

static int apply_theme_from_file(const char *path) {
    ThemeParsed t;
    if (!parse_theme_file(path, &t)) return 0;
    theme_next_color = 16;
    if (t.has_menu) {
        theme_menu_bg = theme_color_from_rgb(t.menu_r, t.menu_g, t.menu_b);
    }
    if (t.has_sidebar) {
        theme_sidebar_bg = theme_color_from_rgb(t.sidebar_r, t.sidebar_g, t.sidebar_b);
    }
    if (t.has_status) {
        theme_status_bg = theme_color_from_rgb(t.status_r, t.status_g, t.status_b);
    }
    if (t.has_editor_bg) {
        theme_editor_bg = theme_color_from_rgb(t.editor_bg_r, t.editor_bg_g, t.editor_bg_b);
    } else if (t.has_background) {
        theme_editor_bg = theme_color_from_rgb(t.bg_r, t.bg_g, t.bg_b);
    }
    if (t.has_editor_text) {
        int fg = theme_color_from_rgb(t.editor_text_r, t.editor_text_g, t.editor_text_b);
        theme_editor_fg = fg;
        theme_sidebar_fg = fg;
        theme_status_fg = fg;
        theme_menu_fg = fg;
    }
    if (t.has_keyword) {
        theme_keyword_fg = theme_color_from_rgb(t.keyword_r, t.keyword_g, t.keyword_b);
        theme_preproc_fg = theme_keyword_fg;
    }
    if (t.has_line_numbers) {
        theme_comment_fg = theme_color_from_rgb(t.line_r, t.line_g, t.line_b);
    }
    if (t.has_accent) {
        int acc = theme_color_from_rgb(t.accent_r, t.accent_g, t.accent_b);
        theme_string_fg = acc;
        theme_number_fg = acc;
    }
    apply_theme_pairs();
    return 1;
}

static void import_theme_prompt(void) {
    char path[PATH_MAX] = "";
    popup_input("Import Theme", "Theme file path (e.g. /home/user/downloads/theme.tasci):", path, sizeof(path));
    if (path[0] == '\0') return;
    if (access(path, R_OK) != 0) {
        set_status("Error: Cannot read theme file %s", path);
        return;
    }
    if (apply_theme_from_file(path)) {
        strncpy(current_theme_path, path, sizeof(current_theme_path) - 1);
        current_theme_path[sizeof(current_theme_path) - 1] = '\0';
        state_save();
        set_status("Theme imported: %s", current_theme_path);
        draw_menu();
        draw_tabs();
        draw_sidebar();
        draw_editor();
        draw_status(status_msg);
    } else {
        set_status("Error: Theme file missing colors");
    }
}

static void theme_menu_prompt(void) {
    const char *items[] = { "Reset to Default", "Import Theme" };
    int sel = popup_select("Theme", items, 2);
    if (sel == 0) {
        reset_theme_defaults();
        current_theme_path[0] = '\0';
        state_save();
        set_status("Theme reset to default");
        draw_menu();
        draw_tabs();
        draw_sidebar();
        draw_editor();
        draw_status(status_msg);
    } else if (sel == 1) {
        import_theme_prompt();
    }
}

static int try_spawn_terminal(const char *cmd) {
    char buf[512];
    if (!cmd || !cmd[0]) return 0;
    snprintf(buf, sizeof(buf), "%s >/dev/null 2>&1 &", cmd);
    int rc = system(buf);
    return rc == 0;
}

static void open_external_terminal(void) {
    const char *env_term = getenv("TERMINAL");
    if (env_term && env_term[0]) {
        if (try_spawn_terminal(env_term)) {
            set_status("Opened terminal: %s", env_term);
            return;
        }
    }
    const char *fallbacks[] = {
        "x-terminal-emulator",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "kitty",
        "alacritty",
        "wezterm",
        "xterm",
        "lxterminal",
        "mate-terminal",
        "tilix",
        NULL
    };
    for (int i = 0; fallbacks[i]; i++) {
        if (try_spawn_terminal(fallbacks[i])) {
            set_status("Opened terminal: %s", fallbacks[i]);
            return;
        }
    }
    set_status("Terminal not found. Set $TERMINAL.");
}

static void special_chars_prompt(void) {
    completion_clear();
    static const char specials[] =
        "!@#$%^&*()"
        "[]{}<>"
        "-_=+"
        "\\|"
        ";:'\""
        ",./?"
        "`~";

    int count = (int)strlen(specials);
    if (count <= 0) return;

    int cell_w = 3;
    int cols = 10;
    int max_cols = (COLS - 6) / cell_w;
    if (max_cols < 4) max_cols = 4;
    if (cols > max_cols) cols = max_cols;
    int rows = (count + cols - 1) / cols;

    int h = rows + 6;
    int w = cols * cell_w + 4;
    if (h > LINES - 2) h = LINES - 2;
    if (w > COLS - 2) w = COLS - 2;
    if (h < 8) h = 8;
    if (w < 20) w = 20;
    int sy = (LINES - h) / 2;
    int sx = (COLS - w) / 2;

    WINDOW *wpopup = newwin(h, w, sy, sx);
    keypad(wpopup, TRUE);
    wtimeout(wpopup, -1);

    int sel = 0;
    while (1) {
        werase(wpopup);
        box(wpopup, 0, 0);
        mvwprintw(wpopup, 1, 2, "Special Characters");
        mvwprintw(wpopup, 2, 2, "Arrows=move  Enter=insert  Esc=close");

        int start_y = 4;
        for (int i = 0; i < count; i++) {
            int r = i / cols;
            int c = i % cols;
            int y = start_y + r;
            int x = 2 + c * cell_w;
            if (y >= h - 1) break;
            if (x + cell_w >= w - 1) continue;
            if (i == sel) wattron(wpopup, A_REVERSE);
            mvwprintw(wpopup, y, x, " %c ", specials[i]);
            if (i == sel) wattroff(wpopup, A_REVERSE);
        }

        wrefresh(wpopup);
        int ch = wgetch(wpopup);
        if (ch == 27) { /* ESC */
            delwin(wpopup);
            set_status("Special chars canceled");
            return;
        }
        if (ch == '\n') {
            char c = specials[sel];
            delwin(wpopup);
            insert_char((unsigned char)c);
            set_status("Inserted: %c", c);
            return;
        }
        if (ch == KEY_LEFT && sel > 0) sel--;
        else if (ch == KEY_RIGHT && sel + 1 < count) sel++;
        else if (ch == KEY_UP && sel - cols >= 0) sel -= cols;
        else if (ch == KEY_DOWN && sel + cols < count) sel += cols;
    }
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

static const char *tab_display_name(const Tab *t, char *out, size_t out_sz, int idx) {
    if (t->path[0]) {
        const char *base = strrchr(t->path, '/');
        base = base ? base + 1 : t->path;
        snprintf(out, out_sz, "%s", base);
    } else {
        snprintf(out, out_sz, "Untitled %d", idx + 1);
    }
    return out;
}

void draw_tabs() {
    if (!tabw) return;
    tab_store_current();
    werase(tabw);
    wbkgd(tabw, COLOR_PAIR(1));
    int x = 1;
    int w = getmaxx(tabw);
    for (int i = 0; i < tab_count; i++) {
        char name[PATH_MAX];
        const char *label = tab_display_name(&tabs[i], name, sizeof(name), i);
        char title[PATH_MAX + 8];
        snprintf(title, sizeof(title), " %s%s x ", label, tabs[i].is_dirty ? "*" : "");
        int len = (int)strlen(title);
        if (x + len >= w - 1) break;
        if (mode == MODE_TABS && i == tab_sel) {
            wattron(tabw, A_REVERSE | A_BOLD);
        } else if (i == tab_current) {
            wattron(tabw, A_BOLD);
        }
        mvwprintw(tabw, 0, x, "%s", title);
        if (mode == MODE_TABS && i == tab_sel) {
            wattroff(tabw, A_REVERSE | A_BOLD);
        } else if (i == tab_current) {
            wattroff(tabw, A_BOLD);
        }
        x += len + 1;
    }
    wrefresh(tabw);
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

static int is_binary_data(const unsigned char *buf, size_t n) {
    if (n == 0) return 0;
    size_t bad = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = buf[i];
        if (c == '\n' || c == '\r' || c == '\t') continue;
        if (c < 32 || c > 126) bad++;
    }
    return (bad * 5 > n); /* >20% non-printable */
}

static void draw_preview_for_selected(void) {
    werase(mainw);
    wbkgd(mainw, COLOR_PAIR(3));
    box(mainw, 0, 0);
    int h, w;
    getmaxyx(mainw, h, w);
    if (file_count <= 0) {
        mvwprintw(mainw, 1, 2, "No files");
        wrefresh(mainw);
        return;
    }

    const char *name = files[sel];
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", name);
    mvwprintw(mainw, 1, 2, "Preview: %s", path);

    if (is_dir(name)) {
        DIR *d = opendir(name);
        if (!d) {
            mvwprintw(mainw, 3, 2, "Cannot open directory");
            wrefresh(mainw);
            return;
        }
        int y = 3;
        int col_w = (w - 4) / 2;
        int x = 2;
        struct dirent *e;
        while ((e = readdir(d)) && y < h - 1) {
            char entry[NAME_MAX + 4];
            snprintf(entry, sizeof(entry), "%s%s", e->d_name, is_dir(e->d_name) ? "/" : "");
            mvwprintw(mainw, y, x, "%.*s", col_w - 1, entry);
            if (x > 2) { x = 2; y++; }
            else x += col_w;
        }
        closedir(d);
        wrefresh(mainw);
        return;
    }

    FILE *fp = fopen(name, "r");
    if (!fp) {
        mvwprintw(mainw, 3, 2, "Cannot open file");
        wrefresh(mainw);
        return;
    }

    unsigned char probe[1024];
    size_t n = fread(probe, 1, sizeof(probe), fp);
    int binary = is_binary_data(probe, n);
    fseek(fp, 0, SEEK_SET);

    if (binary) {
        mvwprintw(mainw, 3, 2, "Binary file");
        mvwprintw(mainw, 4, 2, "Bytes:");
        int y = 5;
        int x = 2;
        for (size_t i = 0; i < n && y < h - 1; i++) {
            mvwprintw(mainw, y, x, "%02x ", probe[i]);
            x += 3;
            if (x > w - 4) { x = 2; y++; }
        }
        fclose(fp);
        wrefresh(mainw);
        return;
    }

    const SyntaxLang *lang = sh_lang_for_file(name);
    const char *lc = (lang && lang->line_comment && lang->line_comment[0]) ? lang->line_comment : NULL;
    const char *bcs = (lang && lang->block_comment_start && lang->block_comment_start[0]) ? lang->block_comment_start : NULL;
    const char *bce = (lang && lang->block_comment_end && lang->block_comment_end[0]) ? lang->block_comment_end : NULL;
    int lc_len = lc ? (int)strlen(lc) : 0;
    int bcs_len = bcs ? (int)strlen(bcs) : 0;
    int bce_len = bce ? (int)strlen(bce) : 0;
    int in_block_comment = 0;

    char line[MAX_LINE];
    int y = 3;
    while (fgets(line, sizeof(line), fp) && y < h - 1) {
        line[strcspn(line, "\n")] = 0;
        int avail = w - 4;
        if (avail < 0) avail = 0;
        int i = 0;
        int col = 0;
        char in_string = 0;

        int preproc_start = -1;
        if (lang_is_c_preproc(lang)) {
            int j = 0;
            while (line[j] == ' ' || line[j] == '\t') j++;
            if (line[j] == '#') preproc_start = j;
        }

        while (line[i] && col < avail) {
            if (!in_block_comment && !in_string && preproc_start >= 0 && i >= preproc_start) {
                wattron(mainw, COLOR_PAIR(9) | A_BOLD);
                mvwaddnstr(mainw, y, 2 + col, &line[i], avail - col);
                wattroff(mainw, COLOR_PAIR(9) | A_BOLD);
                break;
            }
            if (line[i] == '\t') {
                mvwaddch(mainw, y, 2 + col, ' ');
                i++; col++;
                continue;
            }

            if (in_block_comment) {
                if (bce_len && strncmp(&line[i], bce, (size_t)bce_len) == 0) {
                    int draw = bce_len;
                    if (draw > (avail - col)) draw = avail - col;
                    wattron(mainw, COLOR_PAIR(6) | A_DIM);
                    mvwaddnstr(mainw, y, 2 + col, &line[i], draw);
                    wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                    i += draw; col += draw;
                    if (draw == bce_len) in_block_comment = 0;
                    continue;
                }
                wattron(mainw, COLOR_PAIR(6) | A_DIM);
                mvwaddch(mainw, y, 2 + col, line[i]);
                wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                i++; col++;
                continue;
            }

            if (in_string) {
                wattron(mainw, COLOR_PAIR(7));
                mvwaddch(mainw, y, 2 + col, line[i]);
                wattroff(mainw, COLOR_PAIR(7));
                if (line[i] == '\\' && line[i + 1]) {
                    i++; col++;
                    if (col < avail) {
                        wattron(mainw, COLOR_PAIR(7));
                        mvwaddch(mainw, y, 2 + col, line[i]);
                        wattroff(mainw, COLOR_PAIR(7));
                        i++; col++;
                    }
                    continue;
                }
                if (line[i] == in_string) in_string = 0;
                i++; col++;
                continue;
            }

            if (lc_len && strncmp(&line[i], lc, (size_t)lc_len) == 0) {
                wattron(mainw, COLOR_PAIR(6) | A_DIM);
                mvwaddnstr(mainw, y, 2 + col, &line[i], avail - col);
                wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                break;
            }

            if (bcs_len && strncmp(&line[i], bcs, (size_t)bcs_len) == 0) {
                int draw = bcs_len;
                if (draw > (avail - col)) draw = avail - col;
                wattron(mainw, COLOR_PAIR(6) | A_DIM);
                mvwaddnstr(mainw, y, 2 + col, &line[i], draw);
                wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                i += draw; col += draw;
                if (draw == bcs_len) in_block_comment = 1;
                continue;
            }

            if (lang_has_string_delim(lang, line[i])) {
                in_string = line[i];
                wattron(mainw, COLOR_PAIR(7));
                mvwaddch(mainw, y, 2 + col, line[i]);
                wattroff(mainw, COLOR_PAIR(7));
                i++; col++;
                continue;
            }

            if (isdigit((unsigned char)line[i]) ||
                (line[i] == '.' && isdigit((unsigned char)line[i + 1]))) {
                int nstart = i;
                int nlen = 0;
                while (line[i] &&
                       (isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == '_' ||
                        line[i] == '+' || line[i] == '-')) {
                    i++;
                    nlen++;
                }
                int draw = nlen;
                if (draw > (avail - col)) draw = avail - col;
                wattron(mainw, COLOR_PAIR(8));
                mvwaddnstr(mainw, y, 2 + col, &line[nstart], draw);
                wattroff(mainw, COLOR_PAIR(8));
                col += draw;
                if (draw < nlen) break;
                continue;
            }

            if (isalpha((unsigned char)line[i]) || line[i] == '_') {
                int wstart = i;
                int wlen = 0;
                while (line[i] && (isalnum((unsigned char)line[i]) || line[i] == '_')) {
                    i++;
                    wlen++;
                }
                int draw = wlen;
                if (draw > (avail - col)) draw = avail - col;
                if (lang && sh_is_keyword(lang, &line[wstart], wlen)) {
                    wattron(mainw, COLOR_PAIR(4) | A_BOLD);
                    mvwaddnstr(mainw, y, 2 + col, &line[wstart], draw);
                    wattroff(mainw, COLOR_PAIR(4) | A_BOLD);
                } else {
                    mvwaddnstr(mainw, y, 2 + col, &line[wstart], draw);
                }
                col += draw;
                if (draw < wlen) break;
                continue;
            }

            mvwaddch(mainw, y, 2 + col, line[i]);
            i++; col++;
        }
        y++;
    }
    fclose(fp);
    wrefresh(mainw);
}

void draw_editor() {
    if (mode == MODE_EXPLORER) {
        draw_preview_for_selected();
        return;
    }
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
        const char *line = buf[filerow];
        int i = start;
        int col = 0;

        const char *lc = (lang && lang->line_comment && lang->line_comment[0]) ? lang->line_comment : NULL;
        const char *bcs = (lang && lang->block_comment_start && lang->block_comment_start[0]) ? lang->block_comment_start : NULL;
        const char *bce = (lang && lang->block_comment_end && lang->block_comment_end[0]) ? lang->block_comment_end : NULL;
        int lc_len = lc ? (int)strlen(lc) : 0;
        int bcs_len = bcs ? (int)strlen(bcs) : 0;
        int bce_len = bce ? (int)strlen(bce) : 0;

        int in_line_comment = 0;
        int in_block_comment = 0;
        char in_string = 0;
        if (lang && bcs_len && bce_len && hl_open_comment && filerow > 0) {
            in_block_comment = hl_open_comment[filerow - 1] ? 1 : 0;
        }

        int preproc_start = -1;
        if (lang_is_c_preproc(lang)) {
            int j = 0;
            while (line[j] == ' ' || line[j] == '\t') j++;
            if (line[j] == '#') preproc_start = j;
        }

        /* If horizontally scrolled, advance syntax state up to `start` so colors stay correct. */
        int scan_i = 0;
        int scan_block = in_block_comment;
        char scan_str = 0;
        while (line[scan_i] && scan_i < start) {
            if (scan_str) {
                if (line[scan_i] == '\\' && line[scan_i + 1]) { scan_i += 2; continue; }
                if (line[scan_i] == scan_str) { scan_str = 0; scan_i++; continue; }
                scan_i++;
                continue;
            }
            if (scan_block) {
                if (bce_len && strncmp(&line[scan_i], bce, (size_t)bce_len) == 0) {
                    scan_block = 0;
                    scan_i += bce_len;
                    continue;
                }
                scan_i++;
                continue;
            }
            if (lc_len && strncmp(&line[scan_i], lc, (size_t)lc_len) == 0) {
                in_line_comment = 1;
                break;
            }
            if (bcs_len && strncmp(&line[scan_i], bcs, (size_t)bcs_len) == 0) {
                scan_block = 1;
                scan_i += bcs_len;
                continue;
            }
            if (lang_has_string_delim(lang, line[scan_i])) {
                scan_str = line[scan_i];
                scan_i++;
                continue;
            }
            scan_i++;
        }
        in_block_comment = scan_block;
        in_string = scan_str;

        if (in_line_comment) {
            wattron(mainw, COLOR_PAIR(6) | A_DIM);
            mvwaddnstr(mainw, y+1, x, &line[i], avail);
            wattroff(mainw, COLOR_PAIR(6) | A_DIM);
        } else {
            while (line[i] && col < avail) {
                if (!in_block_comment && !in_string && preproc_start >= 0 && i >= preproc_start) {
                    wattron(mainw, COLOR_PAIR(9) | A_BOLD);
                    mvwaddnstr(mainw, y+1, x + col, &line[i], avail - col);
                    wattroff(mainw, COLOR_PAIR(9) | A_BOLD);
                    break;
                }
                if (line[i] == '\t') {
                    mvwaddch(mainw, y+1, x + col, ' ');
                    i++; col++;
                    continue;
                }

                if (in_block_comment) {
                    if (bce_len && strncmp(&line[i], bce, (size_t)bce_len) == 0) {
                        int draw = bce_len;
                        if (draw > (avail - col)) draw = avail - col;
                        wattron(mainw, COLOR_PAIR(6) | A_DIM);
                        mvwaddnstr(mainw, y+1, x + col, &line[i], draw);
                        wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                        i += draw; col += draw;
                        if (draw == bce_len) in_block_comment = 0;
                        continue;
                    }
                    wattron(mainw, COLOR_PAIR(6) | A_DIM);
                    mvwaddch(mainw, y+1, x + col, line[i]);
                    wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                    i++; col++;
                    continue;
                }

                if (in_string) {
                    wattron(mainw, COLOR_PAIR(7));
                    mvwaddch(mainw, y+1, x + col, line[i]);
                    wattroff(mainw, COLOR_PAIR(7));
                    if (line[i] == '\\' && line[i + 1]) {
                        i++; col++;
                        if (col < avail) {
                            wattron(mainw, COLOR_PAIR(7));
                            mvwaddch(mainw, y+1, x + col, line[i]);
                            wattroff(mainw, COLOR_PAIR(7));
                            i++; col++;
                        }
                        continue;
                    }
                    if (line[i] == in_string) in_string = 0;
                    i++; col++;
                    continue;
                }

                if (lc_len && strncmp(&line[i], lc, (size_t)lc_len) == 0) {
                    wattron(mainw, COLOR_PAIR(6) | A_DIM);
                    mvwaddnstr(mainw, y+1, x + col, &line[i], avail - col);
                    wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                    break;
                }

                if (bcs_len && strncmp(&line[i], bcs, (size_t)bcs_len) == 0) {
                    int draw = bcs_len;
                    if (draw > (avail - col)) draw = avail - col;
                    wattron(mainw, COLOR_PAIR(6) | A_DIM);
                    mvwaddnstr(mainw, y+1, x + col, &line[i], draw);
                    wattroff(mainw, COLOR_PAIR(6) | A_DIM);
                    i += draw; col += draw;
                    if (draw == bcs_len) in_block_comment = 1;
                    continue;
                }

                if (lang_has_string_delim(lang, line[i])) {
                    in_string = line[i];
                    wattron(mainw, COLOR_PAIR(7));
                    mvwaddch(mainw, y+1, x + col, line[i]);
                    wattroff(mainw, COLOR_PAIR(7));
                    i++; col++;
                    continue;
                }

                if (isdigit((unsigned char)line[i]) ||
                    (line[i] == '.' && isdigit((unsigned char)line[i + 1]))) {
                    int nstart = i;
                    int nlen = 0;
                    while (line[i] &&
                           (isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == '_' ||
                            line[i] == '+' || line[i] == '-')) {
                        i++;
                        nlen++;
                    }
                    int draw = nlen;
                    if (draw > (avail - col)) draw = avail - col;
                    wattron(mainw, COLOR_PAIR(8));
                    mvwaddnstr(mainw, y+1, x + col, &line[nstart], draw);
                    wattroff(mainw, COLOR_PAIR(8));
                    col += draw;
                    if (draw < nlen) break;
                    continue;
                }

                if (isalpha((unsigned char)line[i]) || line[i] == '_') {
                    int wstart = i;
                    int wlen = 0;
                    while (line[i] && (isalnum((unsigned char)line[i]) || line[i] == '_')) {
                        i++;
                        wlen++;
                    }
                    int draw = wlen;
                    if (draw > (avail - col)) draw = avail - col;
                    if (lang && sh_is_keyword(lang, &line[wstart], wlen)) {
                        wattron(mainw, COLOR_PAIR(4) | A_BOLD);
                        mvwaddnstr(mainw, y+1, x + col, &line[wstart], draw);
                        wattroff(mainw, COLOR_PAIR(4) | A_BOLD);
                    } else {
                        mvwaddnstr(mainw, y+1, x + col, &line[wstart], draw);
                    }
                    col += draw;
                    if (draw < wlen) break;
                    continue;
                }

                mvwaddch(mainw, y+1, x + col, line[i]);
                i++; col++;
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

    if (completion_active && completion_count > 0 && mode == MODE_EDITOR) {
        int max_items = completion_count;
        if (max_items > 8) max_items = 8;
        int max_label = 0;
        for (int i = 0; i < max_items; i++) {
            int len = (int)strlen(completion_items[i]);
            if (len > max_label) max_label = len;
        }
        int popup_w = max_label + 2;
        if (popup_w > w - 2) popup_w = w - 2;
        int px = screenx;
        if (px + popup_w >= w - 1) px = w - popup_w - 1;
        if (px < 1) px = 1;
        int py = screeny + 1;
        if (py + max_items >= h - 1) py = screeny - max_items;
        if (py < 1) py = 1;
        for (int i = 0; i < max_items; i++) {
            if (i == completion_sel) wattron(mainw, A_REVERSE);
            mvwaddnstr(mainw, py + i, px, completion_items[i], popup_w - 1);
            if (i == completion_sel) wattroff(mainw, A_REVERSE);
        }
    }
    wrefresh(mainw);
}

void draw_status(const char *msg){
    werase(statusw);
    wbkgd(statusw, COLOR_PAIR(10));
    if(show_status_bar) {
        char info[256];
        const char *name = current_file[0] ? current_file : "[No Name]";
        long rss_kb = 0, vsz_kb = 0;
        get_mem_usage_cached(&rss_kb, &vsz_kb);
        char rss_buf[32] = "";
        char vsz_buf[32] = "";
        if (rss_kb > 0) {
            if (rss_kb < 10240) snprintf(rss_buf, sizeof(rss_buf), "RSS %ld KB", rss_kb);
            else snprintf(rss_buf, sizeof(rss_buf), "RSS %.2f MB", (double)rss_kb / 1024.0);
        }
        if (vsz_kb > 0) {
            if (vsz_kb < 10240) snprintf(vsz_buf, sizeof(vsz_buf), "VSZ %ld KB", vsz_kb);
            else snprintf(vsz_buf, sizeof(vsz_buf), "VSZ %.2f MB", (double)vsz_kb / 1024.0);
        }
        if (rss_buf[0] && vsz_buf[0]) {
            snprintf(info, sizeof(info), "%s  Ln %d/%d  Col %d  Lines %d  %s  %s",
                     name, cy + 1, lines, cx + 1, lines, rss_buf, vsz_buf);
        } else if (rss_buf[0]) {
            snprintf(info, sizeof(info), "%s  Ln %d/%d  Col %d  Lines %d  %s",
                     name, cy + 1, lines, cx + 1, lines, rss_buf);
        } else {
            snprintf(info, sizeof(info), "%s  Ln %d/%d  Col %d  Lines %d",
                     name, cy + 1, lines, cx + 1, lines);
        }
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

static int confirm_discard_or_save(void) __attribute__((unused));
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
    /* Don't let a dead LSP server (broken pipe) kill the editor. */
    signal(SIGPIPE, SIG_IGN);
    state_load();
    int opened_cli = 0;
    /* Handle command line arguments like nano: ts filename */
    if (argc >= 2) {
        /* Check if argument looks like a file path */
        if (argv[1][0] != '-' || (argv[1][1] != '\0' && argv[1][1] != '+')) {
            /* Load the file specified on command line */
            load_file(argv[1]);
            mode = MODE_EDITOR;
            opened_cli = 1;
        }
    }
    if (!opened_cli && session_restore_has_cwd) {
        if (chdir(session_restore_cwd) != 0) {
            session_restore_has_cwd = 0;
        }
    }
    initscr(); cbreak(); noecho(); keypad(stdscr,TRUE); curs_set(0);
    disable_flow_control();
    timeout(MAIN_LOOP_TIMEOUT_MS);
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
    /* If using terminal default background, use a bright fg so text is visible. */
    int menu_fg = (menu_bg == -1) ? COLOR_WHITE : COLOR_BLACK;
    int sidebar_fg = COLOR_BLACK;
    int sidebar_bg = COLOR_WHITE;
    int editor_fg = (editor_bg == -1) ? COLOR_WHITE : COLOR_WHITE;
    int keyword_fg = COLOR_CYAN;
    int comment_fg = COLOR_GREEN;
    int string_fg = COLOR_YELLOW;
    int number_fg = COLOR_MAGENTA;
    int preproc_fg = COLOR_BLUE;

    theme_menu_bg = menu_bg;
    theme_menu_fg = menu_fg;
    theme_sidebar_bg = sidebar_bg;
    theme_sidebar_fg = sidebar_fg;
    theme_editor_bg = editor_bg;
    theme_editor_fg = editor_fg;
    theme_keyword_fg = keyword_fg;
    theme_comment_fg = comment_fg;
    theme_string_fg = string_fg;
    theme_number_fg = number_fg;
    theme_preproc_fg = preproc_fg;
    theme_status_bg = menu_bg;
    theme_status_fg = menu_fg;
    apply_theme_pairs();

    default_theme_menu_bg = theme_menu_bg;
    default_theme_menu_fg = theme_menu_fg;
    default_theme_sidebar_bg = theme_sidebar_bg;
    default_theme_sidebar_fg = theme_sidebar_fg;
    default_theme_editor_bg = theme_editor_bg;
    default_theme_editor_fg = theme_editor_fg;
    default_theme_keyword_fg = theme_keyword_fg;
    default_theme_comment_fg = theme_comment_fg;
    default_theme_string_fg = theme_string_fg;
    default_theme_number_fg = theme_number_fg;
    default_theme_preproc_fg = theme_preproc_fg;
    default_theme_status_bg = theme_status_bg;
    default_theme_status_fg = theme_status_fg;

    if (session_restore_has_theme && access(session_restore_theme_path, R_OK) == 0) {
        if (apply_theme_from_file(session_restore_theme_path)) {
            strncpy(current_theme_path, session_restore_theme_path, sizeof(current_theme_path) - 1);
            current_theme_path[sizeof(current_theme_path) - 1] = '\0';
        }
    }

    if(!getcwd(cwd,sizeof(cwd))) cwd[0]='\0';
    load_dir();

    menuw=newwin(1,COLS,0,0);
    tabw=newwin(1,COLS,1,0);
    sidew=newwin(LINES-3,SIDEBAR,2,0);
    mainw=newwin(LINES-3,COLS-SIDEBAR,2,SIDEBAR);
    statusw=newwin(1,COLS,LINES-1,0);
    layout_windows();

    if (!opened_cli && session_restore_has_file) {
        if (access(session_restore_file, F_OK) == 0) {
            load_file(session_restore_file);
            mode = MODE_EDITOR;
            if (session_restore_has_cursor) {
                if (session_restore_cy < 0) session_restore_cy = 0;
                if (session_restore_cy >= lines) session_restore_cy = lines - 1;
                cy = session_restore_cy;
                int maxcx = (int)strlen(buf[cy]);
                if (session_restore_cx < 0) session_restore_cx = 0;
                if (session_restore_cx > maxcx) session_restore_cx = maxcx;
                cx = session_restore_cx;
                state_save();
            }
        } else {
            session_restore_has_file = 0;
        }
    }
    tabs_init_from_current();
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
        lsp_poll();
        editor_scroll();
        explorer_scroll();
        draw_menu(); draw_tabs(); draw_sidebar(); draw_editor(); draw_status(status_msg);

        int ch=getch();
        if (ch == ERR) continue;
        if(ch==KEY_RESIZE) { layout_windows(); continue; }
        if(ch==24){ if (confirm_exit_all()) break; else continue; } // Ctrl+X
        if(ch==19){ save_file(); }
        if(ch==23){ set_status("Word wrap %s", soft_wrap ? "off" : "on"); soft_wrap=!soft_wrap; }
        if(ch==KEY_F(5)){ tab_prev(); continue; }
        if(ch==KEY_F(6)){ tab_next(); continue; }

        /* ---------- EXPLORER ---------- */
        if(mode==MODE_EXPLORER){
            if(ch==KEY_UP && sel==0) { mode=MODE_TABS; tab_sel = tab_current; }
            else if(ch==KEY_UP && sel>0) sel--;
            else if(ch==KEY_DOWN && sel<file_count-1) sel++;
            else if(ch=='\n'){
                if(is_dir(files[sel])){
                    if(chdir(files[sel])==0){ if(!getcwd(cwd,sizeof(cwd))) cwd[0]='\0'; load_dir(); }
                }else{
                    tab_open_file(files[sel]);
                    mode=MODE_EDITOR;
                }
            }else if(ch==KEY_BACKSPACE||ch==127){
                if(chdir("..")==0){ if(!getcwd(cwd,sizeof(cwd))) cwd[0]='\0'; load_dir(); }
            }
            else if(ch==KEY_DC || ch==4){
                delete_selected_file();
            }
        }

        /* ---------- MENU ---------- */
        else if(mode==MODE_MENU){
            if(ch==KEY_LEFT && menu_sel>0) menu_sel--;
            else if(ch==KEY_RIGHT && menu_sel<MENU_ITEMS-1) menu_sel++;
            else if(ch==KEY_DOWN) { mode=MODE_TABS; tab_sel = tab_current; }
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
                                lsp_send_did_change();
                                syntax_recalc_from(cy, 1);
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
                        state_save();
                        popup("View","Theme: Soft Gray (active)\nFont: Use terminal settings");
                        break;
                    case 2: settings_dialog(); break;
                    case 3: find_text(); break;
                    case 4: shortcuts_dialog(); break;
                    case 5: { /* File */
                        const char *file_items[] = { "New", "Save", "Save As" };
                        int sel = popup_select("File", file_items, 3);
                        if (sel == 0) new_file_prompt();
                        else if (sel == 1) save_file();
                        else if (sel == 2) save_file_as();
                        break;
                    }
                    case 6: open_external_terminal(); break;
                    case 7: save_file(); break;
                    case 8: save_file_as(); break;
                    case 9: open_folder_prompt(); break;
                    case 10: theme_menu_prompt(); break;
                    case 11: popup("About","Open-source code editor TASCI\nCode editor made by tasic928"); break;
                }
            }
            else if(ch==27) mode=MODE_EXPLORER;
        }

        /* ---------- TABS ---------- */
        else if(mode==MODE_TABS){
            if(ch==KEY_LEFT && tab_sel>0){ tab_sel--; tab_switch(tab_sel); }
            else if(ch==KEY_RIGHT && tab_sel<tab_count-1){ tab_sel++; tab_switch(tab_sel); }
            else if(ch=='\n'){ tab_switch(tab_sel); mode=MODE_EDITOR; }
            else if(ch==KEY_DOWN){ mode=MODE_EXPLORER; }
            else if(ch==KEY_UP){ mode=MODE_MENU; }
            else if(ch=='x' || ch=='X' || ch==KEY_DC || ch==4){
                tab_close(tab_sel);
                if (tab_sel >= tab_count) tab_sel = tab_count - 1;
            }
            else if(ch==27) mode=MODE_EXPLORER;
        }

        /* ---------- EDITOR ---------- */
        else if(mode==MODE_EDITOR){
            const SyntaxLang *lang = sh_lang_for_file(current_file);
            if (completion_active) {
                if (ch == KEY_UP && completion_sel > 0) { completion_sel--; continue; }
                if (ch == KEY_DOWN && completion_sel < completion_count - 1) { completion_sel++; continue; }
                if (ch == '\n' || ch == '\t') { apply_completion(); continue; }
                if (ch == 27) { completion_clear(); continue; }
            }
            if(ch==27) { completion_clear(); mode=MODE_EXPLORER; }
            else if(ch==KEY_UP && cy>0){ completion_clear(); cy--; if(cx>(int)strlen(buf[cy])) cx=strlen(buf[cy]); }
            else if(ch==KEY_DOWN && cy<lines-1){ completion_clear(); cy++; if(cx>(int)strlen(buf[cy])) cx=strlen(buf[cy]); }
            else if(ch==KEY_LEFT && cx>0){ completion_clear(); cx--; }
            else if(ch==KEY_RIGHT && cx<(int)strlen(buf[cy])){ completion_clear(); cx++; }
            else if(ch==KEY_BACKSPACE||ch==127||ch==8){ completion_clear(); delete_char(); }
            else if(ch==KEY_DC){ completion_clear(); delete_forward(); }
            else if(ch=='\n'){ completion_clear(); insert_newline(); }
            else if(isprint(ch)){
                if (!handle_autopair(ch)) insert_char(ch);
                completion_trigger_with_char(lang, ch);
            }
            else if(ch==0){ completion_trigger_with_char(lang, ' '); }
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
                    lsp_send_did_change();
                    syntax_recalc_from(cy, 1);
                }
            }
            else if(ch==6) find_text();
            else if(ch==18) replace_text(); /* Ctrl+R */
            else if(ch==1){ /* Ctrl+A */
                cx = 0; cy = 0;
            }
        }
    }

    state_save();
    lsp_shutdown();
    endwin();
    restore_flow_control();
    /* Reset cursor color to terminal default (BEL and ST variants). */
    printf("\033]112\007");
    printf("\033]112\033\\");
    fflush(stdout);
    return 0;
}
