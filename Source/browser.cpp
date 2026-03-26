// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - browser.cpp
//  المنطق الأساسي والتحكم في دورة حياة المتصفح
// ═══════════════════════════════════════════════════════════════

#include "browser.h"
#include "network.h"
#include "renderer.h"
#include "ui.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

// إعدادات المتصفح الافتراضية
#define BROWSER_HOME_URL "https://google.com" 

void browser_init(BrowserContext* ctx) {
    if (!ctx) return;
    
    mem_zero(ctx, sizeof(BrowserContext));
    
    // تهيئة الأنظمة الفرعية
    network_init(&ctx->net);
    renderer_init(&ctx->renderer);
    ui_init(&ctx->ui, &ctx->renderer);
    
    // إعداد حالة المتصفح
    ctx->is_running = 1;
    ctx->zoom_level = 1.0f;
    strncpy(ctx->current_url, BROWSER_HOME_URL, NET_MAX_URL_LEN);
    
    // تحميل الصفحة الرئيسية عند التشغيل
    browser_load_url(ctx, ctx->current_url);
}

void browser_load_url(BrowserContext* ctx, const char* url) {
    if (!ctx || !url) return;
    
    // تحديث الحالة للواجهة
    ctx->is_loading = 1;
    ctx->load_progress = 0.1f;
    strncpy(ctx->current_url, url, NET_MAX_URL_LEN);

    // بناء رابط البروكسي (Render.com) لجلب الصفحة معالجة جاهزة
    char proxy_url[NET_MAX_URL_LEN];
    url_build_proxy(&ctx->net, url, proxy_url, sizeof(proxy_url));

    // بدء طلب الشبكة
    network_get(&ctx->net, proxy_url, NULL, ctx);
}

void browser_update(BrowserContext* ctx) {
    if (!ctx || !ctx->is_running) return;

    // تحديث حالة الشبكة والتحقق من وصول البيانات
    network_update(&ctx->net);
    
    // إذا انتهى التحميل، نقوم بتحديث الحالة
    if (ctx->is_loading && network_check_online(&ctx->net)) {
        // هنا يتم معالجة البيانات القادمة من Render.com
        // لمحاكاة التقدم في الواجهة
        if (ctx->load_progress < 1.0f) {
            ctx->load_progress += 0.05f;
        } else {
            ctx->is_loading = 0;
        }
    }
}

void browser_render(BrowserContext* ctx) {
    if (!ctx) return;

    // 1. رسم الخلفية (النيون)
    ui_draw_background(&ctx->ui);

    // 2. رسم محتوى الصفحة (إذا تم تحميله)
    if (!ctx->is_loading) {
        // هنا يستدعي المتصفح الـ Renderer لرسم العناصر
        // renderer_draw_page(ctx->renderer, ctx->page_content);
    }

    // 3. رسم واجهة المستخدم فوق كل شيء
    ui_draw_address_bar(&ctx->ui, ctx->current_url, 0);
    
    if (ctx->is_loading) {
        ui_draw_status_bar(&ctx->ui, "جارٍ التحميل عبر Alout Server...", ctx->load_progress);
    }

    // تحديث الشاشة
    renderer_flip(&ctx->renderer);
}

void browser_shutdown(BrowserContext* ctx) {
    if (!ctx) return;
    
    network_cleanup(&ctx->net);
    renderer_shutdown(&ctx->renderer);
    ctx->is_running = 0;
}

// أدوات مساعدة للنصوص (المطلوبة في browser.h)
void str_to_lower(char* str) {
    for (; *str; ++str) {
        if (*str >= 'A' && *str <= 'Z') *str += 32;
    }
}

int str_starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}