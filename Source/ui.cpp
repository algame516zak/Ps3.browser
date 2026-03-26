// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - ui.cpp
//  تنفيذ واجهة المستخدم وتصميم النيون
// ═══════════════════════════════════════════════════════════════

#include "ui.h"
#include "renderer.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>

// ألوان Alout الرسمية (نيون)
#define ALOUT_BLUE   RGBA(0, 243, 255, 255)
#define ALOUT_GREEN  RGBA(57, 255, 20, 255)
#define ALOUT_BG     RGBA(10, 10, 15, 230)

void ui_init(UIContext* ui, RendererContext* renderer) {
    if (!ui || !renderer) return;
    memset(ui, 0, sizeof(UIContext));
    ui->renderer = renderer;
    ui->show_address_bar = 1;
    ui->accent_color = ALOUT_BLUE;
    
    // إعداد واجهة النيون الافتراضية
    ui->root_style.bg_color = ALOUT_BG;
    ui->root_style.border_color = ALOUT_BLUE;
    ui->root_style.border_width = 2.0f;
    ui->root_style.border_radius = UI_CORNER_RADIUS;
}

void ui_draw_background(UIContext* ui) {
    if (!ui) return;
    // رسم الخلفية العميقة للمتصفح
    renderer_draw_rect_filled(ui->renderer, 0, 0, 
                             ui->renderer->screen_w, 
                             ui->renderer->screen_h, 
                             ALOUT_BG);
                             
    // إضافة تأثير "توهج" نيون بسيط في الأعلى
    renderer_draw_rect_filled(ui->renderer, 0, 0, 
                             ui->renderer->screen_w, 4, 
                             ALOUT_BLUE);
}

void ui_draw_address_bar(UIContext* ui, const char* url, int is_focused) {
    if (!ui || !ui->show_address_bar) return;

    f32 x = 40.0f;
    f32 y = 30.0f;
    f32 w = ui->renderer->screen_w - 80.0f;
    f32 h = 45.0f;

    // رسم شريط العنوان بتأثير النيون
    Color border = is_focused ? ALOUT_GREEN : ALOUT_BLUE;
    
    renderer_draw_rect_filled_rounded(ui->renderer, x, y, w, h, 
                                     UI_CORNER_RADIUS, RGBA(20, 20, 30, 255));
    
    renderer_draw_rect_border_rounded(ui->renderer, x, y, w, h, 
                                     UI_CORNER_RADIUS, 2.0f, border);

    // توهج (Glow) بسيط حول الشريط
    if (is_focused) {
        renderer_push_opacity(ui->renderer, 0.3f);
        renderer_draw_rect_border_rounded(ui->renderer, x-2, y-2, w+4, h+4, 
                                         UI_CORNER_RADIUS+2, 4.0f, ALOUT_GREEN);
        renderer_pop_opacity(ui->renderer);
    }

    // كتابة الرابط (يفترض وجود خط محمل)
    // renderer_draw_text(ui->renderer, x + 15, y + 12, url, 18.0f, COLOR_WHITE);
}

void ui_draw_status_bar(UIContext* ui, const char* status, f32 progress) {
    if (!ui) return;
    
    f32 h = 25.0f;
    f32 y = ui->renderer->screen_h - h;
    
    renderer_draw_rect_filled(ui->renderer, 0, y, ui->renderer->screen_w, h, RGBA(5, 5, 10, 255));
    
    // شريط التقدم (Progress Bar) نيون أخضر
    if (progress > 0.0f && progress < 1.0f) {
        renderer_draw_rect_filled(ui->renderer, 0, y, 
                                 ui->renderer->screen_w * progress, 2, 
                                 ALOUT_GREEN);
    }
}

// تنفيذ أدوات المساعدة المتوافقة مع ui.h
void ui_format_size(size_t bytes, char* out, size_t out_len) {
    if (bytes < 1024) snprintf(out, out_len, "%zu B", bytes);
    else if (bytes < 1024 * 1024) snprintf(out, out_len, "%.2f KB", bytes / 1024.0f);
    else snprintf(out, out_len, "%.2f MB", bytes / (1024.0f * 1024.0f));
}

Color ui_get_accent_color(UIContext* ui) {
    return ui ? ui->accent_color : ALOUT_BLUE;
}