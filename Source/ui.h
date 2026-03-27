#pragma once
#ifndef UI_H
#define UI_H

#include "browser.h"
#include "renderer.h"

// ═══════════════════════════════════════════════════════════════
//  ثوابت واجهة المستخدم
// ═══════════════════════════════════════════════════════════════
#define UI_MAX_WIDGETS          512
#define UI_MAX_ANIMATIONS       128
#define UI_MAX_SCROLL_AREAS     32
#define UI_MAX_MENU_ITEMS       64
#define UI_KEYBOARD_ROWS        5
#define UI_KEYBOARD_MAX_KEYS    14
#define UI_MAX_SUGGESTIONS      8
#define UI_ANIM_DURATION_FAST   0.15f
#define UI_ANIM_DURATION_MED    0.25f
#define UI_ANIM_DURATION_SLOW   0.4f
#define UI_SCROLL_BAR_WIDTH     6.0f
#define UI_TAB_MIN_WIDTH        80.0f
#define UI_TAB_MAX_WIDTH        200.0f
#define UI_ICON_SIZE            24.0f
#define UI_CORNER_RADIUS        8.0f
#define UI_HOME_TILE_COLS       4
#define UI_HOME_TILE_ROWS       2
#define UI_HOME_TILE_W          240.0f
#define UI_HOME_TILE_H          140.0f

// ═══════════════════════════════════════════════════════════════
//  أنواع عناصر واجهة المستخدم
// ═══════════════════════════════════════════════════════════════
typedef enum {
    WIDGET_NONE = 0,
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_INPUT,
    WIDGET_CHECKBOX,
    WIDGET_SLIDER,
    WIDGET_DROPDOWN,
    WIDGET_SCROLL_AREA,
    WIDGET_TAB,
    WIDGET_MENU_ITEM,
    WIDGET_ICON_BUTTON,
    WIDGET_TOGGLE,
    WIDGET_PROGRESS_BAR,
    WIDGET_SEPARATOR,
    WIDGET_COLOR_PICKER,
    WIDGET_CARD,
    WIDGET_TILE,
    WIDGET_NOTIFICATION,
    WIDGET_TOOLTIP,
    WIDGET_MODAL,
    WIDGET_COUNT
} WidgetType;

// ═══════════════════════════════════════════════════════════════
//  حالة عنصر واجهة المستخدم
// ═══════════════════════════════════════════════════════════════
typedef enum {
    WIDGET_STATE_NORMAL = 0,
    WIDGET_STATE_HOVERED,
    WIDGET_STATE_PRESSED,
    WIDGET_STATE_FOCUSED,
    WIDGET_STATE_DISABLED,
    WIDGET_STATE_SELECTED,
    WIDGET_STATE_ACTIVE
} WidgetState;

// ═══════════════════════════════════════════════════════════════
//  هيكل الرسوم المتحركة
// ═══════════════════════════════════════════════════════════════
typedef enum {
    ANIM_LINEAR = 0,
    ANIM_EASE_IN,
    ANIM_EASE_OUT,
    ANIM_EASE_IN_OUT,
    ANIM_SPRING,
    ANIM_BOUNCE
} AnimEasing;

typedef struct {
    f32         start_val;
    f32         end_val;
    f32         current_val;
    f32         duration;
    f32         elapsed;
    AnimEasing  easing;
    int         active;
    int         loop;
    int         reverse;
    void        (*on_done)(void* user_data);
    void*       user_data;
} Animation;

// ═══════════════════════════════════════════════════════════════
//  هيكل عنصر واجهة المستخدم
// ═══════════════════════════════════════════════════════════════
typedef struct {
    WidgetType  type;
    WidgetState state;
    f32         x, y, w, h;
    char        label[256];
    char        icon [64];
    Color       color_bg;
    Color       color_fg;
    Color       color_border;
    Color       color_hover;
    Color       color_accent;
    f32         corner_radius;
    f32         border_width;
    f32         opacity;
    f32         glow_size;
    int         visible;
    int         enabled;
    int         focused;
    int         checked;
    f32         value;
    f32         min_val;
    f32         max_val;
    f32         scroll_offset;
    f32         scroll_max;
    int         id;
    Animation   anim_opacity;
    Animation   anim_scale;
    Animation   anim_offset;
    void        (*on_click)(int id, void* data);
    void        (*on_change)(int id, f32 value, void* data);
    void*       user_data;
} Widget;

// ═══════════════════════════════════════════════════════════════
//  هيكل مفتاح لوحة المفاتيح الافتراضية
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    normal[8];
    char    shifted[8];
    f32     width;
    int     is_special;
    int     key_code;
} VKey;

// ═══════════════════════════════════════════════════════════════
//  هيكل لوحة المفاتيح الافتراضية
// ═══════════════════════════════════════════════════════════════
typedef struct {
    VKey    keys    [UI_KEYBOARD_ROWS][UI_KEYBOARD_MAX_KEYS];
    int     key_count[UI_KEYBOARD_ROWS];
    int     selected_row;
    int     selected_col;
    int     shift_active;
    int     caps_lock;
    int     symbols_mode;
    f32     x, y, w, h;
    f32     key_h;
    f32     anim_y;
    int     visible;
} VirtualKeyboard;

// ═══════════════════════════════════════════════════════════════
//  هيكل عنصر القائمة الجانبية
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    label   [128];
    char    icon    [32];
    char    shortcut[16];
    int     selected;
    int     has_submenu;
    int     separator_after;
    Color   accent_color;
    void    (*action)(void* ctx);
} SidebarItem;

// ═══════════════════════════════════════════════════════════════
//  هيكل اقتراح عنوان URL
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    url  [2048];
    char    title[256];
    int     is_history;
    int     is_bookmark;
    int     is_search;
    int     match_score;
} URLSuggestion;

// ═══════════════════════════════════════════════════════════════
//  هيكل إحصائيات الصفحة الرئيسية
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    label[64];
    char    value[64];
    Color   color;
    char    icon [32];
} StatCard;

// ═══════════════════════════════════════════════════════════════
//  هيكل سياق واجهة المستخدم
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // المُصيِّر
    RendererContext* renderer;

    // العناصر
    Widget          widgets   [UI_MAX_WIDGETS];
    int             widget_count;
    int             focused_widget;
    int             hovered_widget;

    // الرسوم المتحركة
    Animation       animations[UI_MAX_ANIMATIONS];
    int             anim_count;

    // لوحة المفاتيح
    VirtualKeyboard keyboard;

    // الاقتراحات
    URLSuggestion   suggestions[UI_MAX_SUGGESTIONS];
    int             suggestion_count;
    int             selected_suggestion;
    int             suggestions_visible;

    // القائمة الجانبية
    SidebarItem     sidebar_items[UI_MAX_MENU_ITEMS];
    int             sidebar_item_count;
    int             sidebar_selected;
    f32             sidebar_scroll;

    // الأبعاد
    u32             screen_w;
    u32             screen_h;
    f32             tab_bar_h;
    f32             url_bar_h;
    f32             status_bar_h;
    f32             content_y;
    f32             content_h;

    // الوقت
    f32             time;
    f32             delta_time;

    // الذاكرة
    void*           memory_pool;

    // حالة مؤشر التحكم
    int             cursor_x;
    int             cursor_y;
    int             cursor_visible;

} UIContext;

// ═══════════════════════════════════════════════════════════════
//  واجهة برمجة واجهة المستخدم
// ═══════════════════════════════════════════════════════════════
#ifdef __cplusplus
extern "C" {
#endif

// تهيئة وإيقاف
UIContext*   ui_init                  (RendererContext* r, void* pool,
                                       u32 w, u32 h);
void         ui_shutdown              (UIContext* ui);
void         ui_update                (UIContext* ui, f32 dt);

// رسم المكونات الأساسية
void         ui_draw_background       (UIContext* ui);
void         ui_draw_splash           (UIContext* ui, f32 alpha);
void         ui_draw_tab_bar          (UIContext* ui, void* tabs,
                                       int tab_count, int active);
void         ui_draw_url_bar          (UIContext* ui, const char* url,
                                       int focused, f32 glow);
void         ui_draw_status_bar       (UIContext* ui, f32 fps,
                                       int online, size_t data_used);
void         ui_draw_loading          (UIContext* ui, f32 progress, f32 angle);
void         ui_draw_loading_bar      (UIContext* ui, f32 progress);
void         ui_draw_page_content     (UIContext* ui, void* tab);
void         ui_draw_home_page        (UIContext* ui, void* history,
                                       int hist_count, void* bookmarks,
                                       int bm_count);
void         ui_draw_sidebar          (UIContext* ui, f32 offset,
                                       void* bookmarks, int bm_count,
                                       void* history,   int hist_count);
void         ui_draw_settings         (UIContext* ui, void* settings);
void         ui_draw_bookmarks        (UIContext* ui, void* bookmarks, int count);
void         ui_draw_history          (UIContext* ui, void* history,   int count);
void         ui_draw_error            (UIContext* ui, const char* msg);
void         ui_draw_notification     (UIContext* ui, const char* text,
                                       f32 alpha, int type);
void         ui_draw_url_keyboard     (UIContext* ui, const char* current_url,
                                       int cursor_pos);
void         ui_draw_url_suggestions  (UIContext* ui);
void         ui_draw_context_menu     (UIContext* ui, f32 x, f32 y,
                                       const char** items, int count);

// رسم مكونات أوبرا جي إكس
void         ui_draw_gx_button        (UIContext* ui, f32 x, f32 y,
                                       f32 w, f32 h, const char* label,
                                       WidgetState state, Color accent);
void         ui_draw_gx_toggle        (UIContext* ui, f32 x, f32 y,
                                       int enabled, Color accent);
void         ui_draw_gx_slider        (UIContext* ui, f32 x, f32 y,
                                       f32 w, f32 val, f32 min, f32 max,
                                       Color accent);
void         ui_draw_gx_card          (UIContext* ui, f32 x, f32 y,
                                       f32 w, f32 h, const char* title,
                                       const char* subtitle, Color accent);
void         ui_draw_gx_progress      (UIContext* ui, f32 x, f32 y,
                                       f32 w, f32 h, f32 progress,
                                       Color accent);
void         ui_draw_gx_icon          (UIContext* ui, f32 x, f32 y,
                                       f32 size, const char* icon_name,
                                       Color color);
void         ui_draw_gx_separator     (UIContext* ui, f32 x, f32 y,
                                       f32 w, Color color);
void         ui_draw_gx_badge         (UIContext* ui, f32 x, f32 y,
                                       const char* text, Color bg, Color fg);
void         ui_draw_gx_speed_dial    (UIContext* ui, void* bookmarks,
                                       int count, f32 x, f32 y,
                                       f32 w, f32 h);
void         ui_draw_gx_neon_line     (UIContext* ui, f32 x1, f32 y1,
                                       f32 x2, f32 y2, Color color,
                                       f32 glow_size);
void         ui_draw_gx_scanlines     (UIContext* ui, f32 x, f32 y,
                                       f32 w, f32 h, f32 alpha);
void         ui_draw_gx_particles     (UIContext* ui, f32 dt);

// الرسوم المتحركة
Animation*   ui_anim_create           (UIContext* ui, f32 from, f32 to,
                                       f32 duration, AnimEasing easing);
void         ui_anim_update           (UIContext* ui, f32 dt);
f32          ui_anim_eval             (Animation* anim);
f32          ui_ease                  (f32 t, AnimEasing easing);

// لوحة المفاتيح
void         vkb_init                 (VirtualKeyboard* kb, f32 screen_w,
                                       f32 screen_h);
void         vkb_show                 (VirtualKeyboard* kb);
void         vkb_hide                 (VirtualKeyboard* kb);
void         vkb_move_cursor          (VirtualKeyboard* kb, int dx, int dy);
const char*  vkb_get_selected_key     (VirtualKeyboard* kb);
void         vkb_toggle_shift         (VirtualKeyboard* kb);
void         vkb_toggle_symbols       (VirtualKeyboard* kb);
void         vkb_draw                 (UIContext* ui, VirtualKeyboard* kb,
                                       const char* input_buf, int cursor_pos);

// الاقتراحات
void         ui_suggest_update        (UIContext* ui, const char* input,
                                       void* history, int hist_count,
                                       void* bookmarks, int bm_count);
void         ui_suggest_clear         (UIContext* ui);
int          ui_suggest_select        (UIContext* ui, int idx);
const char*  ui_suggest_get_url       (UIContext* ui, int idx);

// أدوات
void         ui_format_size           (size_t bytes, char* out, size_t out_len);
void         ui_format_time           (time_t t, char* out, size_t out_len);
void         ui_format_duration       (f32 seconds, char* out, size_t out_len);
void         ui_format_speed          (f32 kbps, char* out, size_t out_len);
Color        ui_get_accent_color      (void);
void         ui_set_accent_color      (Color c);

#ifdef __cplusplus
}
#endif

#endif // UI_H
