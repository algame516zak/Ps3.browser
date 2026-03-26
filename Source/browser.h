#pragma once
#ifndef BROWSER_H
#define BROWSER_H

// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - browser.h
//  المحرك الأساسي للمتصفح
// ═══════════════════════════════════════════════════════════════

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════
//  أنواع البيانات الأساسية
// ═══════════════════════════════════════════════════════════════
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;
typedef float               f32;
typedef double              f64;

// ═══════════════════════════════════════════════════════════════
//  ثوابت HTML و CSS
// ═══════════════════════════════════════════════════════════════
#define HTML_MAX_NODES          4096
#define HTML_MAX_ATTRS          32
#define HTML_MAX_TAG_LEN        64
#define HTML_MAX_TEXT_LEN       8192
#define HTML_MAX_ATTR_LEN       1024
#define HTML_MAX_CLASS_LEN      256
#define HTML_MAX_ID_LEN         128
#define HTML_MAX_HREF_LEN       2048
#define HTML_MAX_SRC_LEN        2048
#define HTML_MAX_STYLE_LEN      2048
#define HTML_MAX_CHILDREN       64
#define HTML_MAX_DEPTH          64

#define CSS_MAX_RULES           2048
#define CSS_MAX_SELECTOR_LEN    512
#define CSS_MAX_PROPERTY_LEN    64
#define CSS_MAX_VALUE_LEN       512
#define CSS_MAX_PROPERTIES      64

#define JS_MAX_VARS             512
#define JS_MAX_FUNCS            256
#define JS_MAX_STACK            128
#define JS_MAX_CODE_LEN         65536
#define JS_MAX_STRING_LEN       4096

#define DOM_MAX_EVENT_LISTENERS 512
#define DOM_MAX_ELEMENTS        4096

// ═══════════════════════════════════════════════════════════════
//  أنواع عُقد HTML
// ═══════════════════════════════════════════════════════════════
typedef enum {
    NODE_UNKNOWN = 0,
    NODE_DOCUMENT,
    NODE_DOCTYPE,
    NODE_HTML,
    NODE_HEAD,
    NODE_BODY,
    NODE_DIV,
    NODE_SPAN,
    NODE_P,
    NODE_A,
    NODE_IMG,
    NODE_H1, NODE_H2, NODE_H3, NODE_H4, NODE_H5, NODE_H6,
    NODE_UL, NODE_OL, NODE_LI,
    NODE_TABLE, NODE_TR, NODE_TD, NODE_TH, NODE_THEAD, NODE_TBODY,
    NODE_FORM, NODE_INPUT, NODE_BUTTON, NODE_SELECT, NODE_OPTION,
    NODE_TEXTAREA, NODE_LABEL,
    NODE_HEADER, NODE_FOOTER, NODE_MAIN, NODE_SECTION, NODE_ARTICLE,
    NODE_NAV, NODE_ASIDE,
    NODE_SCRIPT, NODE_STYLE, NODE_LINK, NODE_META, NODE_TITLE,
    NODE_BR, NODE_HR,
    NODE_STRONG, NODE_EM, NODE_B, NODE_I, NODE_U, NODE_S,
    NODE_CODE, NODE_PRE, NODE_BLOCKQUOTE,
    NODE_VIDEO, NODE_AUDIO, NODE_SOURCE, NODE_CANVAS, NODE_SVG,
    NODE_IFRAME,
    NODE_TEXT,
    NODE_COMMENT,
    NODE_COUNT
} NodeType;

// ═══════════════════════════════════════════════════════════════
//  أنواع CSS
// ═══════════════════════════════════════════════════════════════
typedef enum {
    DISPLAY_NONE = 0,
    DISPLAY_BLOCK,
    DISPLAY_INLINE,
    DISPLAY_INLINE_BLOCK,
    DISPLAY_FLEX,
    DISPLAY_GRID,
    DISPLAY_TABLE,
    DISPLAY_TABLE_ROW,
    DISPLAY_TABLE_CELL
} DisplayType;

typedef enum {
    POSITION_STATIC = 0,
    POSITION_RELATIVE,
    POSITION_ABSOLUTE,
    POSITION_FIXED,
    POSITION_STICKY
} PositionType;

typedef enum {
    OVERFLOW_VISIBLE = 0,
    OVERFLOW_HIDDEN,
    OVERFLOW_SCROLL,
    OVERFLOW_AUTO
} OverflowType;

typedef enum {
    TEXT_ALIGN_LEFT = 0,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT,
    TEXT_ALIGN_JUSTIFY
} TextAlign;

typedef enum {
    FONT_WEIGHT_NORMAL = 400,
    FONT_WEIGHT_BOLD   = 700,
    FONT_WEIGHT_LIGHT  = 300,
    FONT_WEIGHT_BOLDER = 900
} FontWeight;

typedef enum {
    FLEX_DIR_ROW = 0,
    FLEX_DIR_COLUMN,
    FLEX_DIR_ROW_REVERSE,
    FLEX_DIR_COLUMN_REVERSE
} FlexDirection;

typedef enum {
    FLEX_WRAP_NOWRAP = 0,
    FLEX_WRAP_WRAP,
    FLEX_WRAP_WRAP_REVERSE
} FlexWrap;

typedef enum {
    JUSTIFY_FLEX_START = 0,
    JUSTIFY_FLEX_END,
    JUSTIFY_CENTER,
    JUSTIFY_SPACE_BETWEEN,
    JUSTIFY_SPACE_AROUND,
    JUSTIFY_SPACE_EVENLY
} JustifyContent;

typedef enum {
    ALIGN_FLEX_START = 0,
    ALIGN_FLEX_END,
    ALIGN_CENTER,
    ALIGN_STRETCH,
    ALIGN_BASELINE
} AlignItems;

// ═══════════════════════════════════════════════════════════════
//  هيكل لون RGBA
// ═══════════════════════════════════════════════════════════════
typedef struct {
    u8 r, g, b, a;
} Color;

// ═══════════════════════════════════════════════════════════════
//  هيكل وحدات القياس CSS
// ═══════════════════════════════════════════════════════════════
typedef enum {
    UNIT_PX = 0,
    UNIT_PERCENT,
    UNIT_EM,
    UNIT_REM,
    UNIT_VW,
    UNIT_VH,
    UNIT_AUTO,
    UNIT_NONE
} CSSUnit;

typedef struct {
    f32     value;
    CSSUnit unit;
} CSSValue;

// ═══════════════════════════════════════════════════════════════
//  هيكل الحشو والهامش
// ═══════════════════════════════════════════════════════════════
typedef struct {
    CSSValue top, right, bottom, left;
} CSSBox;

// ═══════════════════════════════════════════════════════════════
//  هيكل الحدود CSS
// ═══════════════════════════════════════════════════════════════
typedef enum {
    BORDER_NONE = 0,
    BORDER_SOLID,
    BORDER_DASHED,
    BORDER_DOTTED,
    BORDER_DOUBLE,
    BORDER_GROOVE,
    BORDER_RIDGE
} BorderStyle;

typedef struct {
    CSSValue    width;
    BorderStyle style;
    Color       color;
} CSSBorder;

typedef struct {
    CSSBorder top, right, bottom, left;
    CSSValue  radius_tl, radius_tr, radius_bl, radius_br;
} CSSBorderBox;

// ═══════════════════════════════════════════════════════════════
//  هيكل الخلفية CSS
// ═══════════════════════════════════════════════════════════════
typedef enum {
    BG_NONE = 0,
    BG_COLOR,
    BG_IMAGE,
    BG_GRADIENT_LINEAR,
    BG_GRADIENT_RADIAL
} BackgroundType;

typedef struct {
    BackgroundType  type;
    Color           color;
    char            image_url[HTML_MAX_SRC_LEN];
    Color           gradient_start;
    Color           gradient_end;
    f32             gradient_angle;
    CSSValue        position_x;
    CSSValue        position_y;
    CSSValue        size_x;
    CSSValue        size_y;
    int             repeat_x;
    int             repeat_y;
    int             fixed;
} CSSBackground;

// ═══════════════════════════════════════════════════════════════
//  هيكل الظل CSS
// ═══════════════════════════════════════════════════════════════
typedef struct {
    f32   offset_x;
    f32   offset_y;
    f32   blur;
    f32   spread;
    Color color;
    int   inset;
} CSSShadow;

// ═══════════════════════════════════════════════════════════════
//  هيكل التحويل CSS (transform)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    f32 translate_x;
    f32 translate_y;
    f32 rotate;
    f32 scale_x;
    f32 scale_y;
    f32 skew_x;
    f32 skew_y;
    int has_transform;
} CSSTransform;

// ═══════════════════════════════════════════════════════════════
//  هيكل خصائص CSS الكاملة لعنصر
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // الحجم
    CSSValue        width;
    CSSValue        height;
    CSSValue        min_width;
    CSSValue        min_height;
    CSSValue        max_width;
    CSSValue        max_height;

    // الموضع
    PositionType    position;
    CSSValue        top;
    CSSValue        right;
    CSSValue        bottom;
    CSSValue        left;
    s32             z_index;

    // العرض
    DisplayType     display;
    OverflowType    overflow_x;
    OverflowType    overflow_y;
    f32             opacity;
    int             visible;

    // الحشو والهامش
    CSSBox          padding;
    CSSBox          margin;

    // الحدود
    CSSBorderBox    border;

    // الخلفية
    CSSBackground   background;

    // النص
    Color           color;
    CSSValue        font_size;
    FontWeight      font_weight;
    int             font_italic;
    int             font_underline;
    int             font_strikethrough;
    TextAlign       text_align;
    CSSValue        line_height;
    CSSValue        letter_spacing;
    CSSValue        word_spacing;
    int             text_wrap;
    int             text_overflow_ellipsis;

    // Flex
    FlexDirection   flex_direction;
    FlexWrap        flex_wrap;
    JustifyContent  justify_content;
    AlignItems      align_items;
    AlignItems      align_content;
    f32             flex_grow;
    f32             flex_shrink;
    CSSValue        flex_basis;
    CSSValue        gap;

    // الظل والتحويل
    CSSShadow       box_shadow;
    CSSShadow       text_shadow;
    CSSTransform    transform;

    // الرسوم المتحركة
    f32             transition_duration;
    char            transition_property[64];

    // مؤشر الماوس
    int             cursor_pointer;

    // التصفية
    f32             filter_blur;
    f32             filter_brightness;
    f32             filter_contrast;
    f32             filter_saturate;

} CSSStyle;

// ═══════════════════════════════════════════════════════════════
//  هيكل سمة HTML
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char name [HTML_MAX_ATTR_LEN];
    char value[HTML_MAX_ATTR_LEN];
} HTMLAttribute;

// ═══════════════════════════════════════════════════════════════
//  هيكل عُقدة DOM
// ═══════════════════════════════════════════════════════════════
typedef struct HTMLNode HTMLNode;
struct HTMLNode {
    NodeType        type;
    char            tag    [HTML_MAX_TAG_LEN];
    char            id     [HTML_MAX_ID_LEN];
    char            class_name[HTML_MAX_CLASS_LEN];
    char            text   [HTML_MAX_TEXT_LEN];
    char            href   [HTML_MAX_HREF_LEN];
    char            src    [HTML_MAX_SRC_LEN];
    char            alt    [HTML_MAX_ATTR_LEN];
    char            style_str[HTML_MAX_STYLE_LEN];
    char            onclick[HTML_MAX_ATTR_LEN];
    char            value  [HTML_MAX_ATTR_LEN];
    char            placeholder[HTML_MAX_ATTR_LEN];
    char            input_type [HTML_MAX_TAG_LEN];
    int             hidden;
    int             disabled;
    int             checked;

    HTMLAttribute   attrs     [HTML_MAX_ATTRS];
    int             attr_count;

    // شجرة DOM
    HTMLNode*       parent;
    HTMLNode*       children  [HTML_MAX_CHILDREN];
    int             child_count;
    int             node_index;

    // تخطيط الصفحة (Layout)
    f32             layout_x;
    f32             layout_y;
    f32             layout_w;
    f32             layout_h;
    f32             scroll_x;
    f32             scroll_y;

    // الأنماط المحسوبة
    CSSStyle        computed_style;
    CSSStyle        inline_style;
    int             style_dirty;

    // حالة التفاعل
    int             is_hovered;
    int             is_focused;
    int             is_active;
    int             is_selected;
};

// ═══════════════════════════════════════════════════════════════
//  هيكل قاعدة CSS
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char        selector   [CSS_MAX_SELECTOR_LEN];
    char        property   [CSS_MAX_PROPERTIES][CSS_MAX_PROPERTY_LEN];
    char        value_str  [CSS_MAX_PROPERTIES][CSS_MAX_VALUE_LEN];
    int         prop_count;
    int         specificity;
    int         is_media_query;
    int         media_min_width;
    int         media_max_width;
} CSSRule;

// ═══════════════════════════════════════════════════════════════
//  هيكل قائمة CSS
// ═══════════════════════════════════════════════════════════════
typedef struct {
    CSSRule     rules     [CSS_MAX_RULES];
    int         rule_count;
    char*       raw_css;
    size_t      raw_css_len;
} CSSStyleSheet;

// ═══════════════════════════════════════════════════════════════
//  هيكل مستمع الأحداث
// ═══════════════════════════════════════════════════════════════
typedef enum {
    EVENT_CLICK = 0,
    EVENT_MOUSEOVER,
    EVENT_MOUSEOUT,
    EVENT_KEYDOWN,
    EVENT_KEYUP,
    EVENT_CHANGE,
    EVENT_SUBMIT,
    EVENT_LOAD,
    EVENT_SCROLL,
    EVENT_FOCUS,
    EVENT_BLUR,
    EVENT_INPUT,
    EVENT_COUNT
} EventType;

typedef struct {
    EventType   type;
    HTMLNode*   target;
    char        handler[HTML_MAX_ATTR_LEN];
    int         active;
} EventListener;

// ═══════════════════════════════════════════════════════════════
//  هيكل متغيرات جافاسكريبت
// ═══════════════════════════════════════════════════════════════
typedef enum {
    JS_TYPE_UNDEFINED = 0,
    JS_TYPE_NULL,
    JS_TYPE_BOOLEAN,
    JS_TYPE_NUMBER,
    JS_TYPE_STRING,
    JS_TYPE_OBJECT,
    JS_TYPE_ARRAY,
    JS_TYPE_FUNCTION
} JSType;

typedef struct {
    JSType  type;
    char    name  [128];
    f64     number_val;
    char    string_val[JS_MAX_STRING_LEN];
    int     bool_val;
    int     is_const;
    int     scope_level;
} JSVariable;

// ═══════════════════════════════════════════════════════════════
//  هيكل محرك جافاسكريبت البسيط
// ═══════════════════════════════════════════════════════════════
typedef struct {
    JSVariable  variables [JS_MAX_VARS];
    int         var_count;
    int         scope_level;
    char        code      [JS_MAX_CODE_LEN];
    int         code_len;
    int         error;
    char        error_msg [256];
    int         enabled;
} JSEngine;

// ═══════════════════════════════════════════════════════════════
//  هيكل صورة
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char        url  [HTML_MAX_SRC_LEN];
    u8*         data;
    u32         width;
    u32         height;
    u32         channels;
    size_t      data_size;
    int         loaded;
    int         loading;
    int         error;
    u32         texture_id;
    time_t      cached_at;
} ImageResource;

// ═══════════════════════════════════════════════════════════════
//  هيكل ذاكرة التخزين المؤقت للصور
// ═══════════════════════════════════════════════════════════════
#define IMAGE_CACHE_MAX     64

typedef struct {
    ImageResource   images[IMAGE_CACHE_MAX];
    int             count;
    size_t          total_size;
    size_t          max_size;
} ImageCache;

// ═══════════════════════════════════════════════════════════════
//  هيكل معلومات الصفحة
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char        url          [HTML_MAX_HREF_LEN];
    char        base_url     [HTML_MAX_HREF_LEN];
    char        title        [256];
    char        description  [512];
    char        charset      [32];
    char        lang         [16];
    char        favicon_url  [HTML_MAX_SRC_LEN];
    char        og_title     [256];
    char        og_image     [HTML_MAX_SRC_LEN];
    int         is_https;
    int         is_loaded;
    int         has_error;
    char        error_msg    [256];
    time_t      load_time;
    f32         load_duration;
    size_t      page_size;
    int         status_code;
    char        content_type [128];
    char        raw_html     [0]; // بيانات ديناميكية في الذاكرة
} PageInfo;

// ═══════════════════════════════════════════════════════════════
//  هيكل محرك التخطيط (Layout Engine)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    f32         viewport_width;
    f32         viewport_height;
    f32         scroll_x;
    f32         scroll_y;
    f32         zoom;
    int         viewport_dirty;
    HTMLNode*   root;
    f32         total_height;
    f32         total_width;
} LayoutEngine;

// ═══════════════════════════════════════════════════════════════
//  هيكل محرك التصيير (Paint Engine)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    u32*        pixel_buffer;
    u32         buffer_width;
    u32         buffer_height;
    u32         buffer_pitch;
    f32         dirty_x;
    f32         dirty_y;
    f32         dirty_w;
    f32         dirty_h;
    int         needs_repaint;
    int         partial_repaint;
} PaintEngine;

// ═══════════════════════════════════════════════════════════════
//  هيكل المتصفح الكامل (Browser Engine)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // محللات
    HTMLNode        node_pool   [HTML_MAX_NODES];
    int             node_count;
    HTMLNode*       document;
    HTMLNode*       body;
    HTMLNode*       head;

    // CSS
    CSSStyleSheet   stylesheets [16];
    int             stylesheet_count;
    CSSStyleSheet   inline_styles;
    CSSStyleSheet   user_agent_styles;

    // جافاسكريبت
    JSEngine        js_engine;

    // الأحداث
    EventListener   event_listeners[DOM_MAX_EVENT_LISTENERS];
    int             event_count;

    // الصور
    ImageCache      image_cache;

    // التخطيط والتصيير
    LayoutEngine    layout;
    PaintEngine     paint;

    // معلومات الصفحة
    char            current_url  [HTML_MAX_HREF_LEN];
    char            current_title[256];
    int             page_loaded;
    int             page_loading;
    f32             load_progress;
    size_t          page_size;

    // الإحصائيات
    u32             paint_calls;
    u32             layout_calls;
    u32             js_executions;
    f32             last_paint_time;
    f32             last_layout_time;
    size_t          memory_used;

} BrowserEngine;

// ═══════════════════════════════════════════════════════════════
//  واجهة برمجة محلل HTML
// ═══════════════════════════════════════════════════════════════
#ifdef __cplusplus
extern "C" {
#endif

// تهيئة المحرك
BrowserEngine*  browser_engine_create  (void* memory_pool, size_t pool_size);
void            browser_engine_destroy (BrowserEngine* engine);
void            browser_engine_reset   (BrowserEngine* engine);

// تحليل HTML
int             html_parse             (BrowserEngine* engine, const char* html, size_t len);
HTMLNode*       html_alloc_node        (BrowserEngine* engine, NodeType type);
HTMLNode*       html_find_by_id        (BrowserEngine* engine, const char* id);
HTMLNode*       html_find_by_tag       (BrowserEngine* engine, const char* tag, int index);
int             html_find_all_by_class (BrowserEngine* engine, const char* class_name,
                                        HTMLNode** out, int max);
const char*     html_get_attr          (HTMLNode* node, const char* name);
void            html_set_attr          (HTMLNode* node, const char* name, const char* value);

// تحليل CSS
int             css_parse              (BrowserEngine* engine, const char* css,
                                        size_t len, int sheet_index);
void            css_apply_user_agent   (BrowserEngine* engine);
void            css_compute_styles     (BrowserEngine* engine);
void            css_compute_node       (BrowserEngine* engine, HTMLNode* node,
                                        HTMLNode* parent);
int             css_parse_color        (const char* str, Color* out);
CSSValue        css_parse_value        (const char* str);
f32             css_resolve_value      (CSSValue val, f32 parent_size, f32 viewport_size);
int             css_selector_matches   (const char* selector, HTMLNode* node);
int             css_specificity        (const char* selector);

// محرك التخطيط
void            layout_compute         (BrowserEngine* engine, f32 vw, f32 vh);
void            layout_node            (BrowserEngine* engine, HTMLNode* node,
                                        f32 parent_x, f32 parent_y,
                                        f32 parent_w, f32 parent_h);
void            layout_flex            (BrowserEngine* engine, HTMLNode* node);
void            layout_block           (BrowserEngine* engine, HTMLNode* node,
                                        f32 x, f32 y, f32 w, f32 h);
void            layout_inline          (BrowserEngine* engine, HTMLNode* node,
                                        f32 x, f32 y, f32 w);
void            layout_text_wrap       (HTMLNode* node, f32 max_width,
                                        f32 font_size, f32* out_w, f32* out_h);
f32             layout_get_total_height(BrowserEngine* engine);

// محرك جافاسكريبت
int             js_execute             (JSEngine* js, const char* code);
int             js_set_var             (JSEngine* js, const char* name,
                                        JSType type, const void* value);
JSVariable*     js_get_var             (JSEngine* js, const char* name);
int             js_call_func           (JSEngine* js, const char* func_name,
                                        const char** args, int argc);
void            js_inject_dom_api      (JSEngine* js, BrowserEngine* engine);
void            js_handle_event        (BrowserEngine* engine, EventType type,
                                        HTMLNode* target);

// الأحداث
void            dom_add_event          (BrowserEngine* engine, HTMLNode* node,
                                        EventType type, const char* handler);
void            dom_dispatch_event     (BrowserEngine* engine, EventType type,
                                        HTMLNode* target);
void            dom_handle_click       (BrowserEngine* engine, f32 x, f32 y);
HTMLNode*       dom_hit_test           (BrowserEngine* engine, f32 x, f32 y);

// الصور
ImageResource*  image_load             (ImageCache* cache, const char* url,
                                        void* data, size_t data_size);
ImageResource*  image_find             (ImageCache* cache, const char* url);
void            image_free             (ImageCache* cache, const char* url);
void            image_cache_clear      (ImageCache* cache);

// أدوات مساعدة
NodeType        html_tag_to_type       (const char* tag);
const char*     html_type_to_tag       (NodeType type);
u32             color_to_u32           (Color c);
Color           u32_to_color           (u32 v);
Color           color_mix             (Color a, Color b, f32 t);
Color           color_alpha_blend      (Color src, Color dst);
f32             lerp                   (f32 a, f32 b, f32 t);
f32             clampf                 (f32 v, f32 mn, f32 mx);
int             clampi                 (int  v, int  mn, int  mx);
void            url_resolve            (const char* base, const char* rel,
                                        char* out, size_t out_len);
int             url_is_absolute        (const char* url);
void            html_decode_entities   (const char* in, char* out, size_t out_len);
void            str_trim               (char* str);
void            str_to_lower           (char* str);
int             str_starts_with        (const char* str, const char* prefix);
int             str_ends_with          (const char* str, const char* suffix);

#ifdef __cplusplus
}
#endif

// ═══════════════════════════════════════════════════════════════
//  ماكرو مساعدة
// ═══════════════════════════════════════════════════════════════
#define RGBA(r,g,b,a)   ((Color){(u8)(r),(u8)(g),(u8)(b),(u8)(a)})
#define RGB(r,g,b)      RGBA(r,g,b,255)
#define COLOR_TRANSPARENT RGBA(0,0,0,0)
#define COLOR_BLACK       RGB(0,0,0)
#define COLOR_WHITE       RGB(255,255,255)
#define COLOR_RED         RGB(255,0,0)
#define COLOR_GREEN       RGB(0,255,0)
#define COLOR_BLUE        RGB(0,0,255)

#define CSS_PX(v)       ((CSSValue){(f32)(v), UNIT_PX})
#define CSS_PCT(v)      ((CSSValue){(f32)(v), UNIT_PERCENT})
#define CSS_AUTO        ((CSSValue){0.0f,      UNIT_AUTO})
#define CSS_NONE        ((CSSValue){0.0f,      UNIT_NONE})
#define CSS_EM(v)       ((CSSValue){(f32)(v),  UNIT_EM})

#define MIN(a,b)        ((a)<(b)?(a):(b))
#define MAX(a,b)        ((a)>(b)?(a):(b))
#define ABS(a)          ((a)<0?-(a):(a))
#define CLAMP(v,mn,mx)  ((v)<(mn)?(mn):((v)>(mx)?(mx):(v)))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

#define BROWSER_LOG(fmt, ...) printf("[متصفح] " fmt "\n", ##__VA_ARGS__)
#define BROWSER_WARN(fmt,...) printf("[تحذير] " fmt "\n", ##__VA_ARGS__)
#define BROWSER_ERR(fmt, ...) printf("[خطأ]   " fmt "\n", ##__VA_ARGS__)

#endif // BROWSER_H
