// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - ui.cpp
//  واجهة المستخدم الكاملة بتصميم أوبرا جي إكس
// ═══════════════════════════════════════════════════════════════

#include "ui.h"
#include "renderer.h"
#include "browser.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════
//  ثوابت الواجهة
// ═══════════════════════════════════════════════════════════════
#define UI_TAB_H           48
#define UI_URL_H           52
#define UI_STATUS_H        28
#define UI_SIDEBAR_W       280
#define UI_NOTIF_H         48
#define UI_NOTIF_W         380
#define UI_CORNER_R        8.0f
#define UI_FONT_SM         12
#define UI_FONT_MD         15
#define UI_FONT_LG         18
#define UI_FONT_XL         22
#define UI_FONT_XXL        32
#define UI_ANIM_SPEED      8.0f
#define UI_GLOW_SIZE       12.0f
#define UI_ICON_SIZE       20

// ألوان أوبرا جي إكس الكاملة
#define C_BG_MAIN          0x0D0D0DFF
#define C_BG_PANEL         0x141420FF
#define C_BG_CARD          0x1A1A2EFF
#define C_BG_HOVER         0x1E1E32FF
#define C_BG_ACTIVE        0x22223AFF
#define C_ACCENT           0xFF1A1AFF
#define C_ACCENT_DIM       0xFF1A1A66
#define C_ACCENT_GLOW      0xFF000044
#define C_CYAN             0x00E5FFFF
#define C_CYAN_DIM         0x00E5FF44
#define C_PURPLE           0x9D00FFFF
#define C_PINK             0xFF006EFF
#define C_TEXT_W           0xFFFFFFFF
#define C_TEXT_G           0xAAAAAAFF
#define C_TEXT_DG          0x666666FF
#define C_TEXT_A           0xFF6666FF
#define C_BORDER           0xFF1A1A33
#define C_BORDER_B         0xFF1A1A88
#define C_SCROLL           0xFF1A1A88
#define C_GREEN            0x00FF88FF
#define C_YELLOW           0xFFCC00FF
#define C_ORANGE           0xFF6600FF

// ═══════════════════════════════════════════════════════════════
//  هيكل سياق الواجهة
// ═══════════════════════════════════════════════════════════════
struct UIContext {
    RendererContext* renderer;
    MemoryPool*      pool;
    int              width;
    int              height;
    float            time;
    float            dt;
    // حالة الرسوم المتحركة
    float            tab_hover_alpha[8];
    float            btn_hover_alpha[32];
    float            sidebar_anim;
    float            url_focus_anim;
    float            splash_logo_scale;
    float            splash_ring_angle;
    // محرك HTML
    BrowserEngine*   browser_engine;
    void*            engine_memory;
    // تمرير الصفحة
    int              page_content_x;
    int              page_content_y;
    int              page_content_w;
    int              page_content_h;
};

// ═══════════════════════════════════════════════════════════════
//  رسم مستطيل ملوّن
// ═══════════════════════════════════════════════════════════════
static inline void draw_rect(UIContext* ui, int x, int y, int w, int h, u32 color) {
    renderer_fill_rect(ui->renderer, x, y, w, h, color);
}

// ═══════════════════════════════════════════════════════════════
//  رسم مستطيل بزوايا مستديرة
// ═══════════════════════════════════════════════════════════════
static void draw_rounded_rect(UIContext* ui, int x, int y, int w, int h,
                               float r, u32 color) {
    if (r <= 0) { draw_rect(ui, x, y, w, h, color); return; }
    int ri = (int)r;
    // المستطيل المركزي
    draw_rect(ui, x+ri, y,    w-2*ri, h,       color);
    draw_rect(ui, x,    y+ri, ri,     h-2*ri,  color);
    draw_rect(ui, x+w-ri, y+ri, ri,   h-2*ri, color);
    // زوايا مستديرة بالبكسل
    for (int dy = 0; dy < ri; dy++) {
        for (int dx = 0; dx < ri; dx++) {
            float dist = sqrtf((float)((ri-dx)*(ri-dx)+(ri-dy)*(ri-dy)));
            if (dist <= (float)ri) {
                renderer_set_pixel(ui->renderer, x+dx,       y+dy,       color);
                renderer_set_pixel(ui->renderer, x+w-1-dx,   y+dy,       color);
                renderer_set_pixel(ui->renderer, x+dx,       y+h-1-dy,   color);
                renderer_set_pixel(ui->renderer, x+w-1-dx,   y+h-1-dy,   color);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم مستطيل بحد
// ═══════════════════════════════════════════════════════════════
static void draw_bordered_rect(UIContext* ui, int x, int y, int w, int h,
                                float r, u32 fill, u32 border, int bw) {
    draw_rounded_rect(ui, x, y, w, h, r, fill);
    for (int i = 0; i < bw; i++) {
        renderer_draw_rect_outline(ui->renderer, x+i, y+i, w-2*i, h-2*i, border);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم توهج
// ═══════════════════════════════════════════════════════════════
static void draw_glow(UIContext* ui, int x, int y, int w, int h,
                      u32 glow_color, float intensity) {
    if (intensity <= 0) return;
    u8 base_a = (u8)((glow_color & 0xFF) * intensity);
    for (int i = 1; i <= (int)UI_GLOW_SIZE; i++) {
        float t = 1.0f - (float)i / UI_GLOW_SIZE;
        u8 a = (u8)(base_a * t * t);
        u32 c = (glow_color & 0xFFFFFF00) | a;
        renderer_draw_rect_outline(ui->renderer, x-i, y-i, w+2*i, h+2*i, c);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم نص
// ═══════════════════════════════════════════════════════════════
static void draw_text(UIContext* ui, const char* text, int x, int y,
                      int font_size, u32 color) {
    renderer_draw_text(ui->renderer, text, x, y, font_size, color);
}

// ═══════════════════════════════════════════════════════════════
//  رسم شريط تدرج أفقي
// ═══════════════════════════════════════════════════════════════
static void draw_gradient_h(UIContext* ui, int x, int y, int w, int h,
                             u32 c1, u32 c2) {
    for (int i = 0; i < w; i++) {
        float t = (float)i / (float)w;
        u8 r = (u8)(((c1>>24)&0xFF) * (1-t) + ((c2>>24)&0xFF) * t);
        u8 g = (u8)(((c1>>16)&0xFF) * (1-t) + ((c2>>16)&0xFF) * t);
        u8 b = (u8)(((c1>> 8)&0xFF) * (1-t) + ((c2>> 8)&0xFF) * t);
        u8 a = (u8)(((c1    )&0xFF) * (1-t) + ((c2    )&0xFF) * t);
        u32 c = ((u32)r<<24)|((u32)g<<16)|((u32)b<<8)|a;
        draw_rect(ui, x+i, y, 1, h, c);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم أيقونة بسيطة (بالبكسل)
// ═══════════════════════════════════════════════════════════════
typedef enum {
    ICON_BACK=0, ICON_FORWARD, ICON_REFRESH, ICON_HOME,
    ICON_SEARCH, ICON_CLOSE, ICON_MENU, ICON_BOOKMARK,
    ICON_SETTINGS, ICON_DOWNLOAD, ICON_LOCK, ICON_UNLOCK,
    ICON_NEW_TAB, ICON_HISTORY, ICON_STAR, ICON_SHARE,
    ICON_ARROW_UP, ICON_ARROW_DOWN, ICON_WIFI, ICON_SPEED
} IconType;

static void draw_icon(UIContext* ui, IconType icon, int x, int y,
                      int size, u32 color) {
    int h = size, w = size, cx = x+w/2, cy = y+h/2;
    int s = size/6;
    switch(icon) {
        case ICON_BACK:
            // سهم يسار
            for(int i=0;i<w/2;i++){
                renderer_set_pixel(ui->renderer,cx-i,cy-i,color);
                renderer_set_pixel(ui->renderer,cx-i,cy+i,color);
            }
            for(int i=0;i<w/2;i++) renderer_set_pixel(ui->renderer,cx-w/4+i,cy,color);
            break;
        case ICON_FORWARD:
            for(int i=0;i<w/2;i++){
                renderer_set_pixel(ui->renderer,cx+i,cy-i,color);
                renderer_set_pixel(ui->renderer,cx+i,cy+i,color);
            }
            for(int i=0;i<w/2;i++) renderer_set_pixel(ui->renderer,cx-w/4+i,cy,color);
            break;
        case ICON_REFRESH:
            for(int i=0;i<360;i+=12){
                float rad=(float)i*3.14159f/180.0f;
                int px=(int)(cx+cosf(rad)*w/3), py=(int)(cy+sinf(rad)*h/3);
                renderer_set_pixel(ui->renderer,px,py,color);
            }
            break;
        case ICON_HOME:
            // سقف المنزل
            for(int i=0;i<w/2;i++){
                renderer_set_pixel(ui->renderer,cx-i,cy-i,color);
                renderer_set_pixel(ui->renderer,cx+i,cy-i,color);
            }
            draw_rect(ui,cx-w/4,cy,w/2,h/3,color);
            break;
        case ICON_CLOSE:
            for(int i=0;i<w*2/3;i++){
                renderer_set_pixel(ui->renderer,x+s+i,y+s+i,color);
                renderer_set_pixel(ui->renderer,x+w-s-i,y+s+i,color);
            }
            break;
        case ICON_SEARCH:
            for(int i=0;i<360;i+=15){
                float rad=(float)i*3.14159f/180.0f;
                int px=(int)(cx-s+cosf(rad)*w/4), py=(int)(cy-s+sinf(rad)*h/4);
                renderer_set_pixel(ui->renderer,px,py,color);
            }
            for(int i=0;i<w/4;i++) renderer_set_pixel(ui->renderer,cx+i/2+2,cy+i/2+2,color);
            break;
        case ICON_MENU:
            draw_rect(ui,x+s,cy-h/4,w-2*s,2,color);
            draw_rect(ui,x+s,cy,    w-2*s,2,color);
            draw_rect(ui,x+s,cy+h/4,w-2*s,2,color);
            break;
        case ICON_BOOKMARK:
            draw_rect(ui,cx-w/4,y+s,w/2,h*2/3,color);
            for(int i=0;i<w/4;i++){
                renderer_set_pixel(ui->renderer,cx-i,y+s+h*2/3-i,color);
                renderer_set_pixel(ui->renderer,cx+i,y+s+h*2/3-i,color);
            }
            break;
        case ICON_SETTINGS:
            for(int i=0;i<360;i+=45){
                float rad=(float)i*3.14159f/180.0f;
                int px=(int)(cx+cosf(rad)*w/3), py=(int)(cy+sinf(rad)*h/3);
                draw_rect(ui,px-1,py-1,3,3,color);
            }
            for(int i=0;i<360;i+=20){
                float rad=(float)i*3.14159f/180.0f;
                int px=(int)(cx+cosf(rad)*w/6), py=(int)(cy+sinf(rad)*h/6);
                renderer_set_pixel(ui->renderer,px,py,color);
            }
            break;
        case ICON_LOCK:
            draw_rect(ui,cx-w/5,cy,w*2/5,h/3,color);
            for(int i=0;i<180;i+=15){
                float rad=(float)i*3.14159f/180.0f;
                int px=(int)(cx+cosf(rad)*w/5), py=(int)(cy-sinf(rad)*h/5);
                renderer_set_pixel(ui->renderer,px,py,color);
            }
            break;
        case ICON_UNLOCK:
            draw_rect(ui,cx-w/5,cy,w*2/5,h/3,color);
            for(int i=0;i<180;i+=15){
                float rad=(float)i*3.14159f/180.0f;
                int px=(int)(cx-w/5+cosf(rad)*w/5), py=(int)(cy-sinf(rad)*h/5);
                renderer_set_pixel(ui->renderer,px,py,color);
            }
            break;
        case ICON_STAR:
            for(int i=0;i<5;i++){
                float rad=(float)i*72*3.14159f/180.0f - 3.14159f/2;
                int px=(int)(cx+cosf(rad)*w/3), py=(int)(cy+sinf(rad)*h/3);
                int px2=(int)(cx+cosf(rad+36*3.14159f/180)*w/6);
                int py2=(int)(cy+sinf(rad+36*3.14159f/180)*h/6);
                renderer_set_pixel(ui->renderer,px,py,color);
                renderer_set_pixel(ui->renderer,px2,py2,color);
            }
            break;
        case ICON_WIFI:
            for(int r=1;r<=3;r++){
                for(int i=-45;i<=45;i+=5){
                    float rad=(float)i*3.14159f/180.0f;
                    int px=(int)(cx+cosf(rad)*w*r/8), py=(int)(cy-sinf(rad)*h*r/8+h/6);
                    renderer_set_pixel(ui->renderer,px,py,color);
                }
            }
            renderer_set_pixel(ui->renderer,cx,cy+h/4,color);
            break;
        default:
            draw_rect(ui,cx-2,cy-2,4,4,color);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة سياق الواجهة
// ═══════════════════════════════════════════════════════════════
UIContext* ui_init(RendererContext* renderer, MemoryPool* pool, int w, int h) {
    UIContext* ui = (UIContext*)memory_pool_alloc(pool, sizeof(UIContext));
    if (!ui) return NULL;
    memset(ui, 0, sizeof(UIContext));
    ui->renderer = renderer;
    ui->pool     = pool;
    ui->width    = w;
    ui->height   = h;
    ui->splash_logo_scale = 0.0f;

    // تهيئة محرك HTML
    size_t engine_size = sizeof(BrowserEngine);
    ui->engine_memory  = memory_pool_alloc(pool, engine_size);
    if (ui->engine_memory) {
        ui->browser_engine = browser_engine_create(ui->engine_memory, engine_size);
    }

    // منطقة محتوى الصفحة
    ui->page_content_x = 0;
    ui->page_content_y = UI_TAB_H + UI_URL_H;
    ui->page_content_w = w;
    ui->page_content_h = h - UI_TAB_H - UI_URL_H - UI_STATUS_H;

    BROWSER_LOG("تم تهيئة واجهة المستخدم %dx%d", w, h);
    return ui;
}

void ui_shutdown(UIContext* ui) {
    if (!ui) return;
    if (ui->browser_engine) browser_engine_destroy(ui->browser_engine);
    BROWSER_LOG("تم إيقاف واجهة المستخدم");
}

// ═══════════════════════════════════════════════════════════════
//  رسم الخلفية الرئيسية
// ═══════════════════════════════════════════════════════════════
void ui_draw_background(UIContext* ui) {
    if (!ui) return;
    // خلفية أساسية
    draw_rect(ui, 0, 0, ui->width, ui->height, C_BG_MAIN);
    // شبكة خفية بنمط أوبرا
    for (int y = 0; y < ui->height; y += 40) {
        draw_rect(ui, 0, y, ui->width, 1, 0x14141EFF);
    }
    for (int x = 0; x < ui->width; x += 40) {
        draw_rect(ui, x, 0, 1, ui->height, 0x14141EFF);
    }
    // خط حمر علوي مضيء
    draw_rect(ui, 0, 0, ui->width, 2, C_ACCENT);
    draw_glow(ui, 0, 0, ui->width, 2, C_ACCENT_GLOW, 0.6f);
}

// ═══════════════════════════════════════════════════════════════
//  رسم شاشة البداية
// ═══════════════════════════════════════════════════════════════
void ui_draw_splash(UIContext* ui, float alpha) {
    if (!ui) return;
    int cx = ui->width / 2, cy = ui->height / 2;

    // خلفية
    draw_rect(ui, 0, 0, ui->width, ui->height, C_BG_MAIN);

    u8 a = (u8)(alpha * 255);

    // حلقات نبضية
    ui->splash_ring_angle += 0.02f;
    for (int r = 1; r <= 5; r++) {
        float phase = ui->splash_ring_angle + r * 0.4f;
        float scale = 0.6f + 0.4f * sinf(phase);
        int radius = (int)(60 * r * scale);
        u8 ring_a  = (u8)(a * 0.15f * (6 - r) / 5.0f);
        u32 ring_c = (C_ACCENT & 0xFFFFFF00) | ring_a;
        for (int i = 0; i < 360; i += 3) {
            float rad = (float)i * 3.14159f / 180.0f;
            int px = (int)(cx + cosf(rad) * radius);
            int py = (int)(cy + sinf(rad) * radius);
            if (px >= 0 && px < ui->width && py >= 0 && py < ui->height)
                renderer_set_pixel(ui->renderer, px, py, ring_c);
        }
    }

    // شعار - حرف PS3
    ui->splash_logo_scale += (1.0f - ui->splash_logo_scale) * 0.05f;
    float scale = ui->splash_logo_scale;
    int logo_w = (int)(200 * scale), logo_h = (int)(80 * scale);
    int lx = cx - logo_w/2, ly = cy - logo_h/2 - 40;

    u32 logo_c = (C_ACCENT & 0xFFFFFF00) | a;
    draw_rounded_rect(ui, lx, ly, logo_w, logo_h, 12*scale, (C_BG_CARD & 0xFFFFFF00)|a);
    renderer_draw_rect_outline(ui->renderer, lx, ly, logo_w, logo_h, logo_c);
    draw_glow(ui, lx, ly, logo_w, logo_h, C_ACCENT_GLOW, alpha * 0.8f);

    // نص الشعار
    u32 txt_c = (C_TEXT_W & 0xFFFFFF00) | a;
    draw_text(ui, "PS3", cx - 45, ly + logo_h/4, (int)(UI_FONT_XXL * scale), logo_c);
    draw_text(ui, "UltraBrowser", cx - 75, ly + logo_h/2 + 5, (int)(UI_FONT_MD * scale), txt_c);

    // نص الإصدار
    draw_text(ui, "v1.0.0", cx - 20, cy + 60, UI_FONT_SM, (C_TEXT_G & 0xFFFFFF00)|a);

    // شريط التحميل
    if (alpha > 0.5f) {
        int bar_w = 300, bar_h = 3;
        int bx = cx - bar_w/2, by = cy + 90;
        draw_rounded_rect(ui, bx, by, bar_w, bar_h, 2, (C_BG_CARD & 0xFFFFFF00)|a);
        float prog = (alpha - 0.5f) * 2.0f;
        draw_gradient_h(ui, bx, by, (int)(bar_w * prog), bar_h,
                        C_ACCENT, C_CYAN);
        draw_glow(ui, bx, by, (int)(bar_w * prog), bar_h, C_ACCENT_GLOW, 0.5f);
    }

    // نص التحميل
    static const char* loading_msgs[] = {
        "جاري التهيئة...", "تحميل المحرك...",
        "إعداد الشبكة...", "جاهز!"
    };
    int msg_idx = (int)(alpha * 3.9f);
    if (msg_idx > 3) msg_idx = 3;
    u32 msg_c = (C_TEXT_G & 0xFFFFFF00) | a;
    int msg_x = cx - (int)(strlen(loading_msgs[msg_idx]) * UI_FONT_SM * 0.35f);
    draw_text(ui, loading_msgs[msg_idx], msg_x, cy + 110, UI_FONT_SM, msg_c);

    // تأثير ضبابي سفلي
    for (int i = 0; i < 40; i++) {
        u8 fog_a = (u8)(a * i / 40.0f * 0.3f);
        draw_rect(ui, 0, ui->height - 40 + i, ui->width, 1,
                  (C_BG_MAIN & 0xFFFFFF00) | fog_a);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم شريط التبويبات
// ═══════════════════════════════════════════════════════════════
void ui_draw_tab_bar(UIContext* ui, BrowserTab* tabs, int count, int active) {
    if (!ui || !tabs) return;

    // خلفية الشريط
    draw_rect(ui, 0, 0, ui->width, UI_TAB_H, C_BG_PANEL);
    // خط فاصل سفلي
    draw_rect(ui, 0, UI_TAB_H-1, ui->width, 1, C_BORDER_B);
    // توهج الخط الفاصل
    draw_glow(ui, 0, UI_TAB_H-2, ui->width, 1, C_ACCENT_GLOW, 0.3f);

    int max_tab_w = (ui->width - 40) / (count > 0 ? count : 1);
    if (max_tab_w > 200) max_tab_w = 200;
    if (max_tab_w < 80)  max_tab_w = 80;

    for (int i = 0; i < count; i++) {
        int tx = i * (max_tab_w + 2) + 2;
        int ty = 4;
        int tw = max_tab_w;
        int th = UI_TAB_H - 8;

        int is_active = (i == active);
        u32 tab_bg    = is_active ? C_BG_CARD : C_BG_PANEL;
        u32 tab_text  = is_active ? C_TEXT_W  : C_TEXT_G;

        draw_rounded_rect(ui, tx, ty, tw, th, UI_CORNER_R, tab_bg);

        if (is_active) {
            // خط علوي أحمر للتبويب النشط
            draw_rect(ui, tx+4, ty, tw-8, 2, C_ACCENT);
            draw_glow(ui, tx+4, ty, tw-8, 2, C_ACCENT_GLOW, 0.7f);
            // حد جانبي
            draw_rect(ui, tx, ty, 1, th, C_BORDER_B);
            draw_rect(ui, tx+tw-1, ty, 1, th, C_BORDER_B);
        }

        // أيقونة القفل (للمواقع الآمنة)
        if (tabs[i].is_secure) {
            draw_icon(ui, ICON_LOCK, tx+6, ty+th/2-UI_ICON_SIZE/2,
                      UI_ICON_SIZE-4, C_GREEN);
        }

        // عنوان التبويب
        char title[32]; int icon_off = tabs[i].is_secure ? 18 : 6;
        int avail_w = tw - icon_off - 20;
        if ((int)strlen(tabs[i].title) * UI_FONT_SM / 2 > avail_w) {
            int max_chars = avail_w / (UI_FONT_SM / 2) - 3;
            if (max_chars < 0) max_chars = 0;
            if (max_chars > 30) max_chars = 30;
            strncpy(title, tabs[i].title, max_chars);
            title[max_chars] = '\0';
            strcat(title, "...");
        } else {
            strncpy(title, tabs[i].title, 31); title[31]='\0';
        }
        draw_text(ui, title, tx+icon_off, ty+th/2-UI_FONT_SM/2, UI_FONT_SM, tab_text);

        // شريط تحميل صغير في التبويب
        if (tabs[i].is_loading) {
            int bar_w = (int)(tabs[i].load_progress * (tw - 4));
            draw_rect(ui, tx+2, ty+th-3, bar_w, 2, C_ACCENT);
        }

        // زر إغلاق التبويب (x)
        if (count > 1 && is_active) {
            draw_icon(ui, ICON_CLOSE, tx+tw-18, ty+th/2-7, 14, C_TEXT_G);
        }
    }

    // زر إضافة تبويب
    int plus_x = count * (max_tab_w + 2) + 8;
    draw_rounded_rect(ui, plus_x, 8, 30, UI_TAB_H-16, 6, C_BG_CARD);
    draw_text(ui, "+", plus_x+9, 8+4, UI_FONT_LG, C_TEXT_G);
}

// ═══════════════════════════════════════════════════════════════
//  رسم شريط العنوان
// ═══════════════════════════════════════════════════════════════
void ui_draw_url_bar(UIContext* ui, const char* url,
                     int focused, float glow_intensity) {
    if (!ui || !url) return;
    int y = UI_TAB_H;

    // خلفية الشريط
    draw_rect(ui, 0, y, ui->width, UI_URL_H, C_BG_PANEL);

    // ── أزرار التنقل ──
    int btn_y = y + (UI_URL_H - 32) / 2;
    // زر الرجوع
    draw_rounded_rect(ui, 8, btn_y, 32, 32, 6, C_BG_CARD);
    draw_icon(ui, ICON_BACK, 8+6, btn_y+6, 20, C_TEXT_G);
    // زر الأمام
    draw_rounded_rect(ui, 44, btn_y, 32, 32, 6, C_BG_CARD);
    draw_icon(ui, ICON_FORWARD, 44+6, btn_y+6, 20, C_TEXT_G);
    // زر التحديث
    draw_rounded_rect(ui, 80, btn_y, 32, 32, 6, C_BG_CARD);
    draw_icon(ui, ICON_REFRESH, 80+6, btn_y+6, 20, C_TEXT_G);
    // زر الرئيسية
    draw_rounded_rect(ui, 116, btn_y, 32, 32, 6, C_BG_CARD);
    draw_icon(ui, ICON_HOME, 116+6, btn_y+6, 20, C_TEXT_G);

    // ── شريط العنوان ──
    int url_x = 155, url_w = ui->width - 155 - 90;
    u32 url_bg = focused ? C_BG_HOVER : C_BG_CARD;
    draw_rounded_rect(ui, url_x, btn_y, url_w, 32, 8, url_bg);

    // حد مضيء عند التركيز
    if (focused || glow_intensity > 0.05f) {
        u8 border_a = (u8)(0x88 * glow_intensity + (focused ? 0x88 : 0));
        u32 border_c = (C_ACCENT & 0xFFFFFF00) | border_a;
        renderer_draw_rect_outline(ui->renderer, url_x, btn_y, url_w, 32, border_c);
        draw_glow(ui, url_x, btn_y, url_w, 32, C_ACCENT_GLOW, glow_intensity);
    }

    // أيقونة بروتوكول
    int is_https = (strncmp(url, "https://", 8) == 0);
    int is_ps3   = (strncmp(url, "ps3://",   6) == 0);
    draw_icon(ui, is_https ? ICON_LOCK : (is_ps3 ? ICON_STAR : ICON_UNLOCK),
              url_x + 6, btn_y + 6, 20,
              is_https ? C_GREEN : C_TEXT_DG);

    // نص العنوان
    const char* display_url = url;
    if (strncmp(url, "ps3://home", 10) == 0) display_url = "الصفحة الرئيسية";
    int url_off = 30;
    int url_text_w = url_w - url_off - 10;
    char url_truncated[256] = {0};
    int max_chars = url_text_w / (UI_FONT_MD / 2);
    if (max_chars < 1) max_chars = 1;
    if ((int)strlen(display_url) > max_chars) {
        strncpy(url_truncated, display_url, max_chars - 3);
        strcat(url_truncated, "...");
    } else {
        strncpy(url_truncated, display_url, 255);
    }
    u32 url_txt_c = focused ? C_TEXT_W : C_TEXT_G;
    draw_text(ui, url_truncated, url_x+url_off, btn_y+8, UI_FONT_MD, url_txt_c);

    // ── أزرار اليمين ──
    int rx = ui->width - 84;
    // زر الإشارة المرجعية
    draw_rounded_rect(ui, rx,    btn_y, 32, 32, 6, C_BG_CARD);
    draw_icon(ui, ICON_BOOKMARK, rx+6, btn_y+6, 20, C_TEXT_G);
    // زر القائمة
    draw_rounded_rect(ui, rx+36, btn_y, 32, 32, 6, C_BG_CARD);
    draw_icon(ui, ICON_MENU, rx+36+6, btn_y+6, 20, C_TEXT_G);

    // خط فاصل
    draw_rect(ui, 0, y+UI_URL_H-1, ui->width, 1, C_BORDER);
}

// ═══════════════════════════════════════════════════════════════
//  رسم الصفحة الرئيسية
// ═══════════════════════════════════════════════════════════════
void ui_draw_home_page(UIContext* ui, HistoryEntry* history, int hist_count,
                       Bookmark* bookmarks, int bm_count) {
    if (!ui) return;
    int y0 = UI_TAB_H + UI_URL_H;
    int pw = ui->width;
    int ph = ui->height - y0 - UI_STATUS_H;
    int cx = pw / 2;

    // خلفية المحتوى
    draw_rect(ui, 0, y0, pw, ph, C_BG_MAIN);

    // شعار المتصفح في المنتصف
    int logo_y = y0 + 40;
    draw_text(ui, "PS3 UltraBrowser", cx - 100, logo_y, UI_FONT_XL, C_ACCENT);
    draw_glow(ui, cx-110, logo_y-4, 220, UI_FONT_XL+8, C_ACCENT_GLOW, 0.5f);

    // شريط البحث الكبير
    int sb_w = 600, sb_h = 48;
    int sb_x = cx - sb_w/2;
    int sb_y = logo_y + 50;
    if (sb_x < 20) sb_x = 20;
    if (sb_w > pw - 40) sb_w = pw - 40;
    draw_rounded_rect(ui, sb_x, sb_y, sb_w, sb_h, 24, C_BG_CARD);
    renderer_draw_rect_outline(ui->renderer, sb_x, sb_y, sb_w, sb_h, C_BORDER_B);
    draw_icon(ui, ICON_SEARCH, sb_x+14, sb_y+14, 20, C_TEXT_G);
    draw_text(ui, "ابحث أو أدخل عنوان...", sb_x+44, sb_y+16, UI_FONT_MD, C_TEXT_DG);
    draw_glow(ui, sb_x, sb_y, sb_w, sb_h, C_ACCENT_GLOW, 0.2f);

    // مواقع مفضلة سريعة
    static const struct { const char* name; const char* url; u32 color; } speed_dial[] = {
        {"يوتيوب",    "https://youtube.com",   0xFF0000FF},
        {"غوغل",     "https://google.com",    0x4285F4FF},
        {"تويتر",     "https://twitter.com",   0x1DA1F2FF},
        {"ريديت",     "https://reddit.com",    0xFF4500FF},
        {"ويكيبيديا", "https://wikipedia.org", 0xCCCCCCFF},
        {"جيتهاب",   "https://github.com",    0x24292EFF},
    };
    int sd_y = sb_y + sb_h + 30;
    int sd_count = 6, sd_w = 90, sd_h = 80, sd_gap = 20;
    int sd_total_w = sd_count * (sd_w + sd_gap) - sd_gap;
    int sd_x = cx - sd_total_w / 2;
    if (sd_x < 20) sd_x = 20;

    for (int i = 0; i < sd_count; i++) {
        int ix = sd_x + i * (sd_w + sd_gap);
        draw_rounded_rect(ui, ix, sd_y, sd_w, sd_h, 12, C_BG_CARD);
        // لون العلامة
        draw_rounded_rect(ui, ix+sd_w/2-18, sd_y+10, 36, 36, 8,
                          (speed_dial[i].color & 0xFFFFFF00) | 0x33);
        // نص الاسم
        int name_x = ix + sd_w/2 - (int)(strlen(speed_dial[i].name)*UI_FONT_SM*0.3f);
        if (name_x < ix) name_x = ix;
        draw_text(ui, speed_dial[i].name, name_x, sd_y+54, UI_FONT_SM, C_TEXT_G);
        renderer_draw_rect_outline(ui->renderer, ix, sd_y, sd_w, sd_h, C_BORDER);
    }

    // المواقع الأخيرة
    if (hist_count > 0) {
        int rec_y = sd_y + sd_h + 30;
        draw_text(ui, "المواقع الأخيرة", 30, rec_y, UI_FONT_MD, C_TEXT_G);
        draw_rect(ui, 30, rec_y + UI_FONT_MD + 4, 150, 1, C_BORDER_B);

        int max_show = hist_count > 4 ? 4 : hist_count;
        int item_w = (pw - 60) / 4 - 10;
        for (int i = 0; i < max_show; i++) {
            int ix = 30 + i * (item_w + 10);
            int iy = rec_y + UI_FONT_MD + 16;
            draw_rounded_rect(ui, ix, iy, item_w, 52, 8, C_BG_CARD);
            // أيقونة
            draw_icon(ui, ICON_BACK, ix+8, iy+16, 20, C_ACCENT);
            // عنوان
            char title[24]; strncpy(title, history[hist_count-1-i].title, 23); title[23]='\0';
            draw_text(ui, title, ix+32, iy+18, UI_FONT_SM, C_TEXT_W);
            renderer_draw_rect_outline(ui->renderer, ix, iy, item_w, 52, C_BORDER);
        }
    }

    // الإشارات المرجعية
    if (bm_count > 0) {
        int bm_y = sd_y + sd_h + (hist_count > 0 ? 120 : 30);
        draw_text(ui, "الإشارات المرجعية", 30, bm_y, UI_FONT_MD, C_TEXT_G);
        draw_rect(ui, 30, bm_y + UI_FONT_MD + 4, 180, 1, C_BORDER_B);
        int max_bm = bm_count > 3 ? 3 : bm_count;
        for (int i = 0; i < max_bm; i++) {
            int iy = bm_y + UI_FONT_MD + 16 + i * 40;
            draw_rounded_rect(ui, 30, iy, pw-60, 36, 6, C_BG_CARD);
            draw_icon(ui, ICON_BOOKMARK, 36, iy+8, 20, C_ACCENT);
            draw_text(ui, bookmarks[i].title, 62, iy+10, UI_FONT_SM, C_TEXT_W);
            draw_text(ui, bookmarks[i].url,   62, iy+24, UI_FONT_SM-2, C_TEXT_DG);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم شاشة التحميل
// ═══════════════════════════════════════════════════════════════
void ui_draw_loading(UIContext* ui, float progress, float angle) {
    if (!ui) return;
    int y0 = UI_TAB_H + UI_URL_H;
    int cx = ui->width / 2;
    int cy = y0 + (ui->height - y0 - UI_STATUS_H) / 2;

    draw_rect(ui, 0, y0, ui->width, ui->height - y0 - UI_STATUS_H, C_BG_MAIN);

    // دائرة تحميل دوارة
    int r = 40;
    for (int i = 0; i < 360; i += 4) {
        float a1 = (float)i * 3.14159f / 180.0f;
        float a2 = angle   * 3.14159f / 180.0f;
        float diff = a1 - a2;
        while (diff < 0) diff += 6.28318f;
        float t = 1.0f - diff / 6.28318f;
        if (t < 0) t = 0;
        u8 seg_a = (u8)(255 * t * t);
        u32 seg_c = (C_ACCENT & 0xFFFFFF00) | seg_a;
        int px = (int)(cx + cosf(a1) * r);
        int py = (int)(cy + sinf(a1) * r);
        draw_rect(ui, px-1, py-1, 3, 3, seg_c);
    }

    // نقطة مركزية
    draw_rounded_rect(ui, cx-8, cy-8, 16, 16, 8, C_ACCENT);
    draw_glow(ui, cx-8, cy-8, 16, 16, C_ACCENT_GLOW, 0.8f);

    // شريط التقدم
    int bar_w = 300, bar_h = 4;
    int bx = cx - bar_w/2, by = cy + 70;
    if (bx < 20) bx = 20;
    if (bar_w > ui->width - 40) bar_w = ui->width - 40;
    draw_rounded_rect(ui, bx, by, bar_w, bar_h, 2, C_BG_CARD);
    if (progress > 0) {
        draw_gradient_h(ui, bx, by, (int)(bar_w * progress), bar_h,
                        C_ACCENT, C_CYAN);
        draw_glow(ui, bx, by, (int)(bar_w * progress), bar_h,
                  C_ACCENT_GLOW, 0.5f);
    }
    // نسبة التحميل
    char pct[16]; snprintf(pct, sizeof(pct), "%d%%", (int)(progress * 100));
    draw_text(ui, pct, cx - 12, by + 12, UI_FONT_MD, C_TEXT_G);
    draw_text(ui, "جاري التحميل...", cx - 55, cy + 50, UI_FONT_MD, C_TEXT_G);
}

// ═══════════════════════════════════════════════════════════════
//  رسم شريط التحميل أعلى الصفحة
// ═══════════════════════════════════════════════════════════════
void ui_draw_loading_bar(UIContext* ui, float progress) {
    if (!ui) return;
    int y = UI_TAB_H + UI_URL_H;
    draw_gradient_h(ui, 0, y, (int)(ui->width * progress), 3,
                    C_ACCENT, C_CYAN);
    draw_glow(ui, 0, y, (int)(ui->width * progress), 3, C_ACCENT_GLOW, 0.6f);
}

// ═══════════════════════════════════════════════════════════════
//  رسم محتوى الصفحة
// ═══════════════════════════════════════════════════════════════
void ui_draw_page_content(UIContext* ui, BrowserTab* tab) {
    if (!ui || !tab) return;
    int y0 = UI_TAB_H + UI_URL_H;
    int ph = ui->height - y0 - UI_STATUS_H;

    // منطقة المحتوى
    draw_rect(ui, 0, y0, ui->width, ph, C_BG_MAIN);

    // إذا لم تُحمَّل الصفحة بعد
    if (!ui->browser_engine || !ui->browser_engine->page_loaded) {
        draw_text(ui, "لا توجد صفحة محملة", 20, y0+20, UI_FONT_MD, C_TEXT_DG);
        return;
    }

    // حساب التخطيط
    float vw = (float)(ui->width);
    float vh = (float)(ph);
    layout_compute(ui->browser_engine, vw, vh);

    // رسم عُقد DOM
    int scroll_y = tab->scroll_y;
    int scroll_x = tab->scroll_x;

    // رسم العُقد بشكل تعاودي
    for (int i = 0; i < ui->browser_engine->node_count; i++) {
        HTMLNode* node = &ui->browser_engine->node_pool[i];
        if (!node->computed_style.visible) continue;
        if (node->type == NODE_TEXT || node->type == NODE_DOCUMENT) continue;

        int nx = (int)node->layout_x - scroll_x;
        int ny = (int)node->layout_y - scroll_y + y0;
        int nw = (int)node->layout_w;
        int nh = (int)node->layout_h;

        // تجاهل العناصر خارج الشاشة
        if (ny + nh < y0 || ny > y0 + ph) continue;
        if (nx + nw < 0 || nx > ui->width)  continue;

        // رسم الخلفية
        CSSStyle* s = &node->computed_style;
        if (s->background.type == BG_COLOR && s->background.color.a > 0) {
            u32 bg_c = color_to_u32(s->background.color);
            if (s->border.radius_tl.value > 0)
                draw_rounded_rect(ui, nx, ny, nw, nh,
                                  s->border.radius_tl.value, bg_c);
            else
                draw_rect(ui, nx, ny, nw, nh, bg_c);
        }

        // رسم النص لعُقد النصوص الأبناء
        for (int ci = 0; ci < node->child_count; ci++) {
            HTMLNode* child = node->children[ci];
            if (!child || child->type != NODE_TEXT) continue;
            if (!child->text[0]) continue;

            int tx = (int)child->layout_x - scroll_x;
            int ty = (int)child->layout_y - scroll_y + y0;
            if (ty + 20 < y0 || ty > y0 + ph) continue;

            float fs = css_resolve_value(s->font_size, 16, vw);
            if (fs <= 0) fs = 16;
            int font_size = (int)fs;
            if (font_size < 8)  font_size = 8;
            if (font_size > 48) font_size = 48;

            // تطبيق تكبير التبويب
            font_size = font_size * tab->zoom_level / 100;

            u32 txt_c = color_to_u32(s->color);
            if (s->color.r==0&&s->color.g==0&&s->color.b==0&&s->color.a==0)
                txt_c = C_TEXT_W;

            // رسم النص مع التفاف
            float char_w = (float)font_size * 0.55f;
            float line_h = (float)font_size * 1.6f;
            float lx = (float)tx, ly = (float)ty;
            const char* p = child->text;
            while (*p) {
                if (*p == '\n' || (lx + char_w > tx + (int)(child->layout_w))) {
                    lx = (float)tx;
                    ly += line_h;
                    if (*p == '\n') { p++; continue; }
                }
                char ch[2] = {*p, '\0'};
                draw_text(ui, ch, (int)lx, (int)ly, font_size, txt_c);
                lx += char_w;
                p++;
            }
        }

        // رسم حدود المستطيل إن وجدت
        if (s->border.top.style != BORDER_NONE && s->border.top.width.value > 0) {
            u32 bc = color_to_u32(s->border.top.color);
            int bw = (int)s->border.top.width.value;
            if (bw < 1) bw = 1;
            for (int bi = 0; bi < bw; bi++)
                renderer_draw_rect_outline(ui->renderer, nx+bi, ny+bi,
                                           nw-2*bi, nh-2*bi, bc);
        }

        // رسم خاص لعناصر معينة
        if (node->type == NODE_INPUT || node->type == NODE_TEXTAREA) {
            draw_rounded_rect(ui, nx, ny, nw, nh, 4, C_BG_HOVER);
            renderer_draw_rect_outline(ui->renderer, nx, ny, nw, nh, C_BORDER_B);
            if (node->placeholder[0])
                draw_text(ui, node->placeholder, nx+8, ny+nh/2-7,
                          UI_FONT_SM, C_TEXT_DG);
            else if (node->value[0])
                draw_text(ui, node->value, nx+8, ny+nh/2-7, UI_FONT_SM, C_TEXT_W);
        } else if (node->type == NODE_BUTTON) {
            u32 btn_bg = node->is_hovered ? C_BG_HOVER : C_BG_CARD;
            draw_rounded_rect(ui, nx, ny, nw, nh, 6, btn_bg);
            renderer_draw_rect_outline(ui->renderer, nx, ny, nw, nh, C_BORDER_B);
            if (node->is_hovered)
                draw_glow(ui, nx, ny, nw, nh, C_ACCENT_GLOW, 0.4f);
            if (node->text[0])
                draw_text(ui, node->text, nx+nw/2-10, ny+nh/2-7,
                          UI_FONT_SM, C_TEXT_W);
        } else if (node->type == NODE_HR) {
            draw_gradient_h(ui, nx, ny+nh/2, nw, 1, C_BORDER, C_BORDER_B);
        }
    }

    // شريط التمرير العمودي
    float total_h = layout_get_total_height(ui->browser_engine);
    if (total_h > ph) {
        int sb_h = (int)((float)ph / total_h * ph);
        if (sb_h < 30) sb_h = 30;
        int sb_y = y0 + (int)((float)scroll_y / total_h * ph);
        draw_rect(ui, ui->width - 6, y0, 6, ph, C_BG_CARD);
        draw_rounded_rect(ui, ui->width-5, sb_y, 4, sb_h, 2, C_SCROLL);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم لوحة مفاتيح العنوان
// ═══════════════════════════════════════════════════════════════
void ui_draw_url_keyboard(UIContext* ui, const char* buffer, int cursor_pos) {
    if (!ui) return;
    int kh = 220, kw = ui->width, ky = ui->height - kh - UI_STATUS_H;

    // خلفية اللوحة
    draw_rounded_rect(ui, 0, ky, kw, kh, 0, C_BG_PANEL);
    renderer_draw_rect_outline(ui->renderer, 0, ky, kw, 1, C_BORDER_B);
    draw_glow(ui, 0, ky, kw, 2, C_ACCENT_GLOW, 0.4f);

    // صندوق الإدخال
    int inbox_h = 36, inbox_y = ky + 10;
    draw_rounded_rect(ui, 10, inbox_y, kw-20, inbox_h, 8, C_BG_HOVER);
    renderer_draw_rect_outline(ui->renderer, 10, inbox_y, kw-20, inbox_h, C_ACCENT);
    draw_glow(ui, 10, inbox_y, kw-20, inbox_h, C_ACCENT_GLOW, 0.6f);

    // نص الإدخال
    char display[256] = {0};
    strncpy(display, buffer, 255);
    int len = (int)strlen(display);
    int max_vis = (kw - 40) / (UI_FONT_MD / 2);
    int start = 0;
    if (cursor_pos > max_vis) start = cursor_pos - max_vis;
    draw_text(ui, display + start, 20, inbox_y + 10, UI_FONT_MD, C_TEXT_W);

    // مؤشر الكتابة
    int cursor_x = 20 + (cursor_pos - start) * (UI_FONT_MD / 2);
    draw_rect(ui, cursor_x, inbox_y + 6, 2, inbox_h - 12, C_ACCENT);

    // صفوف المفاتيح
    static const char* rows[] = {
        "1234567890-",
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm./"
    };
    int key_h = 36, key_gap = 3;
    int row_y = inbox_y + inbox_h + 8;

    for (int r = 0; r < 4; r++) {
        const char* row = rows[r];
        int rlen = (int)strlen(row);
        int key_w = (kw - (rlen+1)*key_gap) / rlen;
        int row_x = key_gap;
        for (int k = 0; k < rlen; k++) {
            draw_rounded_rect(ui, row_x, row_y, key_w, key_h, 4, C_BG_CARD);
            renderer_draw_rect_outline(ui->renderer, row_x, row_y,
                                       key_w, key_h, C_BORDER);
            char ch[2] = {row[k], '\0'};
            draw_text(ui, ch, row_x + key_w/2 - 4, row_y + 10, UI_FONT_MD, C_TEXT_W);
            row_x += key_w + key_gap;
        }
        row_y += key_h + key_gap;
    }

    // مؤشر للمساعدة
    draw_text(ui, "مثلث: إلغاء  |  X: موافق  |  R1: مسح", 20,
              ky + kh - 18, UI_FONT_SM, C_TEXT_DG);
}

// ═══════════════════════════════════════════════════════════════
//  رسم شريط الحالة
// ═══════════════════════════════════════════════════════════════
void ui_draw_status_bar(UIContext* ui, float fps, int net_ok, size_t data_used) {
    if (!ui) return;
    int y = ui->height - UI_STATUS_H;

    draw_rect(ui, 0, y, ui->width, UI_STATUS_H, C_BG_PANEL);
    draw_rect(ui, 0, y, ui->width, 1, C_BORDER);

    // اسم المتصفح
    draw_text(ui, "PS3 UltraBrowser", 8, y+8, UI_FONT_SM, C_TEXT_DG);

    // معدل الإطارات
    char fps_str[32];
    snprintf(fps_str, sizeof(fps_str), "%.0f FPS", fps);
    u32 fps_c = fps >= 28 ? C_GREEN : (fps >= 20 ? C_YELLOW : C_ACCENT);
    draw_text(ui, fps_str, ui->width/2 - 20, y+8, UI_FONT_SM, fps_c);

    // حالة الشبكة
    draw_icon(ui, ICON_WIFI, ui->width - 120, y+4, 20,
              net_ok ? C_GREEN : C_TEXT_DG);
    draw_text(ui, net_ok ? "متصل" : "غير متصل",
              ui->width - 96, y+8, UI_FONT_SM,
              net_ok ? C_GREEN : C_TEXT_DG);

    // حجم البيانات المستخدمة
    char data_str[32];
    if (data_used < 1024)
        snprintf(data_str, sizeof(data_str), "%zuB", data_used);
    else if (data_used < 1024*1024)
        snprintf(data_str, sizeof(data_str), "%.1fKB", data_used/1024.0f);
    else
        snprintf(data_str, sizeof(data_str), "%.1fMB", data_used/(1024.0f*1024));
    draw_text(ui, data_str, ui->width/2 + 40, y+8, UI_FONT_SM, C_TEXT_DG);
}

// ═══════════════════════════════════════════════════════════════
//  رسم القائمة الجانبية
// ═══════════════════════════════════════════════════════════════
void ui_draw_sidebar(UIContext* ui, float offset,
                     Bookmark* bookmarks, int bm_count,
                     HistoryEntry* history, int hist_count) {
    if (!ui) return;
    int sw = UI_SIDEBAR_W;
    int sx = (int)offset;
    int sy = UI_TAB_H + UI_URL_H;
    int sh = ui->height - sy - UI_STATUS_H;

    // ظل القائمة
    for (int i = 0; i < 20; i++) {
        u8 sha = (u8)(120 * (1.0f - (float)i/20.0f));
        draw_rect(ui, sx+sw+i, sy, 1, sh, (0x00000000) | sha);
    }

    // خلفية القائمة
    draw_rect(ui, sx, sy, sw, sh, C_BG_PANEL);
    renderer_draw_rect_outline(ui->renderer, sx, sy, sw, sh, C_BORDER_B);
    draw_glow(ui, sx+sw-2, sy, 2, sh, C_ACCENT_GLOW, 0.3f);

    // عنوان القائمة
    draw_rect(ui, sx, sy, sw, 40, C_BG_CARD);
    draw_text(ui, "القائمة", sx+sw/2-25, sy+12, UI_FONT_LG, C_TEXT_W);
    draw_rect(ui, sx, sy+40, sw, 1, C_BORDER_B);

    // عناصر القائمة الرئيسية
    static const struct { const char* label; IconType icon; } menu_items[] = {
        {"الصفحة الرئيسية", ICON_HOME},
        {"الإشارات المرجعية", ICON_BOOKMARK},
        {"السجل", ICON_HISTORY},
        {"التنزيلات", ICON_DOWNLOAD},
        {"الإعدادات", ICON_SETTINGS},
    };
    int item_y = sy + 50;
    for (int i = 0; i < 5; i++) {
        int item_h = 44;
        draw_rounded_rect(ui, sx+8, item_y, sw-16, item_h, 6, C_BG_CARD);
        draw_icon(ui, menu_items[i].icon, sx+16, item_y+12, 20, C_ACCENT);
        draw_text(ui, menu_items[i].label, sx+44, item_y+14, UI_FONT_MD, C_TEXT_W);
        item_y += item_h + 4;
    }

    // فاصل
    draw_rect(ui, sx+8, item_y+4, sw-16, 1, C_BORDER);
    item_y += 10;

    // الإشارات المرجعية السريعة
    draw_text(ui, "إشارات مرجعية", sx+10, item_y, UI_FONT_SM, C_TEXT_G);
    item_y += 20;
    int bm_show = bm_count > 4 ? 4 : bm_count;
    for (int i = 0; i < bm_show; i++) {
        draw_rounded_rect(ui, sx+8, item_y, sw-16, 32, 4, C_BG_CARD);
        draw_icon(ui, ICON_STAR, sx+12, item_y+6, 16, C_ACCENT);
        char title[28]; strncpy(title, bookmarks[i].title, 27); title[27]='\0';
        draw_text(ui, title, sx+32, item_y+9, UI_FONT_SM, C_TEXT_G);
        item_y += 36;
    }

    // السجل السريع
    draw_rect(ui, sx+8, item_y, sw-16, 1, C_BORDER);
    item_y += 8;
    draw_text(ui, "السجل الأخير", sx+10, item_y, UI_FONT_SM, C_TEXT_G);
    item_y += 20;
    int hist_show = hist_count > 3 ? 3 : hist_count;
    for (int i = 0; i < hist_show; i++) {
        draw_rounded_rect(ui, sx+8, item_y, sw-16, 28, 4, C_BG_CARD);
        draw_icon(ui, ICON_BACK, sx+12, item_y+4, 16, C_TEXT_DG);
        char title[28]; strncpy(title, history[hist_count-1-i].title, 27); title[27]='\0';
        draw_text(ui, title, sx+32, item_y+7, UI_FONT_SM-2, C_TEXT_DG);
        item_y += 32;
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم إشعار
// ═══════════════════════════════════════════════════════════════
void ui_draw_notification(UIContext* ui, const char* text,
                           float alpha, int type) {
    if (!ui || !text || alpha <= 0) return;
    u8 a = (u8)(alpha * 220);
    int nw = UI_NOTIF_W, nh = UI_NOTIF_H;
    int nx = ui->width/2 - nw/2;
    int ny = ui->height - UI_STATUS_H - nh - 12;
    if (nw > ui->width - 20) { nw = ui->width-20; nx = 10; }

    u32 bg_c  = (C_BG_CARD   & 0xFFFFFF00) | a;
    u32 acc_c;
    switch(type) {
        case 1:  acc_c = (C_YELLOW & 0xFFFFFF00) | a; break;
        case 2:  acc_c = (C_ACCENT & 0xFFFFFF00) | a; break;
        default: acc_c = (C_GREEN  & 0xFFFFFF00) | a; break;
    }

    draw_rounded_rect(ui, nx, ny, nw, nh, 8, bg_c);
    renderer_draw_rect_outline(ui->renderer, nx, ny, nw, nh,
                               (C_BORDER_B & 0xFFFFFF00) | a);
    draw_rect(ui, nx, ny, 4, nh, acc_c);
    draw_glow(ui, nx, ny, nw, nh, (acc_c & 0xFFFFFF00) | 0x22, alpha * 0.5f);

    // أيقونة الإشعار
    IconType ic = (type==1) ? ICON_SETTINGS : (type==2 ? ICON_CLOSE : ICON_BOOKMARK);
    draw_icon(ui, ic, nx+12, ny+nh/2-10, 20, acc_c);
    draw_text(ui, text, nx+40, ny+nh/2-8, UI_FONT_MD,
              (C_TEXT_W & 0xFFFFFF00) | a);
}

// ═══════════════════════════════════════════════════════════════
//  رسم صفحة الإعدادات
// ═══════════════════════════════════════════════════════════════
void ui_draw_settings(UIContext* ui, BrowserSettings* settings) {
    if (!ui || !settings) return;
    int y0 = UI_TAB_H + UI_URL_H;
    draw_rect(ui, 0, y0, ui->width, ui->height-y0-UI_STATUS_H, C_BG_MAIN);

    // رأس الصفحة
    draw_rect(ui, 0, y0, ui->width, 50, C_BG_CARD);
    draw_text(ui, "الإعدادات", 30, y0+15, UI_FONT_XL, C_TEXT_W);
    draw_rect(ui, 0, y0+50, ui->width, 1, C_BORDER_B);
    draw_glow(ui, 0, y0+49, ui->width, 1, C_ACCENT_GLOW, 0.3f);

    // قائمة الإعدادات
    static const struct { const char* label; const char* desc; } items[] = {
        {"جافاسكريبت",     "تشغيل جافاسكريبت في الصفحات"},
        {"الصور",          "تحميل الصور تلقائياً"},
        {"ملفات تعريف الارتباط", "حفظ الكوكيز"},
        {"التخزين المؤقت",  "تخزين الصفحات محلياً"},
        {"حجب الإعلانات",  "إيقاف الإعلانات"},
        {"الوضع الخاص",    "التصفح بدون سجل"},
        {"تسريع الأجهزة",  "استخدام GPU للتصيير"},
        {"حجب النوافذ المنبثقة", "منع النوافذ المنبثقة"},
    };
    int* toggles[] = {
        &settings->javascript_enabled, &settings->images_enabled,
        &settings->cookies_enabled,    &settings->cache_enabled,
        &settings->adblock_enabled,    &settings->private_mode,
        &settings->hardware_accel,     &settings->block_popups,
    };
    int iy = y0 + 60;
    for (int i = 0; i < 8; i++) {
        draw_rounded_rect(ui, 20, iy, ui->width-40, 50, 8, C_BG_CARD);
        draw_text(ui, items[i].label, 30, iy+10, UI_FONT_MD, C_TEXT_W);
        draw_text(ui, items[i].desc,  30, iy+28, UI_FONT_SM, C_TEXT_G);
        // مفتاح التبديل
        int ton = *toggles[i];
        u32 tg_bg = ton ? C_ACCENT : C_BG_HOVER;
        int tx = ui->width - 80, ty2 = iy+14;
        draw_rounded_rect(ui, tx, ty2, 52, 24, 12, tg_bg);
        int knob_x = ton ? tx+30 : tx+4;
        draw_rounded_rect(ui, knob_x, ty2+4, 18, 16, 8, C_TEXT_W);
        if (ton) draw_glow(ui, tx, ty2, 52, 24, C_ACCENT_GLOW, 0.5f);
        renderer_draw_rect_outline(ui->renderer, 20, iy, ui->width-40, 50, C_BORDER);
        iy += 56;
    }

    // تلميح الزر
    draw_text(ui, "دائرة: رجوع  |  X: تبديل", 30, ui->height-UI_STATUS_H-20,
              UI_FONT_SM, C_TEXT_DG);
}

// ═══════════════════════════════════════════════════════════════
//  رسم الإشارات المرجعية
// ═══════════════════════════════════════════════════════════════
void ui_draw_bookmarks(UIContext* ui, Bookmark* bookmarks, int count) {
    if (!ui) return;
    int y0 = UI_TAB_H + UI_URL_H;
    draw_rect(ui, 0, y0, ui->width, ui->height-y0-UI_STATUS_H, C_BG_MAIN);
    draw_rect(ui, 0, y0, ui->width, 50, C_BG_CARD);
    draw_text(ui, "الإشارات المرجعية", 30, y0+15, UI_FONT_XL, C_TEXT_W);
    draw_rect(ui, 0, y0+50, ui->width, 1, C_BORDER_B);

    if (count == 0) {
        draw_text(ui, "لا توجد إشارات مرجعية بعد",
                  ui->width/2-80, y0+120, UI_FONT_MD, C_TEXT_DG);
        return;
    }
    int iy = y0 + 60;
    for (int i = 0; i < count && iy < ui->height-UI_STATUS_H-60; i++) {
        draw_rounded_rect(ui, 20, iy, ui->width-40, 56, 8, C_BG_CARD);
        draw_icon(ui, ICON_BOOKMARK, 30, iy+18, 20, C_ACCENT);
        draw_text(ui, bookmarks[i].title, 58, iy+12, UI_FONT_MD, C_TEXT_W);
        draw_text(ui, bookmarks[i].url,   58, iy+32, UI_FONT_SM, C_TEXT_DG);
        draw_icon(ui, ICON_CLOSE, ui->width-50, iy+18, 20, C_TEXT_DG);
        renderer_draw_rect_outline(ui->renderer, 20, iy, ui->width-40, 56, C_BORDER);
        iy += 62;
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم السجل
// ═══════════════════════════════════════════════════════════════
void ui_draw_history(UIContext* ui, HistoryEntry* history, int count) {
    if (!ui) return;
    int y0 = UI_TAB_H + UI_URL_H;
    draw_rect(ui, 0, y0, ui->width, ui->height-y0-UI_STATUS_H, C_BG_MAIN);
    draw_rect(ui, 0, y0, ui->width, 50, C_BG_CARD);
    draw_text(ui, "سجل التصفح", 30, y0+15, UI_FONT_XL, C_TEXT_W);
    draw_rect(ui, 0, y0+50, ui->width, 1, C_BORDER_B);

    if (count == 0) {
        draw_text(ui, "السجل فارغ",
                  ui->width/2-40, y0+120, UI_FONT_MD, C_TEXT_DG);
        return;
    }
    int iy = y0 + 60;
    for (int i = count-1; i >= 0 && iy < ui->height-UI_STATUS_H-60; i--) {
        draw_rounded_rect(ui, 20, iy, ui->width-40, 52, 6, C_BG_CARD);
        draw_icon(ui, ICON_HISTORY, 30, iy+16, 20, C_TEXT_DG);
        draw_text(ui, history[i].title, 58, iy+10, UI_FONT_MD, C_TEXT_W);
        draw_text(ui, history[i].url,   58, iy+28, UI_FONT_SM, C_TEXT_DG);
        // وقت الزيارة
        char time_str[32];
        struct tm* tm_info = localtime(&history[i].visit_time);
        if (tm_info)
            strftime(time_str, sizeof(time_str), "%H:%M", tm_info);
        else
            snprintf(time_str, sizeof(time_str), "--:--");
        draw_text(ui, time_str, ui->width-80, iy+20, UI_FONT_SM, C_TEXT_DG);
        renderer_draw_rect_outline(ui->renderer, 20, iy, ui->width-40, 52, C_BORDER);
        iy += 58;
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم صفحة الخطأ
// ═══════════════════════════════════════════════════════════════
void ui_draw_error(UIContext* ui, const char* message) {
    if (!ui) return;
    int y0 = UI_TAB_H + UI_URL_H;
    int cx = ui->width/2;
    int cy = y0 + (ui->height-y0-UI_STATUS_H)/2;

    draw_rect(ui, 0, y0, ui->width, ui->height-y0-UI_STATUS_H, C_BG_MAIN);

    // دائرة خطأ
    for (int i=0;i<360;i+=10) {
        float rad=(float)i*3.14159f/180.0f;
        int px=(int)(cx+cosf(rad)*50),py=(int)(cy-60+sinf(rad)*50);
        renderer_set_pixel(ui->renderer,px,py,C_ACCENT);
    }
    // علامة X
    for (int i=-20;i<=20;i++) {
        renderer_set_pixel(ui->renderer,cx+i,cy-60+i,C_ACCENT);
        renderer_set_pixel(ui->renderer,cx+i,cy-60-i,C_ACCENT);
    }
    draw_glow(ui, cx-52, cy-112, 104, 104, C_ACCENT_GLOW, 0.6f);

    draw_text(ui, "تعذّر تحميل الصفحة",
              cx-80, cy+10, UI_FONT_LG, C_TEXT_W);
    if (message)
        draw_text(ui, message, cx-100, cy+38, UI_FONT_SM, C_TEXT_G);
    draw_text(ui, "اضغط R2 للمحاولة مجدداً",
              cx-90, cy+70, UI_FONT_SM, C_TEXT_DG);
    draw_text(ui, "أو دائرة للرجوع",
              cx-55, cy+90, UI_FONT_SM, C_TEXT_DG);
}

// ═══════════════════════════════════════════════════════════════
//  تحديث محتوى الصفحة من HTML
// ═══════════════════════════════════════════════════════════════
void ui_load_html(UIContext* ui, const char* html, size_t len,
                  const char* base_url) {
    if (!ui || !ui->browser_engine || !html) return;
    (void)base_url;
    html_parse(ui->browser_engine, html, len);
    BROWSER_LOG("تم تحميل %zu بايت من HTML", len);
}
