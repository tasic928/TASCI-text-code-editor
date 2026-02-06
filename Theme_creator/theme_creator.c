#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[128];
    GdkRGBA background;
    GdkRGBA accent;
    GdkRGBA editor_text;
    GdkRGBA keyword;
    GdkRGBA sidebar;
    GdkRGBA menu;
    GdkRGBA status;
    GdkRGBA cursor;
    char bg_image[512];
} ThemeState;

typedef struct {
    ThemeState state;
    GtkWidget *name_entry;
    GtkWidget *image_button;
    GtkWidget *image_view;
    GtkWidget *preview_root;
    GtkWidget *menu_label;
    GtkWidget *sidebar_label;
    GtkWidget *status_label;
    GtkWidget *code_label;
    GtkWidget *keyword_label;
    GtkCssProvider *provider;
} App;

static void rgba_from_hex(GdkRGBA *out, const char *hex) {
    gdk_rgba_parse(out, hex);
}

static void rgba_to_hex(const GdkRGBA *c, char *out, size_t out_sz) {
    int r = (int)(c->red * 255.0);
    int g = (int)(c->green * 255.0);
    int b = (int)(c->blue * 255.0);
    snprintf(out, out_sz, "#%02x%02x%02x", r, g, b);
}

static gchar *escape_ts(const char *s) {
    GString *g = g_string_new("");
    for (const char *p = s; p && *p; p++) {
        if (*p == '\\' || *p == '"') g_string_append_c(g, '\\');
        g_string_append_c(g, *p);
    }
    return g_string_free(g, FALSE);
}

static void update_css(App *app) {
    char bg[16], accent[16], editor[16], keyword[16], sidebar[16], menu[16], status[16];
    rgba_to_hex(&app->state.background, bg, sizeof(bg));
    rgba_to_hex(&app->state.accent, accent, sizeof(accent));
    rgba_to_hex(&app->state.editor_text, editor, sizeof(editor));
    rgba_to_hex(&app->state.keyword, keyword, sizeof(keyword));
    rgba_to_hex(&app->state.sidebar, sidebar, sizeof(sidebar));
    rgba_to_hex(&app->state.menu, menu, sizeof(menu));
    rgba_to_hex(&app->state.status, status, sizeof(status));

    char css[2048];
    snprintf(css, sizeof(css),
        ".app-window { background: #0b0f16; color: #e6e7eb; }\n"
        ".panel { background: #0d1117; border-right: 1px solid #1f2937; }\n"
        ".panel-title { font-size: 18px; font-weight: 700; }\n"
        ".section-title { margin-top: 10px; font-size: 12px; letter-spacing: 1px; color: #9aa4b2; }\n"
        ".muted { color: #9aa4b2; }\n"
        ".preview-root { background: %s; }\n"
        ".preview-menu { background: %s; color: %s; padding: 8px 12px; }\n"
        ".preview-sidebar { background: %s; color: %s; padding: 10px; }\n"
        ".preview-status { background: %s; color: #9aa4b2; padding: 6px 12px; }\n"
        ".preview-code { color: %s; font-family: monospace; }\n"
        ".preview-keyword { color: %s; font-weight: 600; }\n"
        ".accent { color: %s; }\n",
        bg, menu, editor, sidebar, editor, status, editor, editor, keyword, accent
    );

    if (app->provider) g_object_unref(app->provider);
    app->provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(app->provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(app->provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

static void update_image(App *app) {
    if (app->state.bg_image[0]) {
        gtk_image_set_from_file(GTK_IMAGE(app->image_view), app->state.bg_image);
    } else {
        gtk_image_clear(GTK_IMAGE(app->image_view));
    }
}

static void update_preview_text(App *app) {
    if (!app->code_label) return;
    char editor[16];
    rgba_to_hex(&app->state.editor_text, editor, sizeof(editor));
    const char *name = app->state.name[0] ? app->state.name : "Untitled Theme";
    gchar *text = g_strdup_printf(
        "1  export const theme = {\\n"
        "2    name: \\\"%s\\\",\\n"
        "3    colors: {\\n"
        "4      editorText: \\\"%s\\\"\\n"
        "5    }\\n"
        "6  }",
        name,
        editor
    );
    gtk_label_set_text(GTK_LABEL(app->code_label), text);
    g_free(text);
}

static void sync_state(App *app) {
    const char *name = gtk_entry_get_text(GTK_ENTRY(app->name_entry));
    strncpy(app->state.name, name && name[0] ? name : "Untitled Theme", sizeof(app->state.name) - 1);
    app->state.name[sizeof(app->state.name) - 1] = '\0';
    update_css(app);
    update_image(app);
    update_preview_text(app);
}

static void on_name_changed(GtkEditable *editable, gpointer user_data) {
    (void)editable;
    App *app = (App *)user_data;
    sync_state(app);
}

static void on_color_changed(GtkColorButton *btn, gpointer user_data) {
    App *app = (App *)user_data;
    const char *name = gtk_buildable_get_name(GTK_BUILDABLE(btn));
    GdkRGBA c;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &c);
    if (g_strcmp0(name, "background") == 0) app->state.background = c;
    else if (g_strcmp0(name, "accent") == 0) app->state.accent = c;
    else if (g_strcmp0(name, "editor_text") == 0) app->state.editor_text = c;
    else if (g_strcmp0(name, "keyword") == 0) app->state.keyword = c;
    else if (g_strcmp0(name, "sidebar") == 0) app->state.sidebar = c;
    else if (g_strcmp0(name, "menu") == 0) app->state.menu = c;
    else if (g_strcmp0(name, "status") == 0) app->state.status = c;
    else if (g_strcmp0(name, "cursor") == 0) app->state.cursor = c;
    sync_state(app);
}

static void on_image_set(GtkFileChooserButton *btn, gpointer user_data) {
    App *app = (App *)user_data;
    char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(btn));
    if (path) {
        strncpy(app->state.bg_image, path, sizeof(app->state.bg_image) - 1);
        app->state.bg_image[sizeof(app->state.bg_image) - 1] = '\0';
        g_free(path);
    } else {
        app->state.bg_image[0] = '\0';
    }
    sync_state(app);
}

static void on_clear_image(GtkButton *btn, gpointer user_data) {
    (void)btn;
    App *app = (App *)user_data;
    app->state.bg_image[0] = '\0';
    gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->image_button));
    sync_state(app);
}

static void export_ts(App *app, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;

    char background[16], accent[16], editor[16], keyword[16], sidebar[16], menu[16], status[16], cursor[16];
    rgba_to_hex(&app->state.background, background, sizeof(background));
    rgba_to_hex(&app->state.accent, accent, sizeof(accent));
    rgba_to_hex(&app->state.editor_text, editor, sizeof(editor));
    rgba_to_hex(&app->state.keyword, keyword, sizeof(keyword));
    rgba_to_hex(&app->state.sidebar, sidebar, sizeof(sidebar));
    rgba_to_hex(&app->state.menu, menu, sizeof(menu));
    rgba_to_hex(&app->state.status, status, sizeof(status));
    rgba_to_hex(&app->state.cursor, cursor, sizeof(cursor));

    gchar *safe_name = escape_ts(app->state.name);
    gchar *safe_img = escape_ts(app->state.bg_image);

    fprintf(fp,
        "export interface TasciTheme {\\n"
        "  name: string;\\n"
        "  colors: {\\n"
        "    background: string;\\n"
        "    accent: string;\\n"
        "    editorText: string;\\n"
        "    keyword: string;\\n"
        "    sidebar: string;\\n"
        "    menu: string;\\n"
        "    status: string;\\n"
        "    cursor: string;\\n"
        "  };\\n"
        "  backgroundImage?: string | null;\\n"
        "}\\n\\n");

    fprintf(fp,
        "export const theme: TasciTheme = {\\n"
        "  name: \"%s\",\\n"
        "  colors: {\\n"
        "    background: \"%s\",\\n"
        "    accent: \"%s\",\\n"
        "    editorText: \"%s\",\\n"
        "    keyword: \"%s\",\\n"
        "    sidebar: \"%s\",\\n"
        "    menu: \"%s\",\\n"
        "    status: \"%s\",\\n"
        "    cursor: \"%s\"\\n"
        "  },\\n"
        "  backgroundImage: ",
        safe_name,
        background,
        accent,
        editor,
        keyword,
        sidebar,
        menu,
        status,
        cursor);

    if (safe_img && safe_img[0]) {
        fprintf(fp, "\"%s\"\\n};\\n", safe_img);
    } else {
        fprintf(fp, "null\\n};\\n");
    }

    fclose(fp);
    g_free(safe_name);
    g_free(safe_img);
}

static void disable_alpha(GtkWidget *btn) {
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(btn), FALSE);
}

static void on_export(GtkButton *btn, gpointer user_data) {
    (void)btn;
    App *app = (App *)user_data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Export Theme",
        NULL,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL
    );
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "tasci-theme.ts");
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        export_ts(app, filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static GtkWidget *label_with_class(const char *text, const char *klass) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    GtkStyleContext *ctx = gtk_widget_get_style_context(lbl);
    gtk_style_context_add_class(ctx, klass);
    return lbl;
}

static GtkWidget *section_title(const char *text) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    GtkStyleContext *ctx = gtk_widget_get_style_context(lbl);
    gtk_style_context_add_class(ctx, "section-title");
    return lbl;
}

static GtkWidget *muted_label(const char *text) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    GtkStyleContext *ctx = gtk_widget_get_style_context(lbl);
    gtk_style_context_add_class(ctx, "muted");
    return lbl;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    App app;
    memset(&app, 0, sizeof(app));

    rgba_from_hex(&app.state.background, "#12141b");
    rgba_from_hex(&app.state.accent, "#2c6bff");
    rgba_from_hex(&app.state.editor_text, "#e6e7eb");
    rgba_from_hex(&app.state.keyword, "#5eead4");
    rgba_from_hex(&app.state.sidebar, "#1b1f2a");
    rgba_from_hex(&app.state.menu, "#0f172a");
    rgba_from_hex(&app.state.status, "#0b1220");
    rgba_from_hex(&app.state.cursor, "#ffffff");
    strncpy(app.state.name, "Midnight Drift", sizeof(app.state.name) - 1);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "TASCI Theme Creator");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 720);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_set_name(window, "app-window");
    GtkStyleContext *win_ctx = gtk_widget_get_style_context(window);
    gtk_style_context_add_class(win_ctx, "app-window");

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), root);

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_size_request(panel, 360, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(panel), "panel");
    gtk_container_set_border_width(GTK_CONTAINER(panel), 16);
    gtk_box_pack_start(GTK_BOX(root), panel, FALSE, FALSE, 0);

    GtkWidget *title = label_with_class("TASCI Theme Creator", "panel-title");
    gtk_box_pack_start(GTK_BOX(panel), title, FALSE, FALSE, 0);
    GtkWidget *subtitle = muted_label("Design a theme and export a .ts file");
    gtk_box_pack_start(GTK_BOX(panel), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), section_title("Basics"), FALSE, FALSE, 6);

    GtkWidget *name_label = muted_label("Theme name");
    gtk_box_pack_start(GTK_BOX(panel), name_label, FALSE, FALSE, 0);
    app.name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app.name_entry), app.state.name);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.name_entry), "e.g. Midnight Drift");
    gtk_box_pack_start(GTK_BOX(panel), app.name_entry, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), section_title("Background"), FALSE, FALSE, 6);

    GtkWidget *bg_color = gtk_color_button_new_with_rgba(&app.state.background);
    gtk_buildable_set_name(GTK_BUILDABLE(bg_color), "background");
    disable_alpha(bg_color);
    gtk_box_pack_start(GTK_BOX(panel), muted_label("Background color"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), bg_color, FALSE, FALSE, 0);

    GtkWidget *accent_color = gtk_color_button_new_with_rgba(&app.state.accent);
    gtk_buildable_set_name(GTK_BUILDABLE(accent_color), "accent");
    disable_alpha(accent_color);
    gtk_box_pack_start(GTK_BOX(panel), muted_label("Accent color"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), accent_color, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), section_title("Editor Colors"), FALSE, FALSE, 6);

    GtkWidget *colors_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(colors_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(colors_grid), 12);
    gtk_box_pack_start(GTK_BOX(panel), colors_grid, FALSE, FALSE, 0);

    GtkWidget *editor_color = gtk_color_button_new_with_rgba(&app.state.editor_text);
    gtk_buildable_set_name(GTK_BUILDABLE(editor_color), "editor_text");
    disable_alpha(editor_color);
    gtk_grid_attach(GTK_GRID(colors_grid), muted_label("Editor text"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(colors_grid), editor_color, 1, 0, 1, 1);

    GtkWidget *keyword_color = gtk_color_button_new_with_rgba(&app.state.keyword);
    gtk_buildable_set_name(GTK_BUILDABLE(keyword_color), "keyword");
    disable_alpha(keyword_color);
    gtk_grid_attach(GTK_GRID(colors_grid), muted_label("Keyword"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(colors_grid), keyword_color, 1, 1, 1, 1);

    GtkWidget *sidebar_color = gtk_color_button_new_with_rgba(&app.state.sidebar);
    gtk_buildable_set_name(GTK_BUILDABLE(sidebar_color), "sidebar");
    disable_alpha(sidebar_color);
    gtk_grid_attach(GTK_GRID(colors_grid), muted_label("Sidebar"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(colors_grid), sidebar_color, 1, 2, 1, 1);

    GtkWidget *menu_color = gtk_color_button_new_with_rgba(&app.state.menu);
    gtk_buildable_set_name(GTK_BUILDABLE(menu_color), "menu");
    disable_alpha(menu_color);
    gtk_grid_attach(GTK_GRID(colors_grid), muted_label("Menu bar"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(colors_grid), menu_color, 1, 3, 1, 1);

    GtkWidget *status_color = gtk_color_button_new_with_rgba(&app.state.status);
    gtk_buildable_set_name(GTK_BUILDABLE(status_color), "status");
    disable_alpha(status_color);
    gtk_grid_attach(GTK_GRID(colors_grid), muted_label("Status bar"), 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(colors_grid), status_color, 1, 4, 1, 1);

    GtkWidget *cursor_color = gtk_color_button_new_with_rgba(&app.state.cursor);
    gtk_buildable_set_name(GTK_BUILDABLE(cursor_color), "cursor");
    disable_alpha(cursor_color);
    gtk_grid_attach(GTK_GRID(colors_grid), muted_label("Cursor"), 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(colors_grid), cursor_color, 1, 5, 1, 1);

    gtk_box_pack_start(GTK_BOX(panel), section_title("Image"), FALSE, FALSE, 6);

    GtkWidget *image_label = muted_label("Background image");
    gtk_box_pack_start(GTK_BOX(panel), image_label, FALSE, FALSE, 0);
    app.image_button = gtk_file_chooser_button_new("Select Image", GTK_FILE_CHOOSER_ACTION_OPEN);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/webp");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app.image_button), filter);
    gtk_box_pack_start(GTK_BOX(panel), app.image_button, FALSE, FALSE, 0);

    GtkWidget *clear_image = gtk_button_new_with_label("Clear image");
    gtk_box_pack_start(GTK_BOX(panel), clear_image, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), section_title("Export"), FALSE, FALSE, 6);
    GtkWidget *export_btn = gtk_button_new_with_label("Export .ts");
    gtk_box_pack_start(GTK_BOX(panel), export_btn, FALSE, FALSE, 4);

    GtkWidget *preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(root), preview_box, TRUE, TRUE, 0);

    GtkWidget *preview = gtk_overlay_new();
    app.preview_root = preview;
    gtk_style_context_add_class(gtk_widget_get_style_context(preview), "preview-root");
    gtk_box_pack_start(GTK_BOX(preview_box), preview, TRUE, TRUE, 0);

    app.image_view = gtk_image_new();
    gtk_overlay_add_overlay(GTK_OVERLAY(preview), app.image_view);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(content, 24);
    gtk_widget_set_margin_bottom(content, 24);
    gtk_widget_set_margin_start(content, 24);
    gtk_widget_set_margin_end(content, 24);
    gtk_overlay_add_overlay(GTK_OVERLAY(preview), content);

    app.menu_label = label_with_class("File  Edit  View  Find  Help", "preview-menu");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.menu_label), "preview-menu");
    gtk_box_pack_start(GTK_BOX(content), app.menu_label, FALSE, FALSE, 0);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(content), body, TRUE, TRUE, 0);

    app.sidebar_label = label_with_class("Explorer\nmain.ts\ntheme.ts\nnotes.md", "preview-sidebar");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.sidebar_label), "preview-sidebar");
    gtk_widget_set_size_request(app.sidebar_label, 180, -1);
    gtk_box_pack_start(GTK_BOX(body), app.sidebar_label, FALSE, FALSE, 0);

    GtkWidget *code_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(code_box, 16);
    gtk_box_pack_start(GTK_BOX(body), code_box, TRUE, TRUE, 0);

    app.code_label = label_with_class("1  export const theme = {\n2    name: \"Midnight Drift\",\n3    colors: {\n4      editorText: \"#e6e7eb\"\n5    }\n6  }", "preview-code");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.code_label), "preview-code");
    gtk_box_pack_start(GTK_BOX(code_box), app.code_label, FALSE, FALSE, 0);

    app.keyword_label = label_with_class("export  const", "preview-keyword");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.keyword_label), "preview-keyword");
    gtk_box_pack_start(GTK_BOX(code_box), app.keyword_label, FALSE, FALSE, 0);

    app.status_label = label_with_class("Ln 3/7  Col 12  Lines 7", "preview-status");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.status_label), "preview-status");
    gtk_box_pack_start(GTK_BOX(content), app.status_label, FALSE, FALSE, 0);

    g_signal_connect(app.name_entry, "changed", G_CALLBACK(on_name_changed), &app);
    g_signal_connect(bg_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(accent_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(editor_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(keyword_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(sidebar_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(menu_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(status_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(cursor_color, "color-set", G_CALLBACK(on_color_changed), &app);
    g_signal_connect(app.image_button, "file-set", G_CALLBACK(on_image_set), &app);
    g_signal_connect(clear_image, "clicked", G_CALLBACK(on_clear_image), &app);
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export), &app);

    sync_state(&app);
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
