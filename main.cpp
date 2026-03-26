#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <psl1ght/lv2.h>
#include <psl1ght/lv2/memory.h>
#include <psl1ght/lv2/thread.h>
#include <psl1ght/lv2/mutex.h>
#include <psl1ght/lv2/timer.h>

#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <rsx/reality.h>

#include <io/pad.h>
#include <io/kb.h>

#include <net/net.h>
#include <net/netctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sysutil/sysutil.h>
#include <sysutil/video.h>

#include "browser.h"
#include "renderer.h"
#include "network.h"
#include "memory.h"
#include "ui.h"

// ═══════════════════════════════════════════════════════════════
//  ثوابت النظام
// ═══════════════════════════════════════════════════════════════
#define PS3_BROWSER_VERSION        "1.0.0"
#define PS3_BROWSER_NAME           "PS3 UltraBrowser"
#define SERVER_URL                 "https://ps3browser.onrender.com"

#define FRAME_WIDTH                1280
#define FRAME_HEIGHT               720
#define FRAME_DEPTH                4
#define FRAME_COUNT                2

#define MEMORY_POOL_SIZE           (180 * 1024 * 1024)   // 180 ميغابايت للمتصفح
#define RENDER_POOL_SIZE           (40  * 1024 * 1024)   // 40 ميغابايت للرسم
#define NETWORK_POOL_SIZE          (20  * 1024 * 1024)   // 20 ميغابايت للشبكة
#define CACHE_POOL_SIZE            (16  * 1024 * 1024)   // 16 ميغابايت للكاش

#define MAX_URL_LENGTH             2048
#define MAX_HISTORY_SIZE           50
#define MAX_TABS                   8
#define MAX_BOOKMARKS              100
#define TAB_BAR_HEIGHT             48
#define URL_BAR_HEIGHT             52
#define STATUS_BAR_HEIGHT          28
#define SIDEBAR_WIDTH              280

#define TARGET_FPS                 30
#define FRAME_TIME_MS              (1000 / TARGET_FPS)

// ═══════════════════════════════════════════════════════════════
//  ألوان واجهة أوبرا جي إكس
// ═══════════════════════════════════════════════════════════════
#define COLOR_BG_PRIMARY           0x0D0D0DFF   // خلفية رئيسية سوداء
#define COLOR_BG_SECONDARY         0x1A1A1AFF   // خلفية ثانوية
#define COLOR_BG_TERTIARY          0x242424FF   // خلفية ثالثية
#define COLOR_ACCENT_RED           0xFF1A1AFF   // أحمر أوبرا
#define COLOR_ACCENT_PINK          0xFF006EFF   // وردي نيون
#define COLOR_ACCENT_CYAN          0x00E5FFFF   // سماوي نيون
#define COLOR_ACCENT_PURPLE        0x9D00FFFF   // بنفسجي نيون
#define COLOR_TEXT_PRIMARY         0xFFFFFFFF   // نص أبيض
#define COLOR_TEXT_SECONDARY       0xAAAAAFFF   // نص رمادي
#define COLOR_TEXT_ACCENT          0xFF4D4DFF   // نص أحمر
#define COLOR_TAB_ACTIVE           0x1E1E2EFF   // تبويب نشط
#define COLOR_TAB_INACTIVE         0x12121AFF   // تبويب غير نشط
#define COLOR_TAB_HOVER            0x16162AFF   // تبويب عند المرور
#define COLOR_URL_BAR              0x0A0A14FF   // شريط العنوان
#define COLOR_URL_BAR_FOCUS        0x14142AFF   // شريط العنوان عند التركيز
#define COLOR_BORDER               0xFF1A1A44   // حدود
#define COLOR_BORDER_ACCENT        0xFF1A1AFF   // حدود مضيئة
#define COLOR_GLOW_RED             0xFF000088   // توهج أحمر
#define COLOR_SCROLL_BAR           0xFF1A1A88   // شريط التمرير
#define COLOR_BUTTON_HOVER         0xFF1A1A22   // زر عند المرور
#define COLOR_LOADING_BAR          0xFF1A1AFF   // شريط التحميل
#define COLOR_SHADOW               0x00000099   // ظل

// ═══════════════════════════════════════════════════════════════
//  حالات المتصفح
// ═══════════════════════════════════════════════════════════════
typedef enum {
    BROWSER_STATE_SPLASH,        // شاشة البداية
    BROWSER_STATE_HOME,          // الصفحة الرئيسية
    BROWSER_STATE_LOADING,       // جاري التحميل
    BROWSER_STATE_BROWSING,      // التصفح
    BROWSER_STATE_URL_INPUT,     // إدخال عنوان
    BROWSER_STATE_SETTINGS,      // الإعدادات
    BROWSER_STATE_BOOKMARKS,     // الإشارات المرجعية
    BROWSER_STATE_HISTORY,       // السجل
    BROWSER_STATE_DOWNLOADS,     // التنزيلات
    BROWSER_STATE_ERROR,         // خطأ
    BROWSER_STATE_MENU,          // القائمة الجانبية
    BROWSER_STATE_EXIT           // الخروج
} BrowserState;

// ═══════════════════════════════════════════════════════════════
//  هيكل التبويب
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char     url[MAX_URL_LENGTH];
    char     title[256];
    char     favicon[64];
    int      scroll_x;
    int      scroll_y;
    int      zoom_level;
    int      is_loading;
    int      is_secure;
    float    load_progress;
    u32      background_color;
    int      active;
    time_t   last_visited;
    void*    page_cache;
    size_t   page_cache_size;
} BrowserTab;

// ═══════════════════════════════════════════════════════════════
//  هيكل السجل
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    url[MAX_URL_LENGTH];
    char    title[256];
    time_t  visit_time;
    int     visit_count;
} HistoryEntry;

// ═══════════════════════════════════════════════════════════════
//  هيكل الإشارة المرجعية
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    url[MAX_URL_LENGTH];
    char    title[256];
    char    folder[64];
    time_t  created_at;
} Bookmark;

// ═══════════════════════════════════════════════════════════════
//  هيكل الإعدادات
// ═══════════════════════════════════════════════════════════════
typedef struct {
    int     javascript_enabled;
    int     images_enabled;
    int     cookies_enabled;
    int     cache_enabled;
    int     adblock_enabled;
    int     private_mode;
    int     dark_mode;
    int     zoom_default;
    int     scroll_speed;
    int     animation_speed;
    char    homepage[MAX_URL_LENGTH];
    char    search_engine[MAX_URL_LENGTH];
    char    user_agent[512];
    int     hardware_accel;
    int     preload_pages;
    int     max_cache_size;
    int     font_size;
    int     show_images;
    int     block_popups;
    int     notify_downloads;
    u32     accent_color;
    int     glow_effect;
    int     blur_effect;
    int     transparency_level;
} BrowserSettings;

// ═══════════════════════════════════════════════════════════════
//  هيكل الحالة الرئيسية للمتصفح
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // النظام
    gcmContextData*     rsx_context;
    VideoResolution     resolution;
    u32*                frame_buffer[FRAME_COUNT];
    u32                 current_frame;
    u32                 frame_count;
    
    // الحالة
    BrowserState        state;
    BrowserState        prev_state;
    int                 running;
    float               delta_time;
    u64                 last_frame_time;
    u64                 fps_counter;
    float               current_fps;
    
    // التبويبات
    BrowserTab          tabs[MAX_TABS];
    int                 active_tab;
    int                 tab_count;
    int                 tab_bar_visible;
    float               tab_scroll_offset;
    
    // التنقل
    char                url_buffer[MAX_URL_LENGTH];
    int                 url_cursor_pos;
    int                 url_input_active;
    
    // السجل والإشارات
    HistoryEntry        history[MAX_HISTORY_SIZE];
    int                 history_count;
    Bookmark            bookmarks[MAX_BOOKMARKS];
    int                 bookmark_count;
    
    // الإعدادات
    BrowserSettings     settings;
    
    // الشبكة
    NetworkContext*     net_ctx;
    int                 network_available;
    int                 connection_speed;
    
    // الذاكرة
    MemoryPool*         main_pool;
    MemoryPool*         render_pool;
    MemoryPool*         network_pool;
    MemoryPool*         cache_pool;
    
    // واجهة المستخدم
    UIContext*          ui_ctx;
    RendererContext*    renderer;
    
    // التحكم
    PadData             pad_data;
    PadData             prev_pad_data;
    int                 pad_connected;
    
    // الرسوم المتحركة
    float               splash_alpha;
    float               sidebar_offset;
    float               url_bar_glow;
    float               loading_angle;
    float               tab_anim_offset;
    float               menu_slide;
    float               notification_alpha;
    
    // الإحصائيات
    size_t              total_data_used;
    int                 pages_visited;
    float               avg_load_time;
    u64                 session_start;
    
    // الخيط الثاني للشبكة
    sys_ppu_thread_t    network_thread;
    sys_mutex_t         network_mutex;
    int                 network_thread_running;
    
    // الإشعارات
    char                notification_text[256];
    float               notification_timer;
    int                 notification_type;
    
} BrowserContext;

// ═══════════════════════════════════════════════════════════════
//  متغيرات عامة
// ═══════════════════════════════════════════════════════════════
static BrowserContext   g_browser;
static volatile int     g_sys_callback_exit = 0;

// ═══════════════════════════════════════════════════════════════
//  نماذج الدوال
// ═══════════════════════════════════════════════════════════════
static int  browser_init(BrowserContext* ctx);
static void browser_shutdown(BrowserContext* ctx);
static void browser_main_loop(BrowserContext* ctx);
static void browser_update(BrowserContext* ctx);
static void browser_render(BrowserContext* ctx);
static void browser_handle_input(BrowserContext* ctx);
static void browser_navigate(BrowserContext* ctx, const char* url);
static void browser_new_tab(BrowserContext* ctx);
static void browser_close_tab(BrowserContext* ctx, int tab_index);
static void browser_switch_tab(BrowserContext* ctx, int tab_index);
static void browser_go_back(BrowserContext* ctx);
static void browser_go_forward(BrowserContext* ctx);
static void browser_refresh(BrowserContext* ctx);
static void browser_show_notification(BrowserContext* ctx, const char* text, int type);
static void browser_save_history(BrowserContext* ctx);
static void browser_load_history(BrowserContext* ctx);
static void browser_save_bookmarks(BrowserContext* ctx);
static void browser_load_bookmarks(BrowserContext* ctx);
static void browser_save_settings(BrowserContext* ctx);
static void browser_load_settings(BrowserContext* ctx);
static void browser_set_default_settings(BrowserContext* ctx);
static void network_thread_func(u64 arg);
static void sys_callback(u64 status, u64 param, void* userdata);
static int  init_video(BrowserContext* ctx);
static int  init_rsx(BrowserContext* ctx);
static int  init_network_system(BrowserContext* ctx);
static int  init_input(BrowserContext* ctx);
static int  init_memory_pools(BrowserContext* ctx);
static void flip_frame(BrowserContext* ctx);
static u64  get_time_ms(void);
static float calculate_fps(BrowserContext* ctx);

// ═══════════════════════════════════════════════════════════════
//  دالة رد نداء النظام
// ═══════════════════════════════════════════════════════════════
static void sys_callback(u64 status, u64 param, void* userdata) {
    (void)param;
    (void)userdata;
    switch (status) {
        case SYSUTIL_EXIT_GAME:
            g_sys_callback_exit = 1;
            break;
        case SYSUTIL_DRAW_BEGIN:
        case SYSUTIL_DRAW_END:
            break;
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════
//  الحصول على الوقت بالميلي ثانية
// ═══════════════════════════════════════════════════════════════
static u64 get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

// ═══════════════════════════════════════════════════════════════
//  حساب معدل الإطارات
// ═══════════════════════════════════════════════════════════════
static float calculate_fps(BrowserContext* ctx) {
    static u64  fps_time    = 0;
    static u64  fps_frames  = 0;
    static float fps_result = 0.0f;
    u64 now = get_time_ms();
    fps_frames++;
    if (now - fps_time >= 1000) {
        fps_result = (float)fps_frames * 1000.0f / (float)(now - fps_time);
        fps_frames = 0;
        fps_time   = now;
    }
    ctx->current_fps = fps_result;
    return fps_result;
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة الفيديو
// ═══════════════════════════════════════════════════════════════
static int init_video(BrowserContext* ctx) {
    VideoState vstate;
    if (videoGetState(0, 0, &vstate) != 0) return -1;
    if (vstate.state != 0) return -2;

    VideoResolution res;
    if (videoGetResolution(vstate.displayMode.resolution, &res) != 0) return -3;

    ctx->resolution.width  = res.width;
    ctx->resolution.height = res.height;

    VideoConfiguration vcfg;
    memset(&vcfg, 0, sizeof(VideoConfiguration));
    vcfg.resolution = vstate.displayMode.resolution;
    vcfg.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vcfg.pitch      = res.width * FRAME_DEPTH;
    vcfg.aspect     = VIDEO_ASPECT_AUTO;

    if (videoConfigure(0, &vcfg, NULL, 0) != 0) return -4;
    if (videoGetState(0, 0, &vstate) != 0)      return -5;

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة وحدة معالجة الرسومات RSX
// ═══════════════════════════════════════════════════════════════
static int init_rsx(BrowserContext* ctx) {
    void* host_addr = memalign(0x100000, RENDER_POOL_SIZE);
    if (!host_addr) return -1;

    ctx->rsx_context = rsxInit(0x10000, RENDER_POOL_SIZE, host_addr);
    if (!ctx->rsx_context) { free(host_addr); return -2; }

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    u32 width  = ctx->resolution.width;
    u32 height = ctx->resolution.height;
    u32 pitch  = width * FRAME_DEPTH;
    u32 size   = pitch * height;

    for (int i = 0; i < FRAME_COUNT; i++) {
        rsxAddressToOffset(&ctx->frame_buffer[i], NULL);
        ctx->frame_buffer[i] = (u32*)rsxMemalign(0x100000, size);
        if (!ctx->frame_buffer[i]) return -3;

        u32 offset;
        rsxAddressToOffset(ctx->frame_buffer[i], &offset);
        gcmSetDisplayBuffer(i, offset, pitch, width, height);
    }

    ctx->current_frame = 0;
    ctx->frame_count   = 0;
    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  تبديل الإطار
// ═══════════════════════════════════════════════════════════════
static void flip_frame(BrowserContext* ctx) {
    gcmSetFlip(ctx->rsx_context, ctx->current_frame);
    rsxFlushBuffer(ctx->rsx_context);
    gcmSetWaitFlip(ctx->rsx_context);
    ctx->current_frame = (ctx->current_frame + 1) % FRAME_COUNT;
    ctx->frame_count++;
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة نظام الشبكة
// ═══════════════════════════════════════════════════════════════
static int init_network_system(BrowserContext* ctx) {
    if (netInitialize() != 0) return -1;

    NetCtlInfo info;
    if (netCtlGetInfo(NET_CTL_INFO_IP_ADDRESS, &info) != 0) {
        ctx->network_available = 0;
        return 0;
    }

    ctx->network_available = 1;
    ctx->net_ctx = network_init(SERVER_URL, ctx->network_pool);
    if (!ctx->net_ctx) return -2;

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة الإدخال
// ═══════════════════════════════════════════════════════════════
static int init_input(BrowserContext* ctx) {
    if (ioPadInit(7) != 0) return -1;
    memset(&ctx->pad_data,      0, sizeof(PadData));
    memset(&ctx->prev_pad_data, 0, sizeof(PadData));
    ctx->pad_connected = 1;
    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة مجمعات الذاكرة
// ═══════════════════════════════════════════════════════════════
static int init_memory_pools(BrowserContext* ctx) {
    ctx->main_pool    = memory_pool_create(MEMORY_POOL_SIZE);
    ctx->render_pool  = memory_pool_create(RENDER_POOL_SIZE);
    ctx->network_pool = memory_pool_create(NETWORK_POOL_SIZE);
    ctx->cache_pool   = memory_pool_create(CACHE_POOL_SIZE);

    if (!ctx->main_pool || !ctx->render_pool ||
        !ctx->network_pool || !ctx->cache_pool) return -1;

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  الإعدادات الافتراضية
// ═══════════════════════════════════════════════════════════════
static void browser_set_default_settings(BrowserContext* ctx) {
    BrowserSettings* s = &ctx->settings;
    s->javascript_enabled  = 1;
    s->images_enabled      = 1;
    s->cookies_enabled     = 1;
    s->cache_enabled       = 1;
    s->adblock_enabled     = 1;
    s->private_mode        = 0;
    s->dark_mode           = 1;
    s->zoom_default        = 100;
    s->scroll_speed        = 8;
    s->animation_speed     = 5;
    s->hardware_accel      = 1;
    s->preload_pages       = 0;
    s->max_cache_size      = 50;
    s->font_size           = 16;
    s->show_images         = 1;
    s->block_popups        = 1;
    s->notify_downloads    = 1;
    s->accent_color        = COLOR_ACCENT_RED;
    s->glow_effect         = 1;
    s->blur_effect         = 1;
    s->transparency_level  = 85;
    snprintf(s->homepage,       MAX_URL_LENGTH, "ps3://home");
    snprintf(s->search_engine,  MAX_URL_LENGTH, "https://www.google.com/search?q=");
    snprintf(s->user_agent,     512,
        "Mozilla/5.0 (PLAYSTATION 3 4.90) PS3Browser/1.0");
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة المتصفح الرئيسية
// ═══════════════════════════════════════════════════════════════
static int browser_init(BrowserContext* ctx) {
    memset(ctx, 0, sizeof(BrowserContext));

    // تسجيل رد نداء النظام
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

    // تهيئة مجمعات الذاكرة أولاً
    if (init_memory_pools(ctx) != 0) {
        printf("[خطأ] فشل في تهيئة الذاكرة\n");
        return -1;
    }

    // تهيئة الفيديو
    if (init_video(ctx) != 0) {
        printf("[خطأ] فشل في تهيئة الفيديو\n");
        return -2;
    }

    // تهيئة وحدة الرسومات
    if (init_rsx(ctx) != 0) {
        printf("[خطأ] فشل في تهيئة وحدة الرسومات\n");
        return -3;
    }

    // تهيئة المُصيِّر
    ctx->renderer = renderer_init(ctx->rsx_context, ctx->render_pool,
                                  ctx->resolution.width, ctx->resolution.height);
    if (!ctx->renderer) {
        printf("[خطأ] فشل في تهيئة المُصيِّر\n");
        return -4;
    }

    // تهيئة واجهة المستخدم
    ctx->ui_ctx = ui_init(ctx->renderer, ctx->main_pool,
                          ctx->resolution.width, ctx->resolution.height);
    if (!ctx->ui_ctx) {
        printf("[خطأ] فشل في تهيئة واجهة المستخدم\n");
        return -5;
    }

    // تهيئة الشبكة
    if (init_network_system(ctx) != 0) {
        printf("[تحذير] الشبكة غير متاحة\n");
    }

    // تهيئة الإدخال
    if (init_input(ctx) != 0) {
        printf("[خطأ] فشل في تهيئة الإدخال\n");
        return -6;
    }

    // الإعدادات الافتراضية
    browser_set_default_settings(ctx);
    browser_load_settings(ctx);
    browser_load_history(ctx);
    browser_load_bookmarks(ctx);

    // تهيئة التبويب الأول
    ctx->tab_count   = 1;
    ctx->active_tab  = 0;
    memset(&ctx->tabs[0], 0, sizeof(BrowserTab));
    snprintf(ctx->tabs[0].url,   MAX_URL_LENGTH, "ps3://home");
    snprintf(ctx->tabs[0].title, 256, "الصفحة الرئيسية");
    ctx->tabs[0].active     = 1;
    ctx->tabs[0].zoom_level = ctx->settings.zoom_default;

    // تهيئة خيط الشبكة
    sys_mutex_attribute_t mutex_attr;
    sysMutexAttrInitialize(mutex_attr);
    sysMutexCreate(&ctx->network_mutex, &mutex_attr);
    ctx->network_thread_running = 1;
    sysThreadCreate(&ctx->network_thread, network_thread_func,
                    (u64)(uintptr_t)ctx, 1000, 0x10000, 0, "net_thread");

    // حالة البداية
    ctx->state        = BROWSER_STATE_SPLASH;
    ctx->running      = 1;
    ctx->splash_alpha = 0.0f;
    ctx->session_start= get_time_ms();
    ctx->last_frame_time = get_time_ms();

    printf("[نجاح] تم تهيئة %s v%s\n", PS3_BROWSER_NAME, PS3_BROWSER_VERSION);
    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  خيط الشبكة
// ═══════════════════════════════════════════════════════════════
static void network_thread_func(u64 arg) {
    BrowserContext* ctx = (BrowserContext*)(uintptr_t)arg;
    while (ctx->network_thread_running) {
        sysMutexLock(ctx->network_mutex, 0);
        // معالجة طلبات الشبكة المعلقة
        if (ctx->network_available && ctx->net_ctx) {
            network_process_queue(ctx->net_ctx);
        }
        sysMutexUnlock(ctx->network_mutex);
        usleep(16000); // 16 ميلي ثانية
    }
    sysThreadExit(0);
}

// ═══════════════════════════════════════════════════════════════
//  التنقل إلى عنوان
// ═══════════════════════════════════════════════════════════════
static void browser_navigate(BrowserContext* ctx, const char* url) {
    if (!url || strlen(url) == 0) return;

    BrowserTab* tab = &ctx->tabs[ctx->active_tab];

    // حفظ في السجل
    if (ctx->history_count < MAX_HISTORY_SIZE) {
        HistoryEntry* entry = &ctx->history[ctx->history_count++];
        strncpy(entry->url,   tab->url,   MAX_URL_LENGTH - 1);
        strncpy(entry->title, tab->title, 255);
        entry->visit_time  = time(NULL);
        entry->visit_count = 1;
    }

    // تعيين العنوان الجديد
    strncpy(tab->url, url, MAX_URL_LENGTH - 1);
    snprintf(tab->title, 256, "جاري التحميل...");
    tab->is_loading   = 1;
    tab->load_progress= 0.0f;
    tab->scroll_x     = 0;
    tab->scroll_y     = 0;

    ctx->state        = BROWSER_STATE_LOADING;
    ctx->pages_visited++;

    // إرسال طلب الشبكة
    if (ctx->network_available && ctx->net_ctx) {
        sysMutexLock(ctx->network_mutex, 0);
        network_fetch_url(ctx->net_ctx, url, tab);
        sysMutexUnlock(ctx->network_mutex);
    }

    browser_show_notification(ctx, "جاري التحميل...", 0);
}

// ═══════════════════════════════════════════════════════════════
//  فتح تبويب جديد
// ═══════════════════════════════════════════════════════════════
static void browser_new_tab(BrowserContext* ctx) {
    if (ctx->tab_count >= MAX_TABS) {
        browser_show_notification(ctx, "الحد الأقصى للتبويبات هو 8", 1);
        return;
    }
    int idx = ctx->tab_count++;
    memset(&ctx->tabs[idx], 0, sizeof(BrowserTab));
    snprintf(ctx->tabs[idx].url,   MAX_URL_LENGTH, "ps3://home");
    snprintf(ctx->tabs[idx].title, 256, "تبويب جديد");
    ctx->tabs[idx].zoom_level = ctx->settings.zoom_default;
    ctx->tabs[idx].active     = 1;
    browser_switch_tab(ctx, idx);
    browser_show_notification(ctx, "تم فتح تبويب جديد", 0);
}

// ═══════════════════════════════════════════════════════════════
//  إغلاق تبويب
// ═══════════════════════════════════════════════════════════════
static void browser_close_tab(BrowserContext* ctx, int idx) {
    if (ctx->tab_count <= 1) {
        ctx->state = BROWSER_STATE_EXIT;
        return;
    }
    if (idx < 0 || idx >= ctx->tab_count) return;

    // تحرير كاش الصفحة
    if (ctx->tabs[idx].page_cache) {
        memory_pool_free(ctx->cache_pool, ctx->tabs[idx].page_cache);
        ctx->tabs[idx].page_cache      = NULL;
        ctx->tabs[idx].page_cache_size = 0;
    }

    // إزاحة التبويبات
    for (int i = idx; i < ctx->tab_count - 1; i++) {
        ctx->tabs[i] = ctx->tabs[i + 1];
    }
    ctx->tab_count--;

    if (ctx->active_tab >= ctx->tab_count) {
        ctx->active_tab = ctx->tab_count - 1;
    }
}

// ═══════════════════════════════════════════════════════════════
//  تبديل التبويب
// ═══════════════════════════════════════════════════════════════
static void browser_switch_tab(BrowserContext* ctx, int idx) {
    if (idx < 0 || idx >= ctx->tab_count) return;
    ctx->active_tab = idx;
    BrowserTab* tab = &ctx->tabs[idx];
    if (tab->is_loading) {
        ctx->state = BROWSER_STATE_LOADING;
    } else if (strncmp(tab->url, "ps3://home", 10) == 0) {
        ctx->state = BROWSER_STATE_HOME;
    } else {
        ctx->state = BROWSER_STATE_BROWSING;
    }
}

// ═══════════════════════════════════════════════════════════════
//  الرجوع للخلف
// ═══════════════════════════════════════════════════════════════
static void browser_go_back(BrowserContext* ctx) {
    if (ctx->history_count > 0) {
        ctx->history_count--;
        HistoryEntry* entry = &ctx->history[ctx->history_count];
        browser_navigate(ctx, entry->url);
    }
}

// ═══════════════════════════════════════════════════════════════
//  الرجوع للأمام
// ═══════════════════════════════════════════════════════════════
static void browser_go_forward(BrowserContext* ctx) {
    browser_show_notification(ctx, "لا يوجد صفحة للأمام", 1);
}

// ═══════════════════════════════════════════════════════════════
//  تحديث الصفحة
// ═══════════════════════════════════════════════════════════════
static void browser_refresh(BrowserContext* ctx) {
    BrowserTab* tab = &ctx->tabs[ctx->active_tab];
    browser_navigate(ctx, tab->url);
}

// ═══════════════════════════════════════════════════════════════
//  عرض إشعار
// ═══════════════════════════════════════════════════════════════
static void browser_show_notification(BrowserContext* ctx, const char* text, int type) {
    strncpy(ctx->notification_text, text, 255);
    ctx->notification_timer = 3.0f;
    ctx->notification_type  = type;
    ctx->notification_alpha = 1.0f;
}

// ═══════════════════════════════════════════════════════════════
//  معالجة الإدخال
// ═══════════════════════════════════════════════════════════════
static void browser_handle_input(BrowserContext* ctx) {
    ctx->prev_pad_data = ctx->pad_data;
    ioPadGetData(0, &ctx->pad_data);

    if (!ctx->pad_data.len) return;

    u32 pressed  = ctx->pad_data.button[2]  | (ctx->pad_data.button[3]  << 8);
    u32 prev     = ctx->prev_pad_data.button[2] | (ctx->prev_pad_data.button[3] << 8);
    u32 just_pressed = pressed & ~prev;

    BrowserTab* tab = &ctx->tabs[ctx->active_tab];

    // زر المثلث: فتح شريط العنوان
    if (just_pressed & PAD_TRIANGLE) {
        if (ctx->state != BROWSER_STATE_URL_INPUT) {
            ctx->state = BROWSER_STATE_URL_INPUT;
            strncpy(ctx->url_buffer, tab->url, MAX_URL_LENGTH - 1);
            ctx->url_cursor_pos = strlen(ctx->url_buffer);
            ctx->url_input_active = 1;
        } else {
            ctx->url_input_active = 0;
            ctx->state = BROWSER_STATE_BROWSING;
        }
    }

    // زر الدائرة: الرجوع
    if (just_pressed & PAD_CIRCLE) {
        if (ctx->state == BROWSER_STATE_URL_INPUT) {
            ctx->url_input_active = 0;
            ctx->state = BROWSER_STATE_BROWSING;
        } else if (ctx->state == BROWSER_STATE_SETTINGS  ||
                   ctx->state == BROWSER_STATE_BOOKMARKS ||
                   ctx->state == BROWSER_STATE_HISTORY   ||
                   ctx->state == BROWSER_STATE_MENU) {
            ctx->state = BROWSER_STATE_BROWSING;
        } else {
            browser_go_back(ctx);
        }
    }

    // زر الصليب: تأكيد
    if (just_pressed & PAD_CROSS) {
        if (ctx->state == BROWSER_STATE_URL_INPUT) {
            browser_navigate(ctx, ctx->url_buffer);
            ctx->url_input_active = 0;
        }
    }

    // زر المربع: تبويب جديد
    if (just_pressed & PAD_SQUARE) {
        browser_new_tab(ctx);
    }

    // L1: تبويب يسار
    if (just_pressed & PAD_L1) {
        int next = ctx->active_tab - 1;
        if (next < 0) next = ctx->tab_count - 1;
        browser_switch_tab(ctx, next);
    }

    // R1: تبويب يمين
    if (just_pressed & PAD_R1) {
        int next = (ctx->active_tab + 1) % ctx->tab_count;
        browser_switch_tab(ctx, next);
    }

    // L2: إغلاق التبويب الحالي
    if (just_pressed & PAD_L2) {
        browser_close_tab(ctx, ctx->active_tab);
    }

    // R2: تحديث الصفحة
    if (just_pressed & PAD_R2) {
        browser_refresh(ctx);
    }

    // زر Start: القائمة
    if (just_pressed & PAD_START) {
        if (ctx->state == BROWSER_STATE_MENU) {
            ctx->state = BROWSER_STATE_BROWSING;
        } else {
            ctx->state = BROWSER_STATE_MENU;
        }
    }

    // زر Select: الإعدادات
    if (just_pressed & PAD_SELECT) {
        ctx->state = BROWSER_STATE_SETTINGS;
    }

    // السهام: التمرير
    if (pressed & PAD_UP)    tab->scroll_y -= ctx->settings.scroll_speed * 4;
    if (pressed & PAD_DOWN)  tab->scroll_y += ctx->settings.scroll_speed * 4;
    if (pressed & PAD_LEFT)  tab->scroll_x -= ctx->settings.scroll_speed * 4;
    if (pressed & PAD_RIGHT) tab->scroll_x += ctx->settings.scroll_speed * 4;

    if (tab->scroll_y < 0) tab->scroll_y = 0;
    if (tab->scroll_x < 0) tab->scroll_x = 0;

    // عصا التحكم اليسرى: التمرير السريع
    s32 lx = (s32)ctx->pad_data.button[6] - 128;
    s32 ly = (s32)ctx->pad_data.button[7] - 128;
    if (abs(lx) > 20) tab->scroll_x += (lx / 8) * ctx->settings.scroll_speed;
    if (abs(ly) > 20) tab->scroll_y += (ly / 8) * ctx->settings.scroll_speed;

    // عصا التحكم اليمنى: التكبير والتصغير
    s32 ry = (s32)ctx->pad_data.button[9] - 128;
    if (abs(ry) > 40) {
        tab->zoom_level -= ry / 40;
        if (tab->zoom_level < 50)  tab->zoom_level = 50;
        if (tab->zoom_level > 200) tab->zoom_level = 200;
    }
}

// ═══════════════════════════════════════════════════════════════
//  تحديث حالة المتصفح
// ═══════════════════════════════════════════════════════════════
static void browser_update(BrowserContext* ctx) {
    u64   now   = get_time_ms();
    float dt    = (float)(now - ctx->last_frame_time) / 1000.0f;
    ctx->delta_time      = dt;
    ctx->last_frame_time = now;

    calculate_fps(ctx);

    // تحديث رسوم متحركة شاشة البداية
    if (ctx->state == BROWSER_STATE_SPLASH) {
        ctx->splash_alpha += dt * 1.5f;
        if (ctx->splash_alpha >= 1.0f) {
            ctx->splash_alpha = 1.0f;
            static float splash_wait = 0.0f;
            splash_wait += dt;
            if (splash_wait >= 2.5f) {
                splash_wait = 0.0f;
                ctx->state  = BROWSER_STATE_HOME;
            }
        }
    }

    // تحديث شريط التحميل
    BrowserTab* tab = &ctx->tabs[ctx->active_tab];
    if (tab->is_loading) {
        ctx->loading_angle += dt * 180.0f;
        if (ctx->loading_angle > 360.0f) ctx->loading_angle -= 360.0f;
        tab->load_progress += dt * 0.3f;
        if (tab->load_progress > 0.95f) tab->load_progress = 0.95f;
    }

    // تحديث توهج شريط العنوان
    if (ctx->url_input_active) {
        ctx->url_bar_glow = 0.5f + 0.5f * sinf(now * 0.003f);
    } else {
        ctx->url_bar_glow *= 0.9f;
    }

    // تحديث القائمة الجانبية
    float target_menu = (ctx->state == BROWSER_STATE_MENU) ? 0.0f : -(float)SIDEBAR_WIDTH;
    ctx->sidebar_offset += (target_menu - ctx->sidebar_offset) * dt * 8.0f;

    // تحديث الإشعار
    if (ctx->notification_timer > 0.0f) {
        ctx->notification_timer -= dt;
        if (ctx->notification_timer <= 0.5f) {
            ctx->notification_alpha = ctx->notification_timer / 0.5f;
        }
    }

    // فحص انتهاء تحميل الصفحة
    if (tab->is_loading && ctx->net_ctx) {
        sysMutexLock(ctx->network_mutex, 0);
        int done = network_is_fetch_done(ctx->net_ctx);
        sysMutexUnlock(ctx->network_mutex);
        if (done) {
            tab->is_loading    = 0;
            tab->load_progress = 1.0f;
            ctx->state         = BROWSER_STATE_BROWSING;
            browser_show_notification(ctx, "تم التحميل", 0);
        }
    }

    // فحص خروج النظام
    sysUtilCheckCallback();
    if (g_sys_callback_exit) ctx->state = BROWSER_STATE_EXIT;
}

// ═══════════════════════════════════════════════════════════════
//  رسم الإطار
// ═══════════════════════════════════════════════════════════════
static void browser_render(BrowserContext* ctx) {
    renderer_begin_frame(ctx->renderer, ctx->frame_buffer[ctx->current_frame]);

    switch (ctx->state) {
        case BROWSER_STATE_SPLASH:
            ui_draw_splash(ctx->ui_ctx, ctx->splash_alpha);
            break;
        case BROWSER_STATE_HOME:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_tab_bar(ctx->ui_ctx, ctx->tabs, ctx->tab_count, ctx->active_tab);
            ui_draw_url_bar(ctx->ui_ctx, ctx->tabs[ctx->active_tab].url,
                            ctx->url_input_active, ctx->url_bar_glow);
            ui_draw_home_page(ctx->ui_ctx, ctx->history,
                              ctx->history_count, ctx->bookmarks, ctx->bookmark_count);
            ui_draw_status_bar(ctx->ui_ctx, ctx->current_fps,
                               ctx->network_available, ctx->total_data_used);
            break;
        case BROWSER_STATE_LOADING:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_tab_bar(ctx->ui_ctx, ctx->tabs, ctx->tab_count, ctx->active_tab);
            ui_draw_url_bar(ctx->ui_ctx, ctx->tabs[ctx->active_tab].url,
                            ctx->url_input_active, ctx->url_bar_glow);
            ui_draw_loading(ctx->ui_ctx,
                            ctx->tabs[ctx->active_tab].load_progress,
                            ctx->loading_angle);
            ui_draw_status_bar(ctx->ui_ctx, ctx->current_fps,
                               ctx->network_available, ctx->total_data_used);
            break;
        case BROWSER_STATE_BROWSING:
        case BROWSER_STATE_URL_INPUT:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_tab_bar(ctx->ui_ctx, ctx->tabs, ctx->tab_count, ctx->active_tab);
            ui_draw_url_bar(ctx->ui_ctx, ctx->url_input_active ?
                            ctx->url_buffer : ctx->tabs[ctx->active_tab].url,
                            ctx->url_input_active, ctx->url_bar_glow);
            ui_draw_page_content(ctx->ui_ctx, &ctx->tabs[ctx->active_tab]);
            if (ctx->tabs[ctx->active_tab].is_loading) {
                ui_draw_loading_bar(ctx->ui_ctx,
                                    ctx->tabs[ctx->active_tab].load_progress);
            }
            ui_draw_status_bar(ctx->ui_ctx, ctx->current_fps,
                               ctx->network_available, ctx->total_data_used);
            if (ctx->url_input_active) {
                ui_draw_url_keyboard(ctx->ui_ctx, ctx->url_buffer,
                                     ctx->url_cursor_pos);
            }
            break;
        case BROWSER_STATE_SETTINGS:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_settings(ctx->ui_ctx, &ctx->settings);
            break;
        case BROWSER_STATE_BOOKMARKS:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_bookmarks(ctx->ui_ctx, ctx->bookmarks, ctx->bookmark_count);
            break;
        case BROWSER_STATE_HISTORY:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_history(ctx->ui_ctx, ctx->history, ctx->history_count);
            break;
        case BROWSER_STATE_MENU:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_tab_bar(ctx->ui_ctx, ctx->tabs, ctx->tab_count, ctx->active_tab);
            ui_draw_url_bar(ctx->ui_ctx, ctx->tabs[ctx->active_tab].url,
                            0, 0.0f);
            ui_draw_page_content(ctx->ui_ctx, &ctx->tabs[ctx->active_tab]);
            ui_draw_sidebar(ctx->ui_ctx, ctx->sidebar_offset,
                            ctx->bookmarks, ctx->bookmark_count,
                            ctx->history,   ctx->history_count);
            ui_draw_status_bar(ctx->ui_ctx, ctx->current_fps,
                               ctx->network_available, ctx->total_data_used);
            break;
        case BROWSER_STATE_ERROR:
            ui_draw_background(ctx->ui_ctx);
            ui_draw_error(ctx->ui_ctx, "حدث خطأ في تحميل الصفحة");
            break;
        default:
            break;
    }

    // رسم الإشعار فوق كل شيء
    if (ctx->notification_timer > 0.0f) {
        ui_draw_notification(ctx->ui_ctx, ctx->notification_text,
                             ctx->notification_alpha, ctx->notification_type);
    }

    renderer_end_frame(ctx->renderer);
    flip_frame(ctx);
}

// ═══════════════════════════════════════════════════════════════
//  الحلقة الرئيسية
// ═══════════════════════════════════════════════════════════════
static void browser_main_loop(BrowserContext* ctx) {
    while (ctx->running && ctx->state != BROWSER_STATE_EXIT) {
        u64 frame_start = get_time_ms();

        browser_handle_input(ctx);
        browser_update(ctx);
        browser_render(ctx);

        // تحديد معدل الإطارات
        u64 frame_end     = get_time_ms();
        u64 frame_elapsed = frame_end - frame_start;
        if (frame_elapsed < FRAME_TIME_MS) {
            usleep((FRAME_TIME_MS - frame_elapsed) * 1000);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  إيقاف تشغيل المتصفح
// ═══════════════════════════════════════════════════════════════
static void browser_shutdown(BrowserContext* ctx) {
    // حفظ البيانات
    browser_save_settings(ctx);
    browser_save_history(ctx);
    browser_save_bookmarks(ctx);

    // إيقاف خيط الشبكة
    ctx->network_thread_running = 0;
    u64 thread_ret;
    sysThreadJoin(ctx->network_thread, &thread_ret);
    sysMutexDestroy(ctx->network_mutex);

    // تحرير الموارد
    if (ctx->net_ctx)   network_shutdown(ctx->net_ctx);
    if (ctx->ui_ctx)    ui_shutdown(ctx->ui_ctx);
    if (ctx->renderer)  renderer_shutdown(ctx->renderer);

    // تحرير مجمعات الذاكرة
    if (ctx->cache_pool)   memory_pool_destroy(ctx->cache_pool);
    if (ctx->network_pool) memory_pool_destroy(ctx->network_pool);
    if (ctx->render_pool)  memory_pool_destroy(ctx->render_pool);
    if (ctx->main_pool)    memory_pool_destroy(ctx->main_pool);

    netDeinitialize();
    printf("[نجاح] تم إيقاف المتصفح بأمان\n");
}

// ═══════════════════════════════════════════════════════════════
//  حفظ وتحميل البيانات
// ═══════════════════════════════════════════════════════════════
static void browser_save_settings(BrowserContext* ctx) {
    FILE* f = fopen("/dev_hdd0/game/PS3BROWSER/settings.bin", "wb");
    if (f) { fwrite(&ctx->settings, sizeof(BrowserSettings), 1, f); fclose(f); }
}
static void browser_load_settings(BrowserContext* ctx) {
    FILE* f = fopen("/dev_hdd0/game/PS3BROWSER/settings.bin", "rb");
    if (f) { fread(&ctx->settings, sizeof(BrowserSettings), 1, f); fclose(f); }
}
static void browser_save_history(BrowserContext* ctx) {
    FILE* f = fopen("/dev_hdd0/game/PS3BROWSER/history.bin", "wb");
    if (f) {
        fwrite(&ctx->history_count, sizeof(int), 1, f);
        fwrite(ctx->history, sizeof(HistoryEntry), ctx->history_count, f);
        fclose(f);
    }
}
static void browser_load_history(BrowserContext* ctx) {
    FILE* f = fopen("/dev_hdd0/game/PS3BROWSER/history.bin", "rb");
    if (f) {
        fread(&ctx->history_count, sizeof(int), 1, f);
        if (ctx->history_count > MAX_HISTORY_SIZE) ctx->history_count = MAX_HISTORY_SIZE;
        fread(ctx->history, sizeof(HistoryEntry), ctx->history_count, f);
        fclose(f);
    }
}
static void browser_save_bookmarks(BrowserContext* ctx) {
    FILE* f = fopen("/dev_hdd0/game/PS3BROWSER/bookmarks.bin", "wb");
    if (f) {
        fwrite(&ctx->bookmark_count, sizeof(int), 1, f);
        fwrite(ctx->bookmarks, sizeof(Bookmark), ctx->bookmark_count, f);
        fclose(f);
    }
}
static void browser_load_bookmarks(BrowserContext* ctx) {
    FILE* f = fopen("/dev_hdd0/game/PS3BROWSER/bookmarks.bin", "rb");
    if (f) {
        fread(&ctx->bookmark_count, sizeof(int), 1, f);
        if (ctx->bookmark_count > MAX_BOOKMARKS) ctx->bookmark_count = MAX_BOOKMARKS;
        fread(ctx->bookmarks, sizeof(Bookmark), ctx->bookmark_count, f);
        fclose(f);
    }
}

// ═══════════════════════════════════════════════════════════════
//  نقطة الدخول الرئيسية
// ═══════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║     PS3 UltraBrowser v1.0.0          ║\n");
    printf("║     متصفح بلايستيشن 3 المتقدم       ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\n");

    if (browser_init(&g_browser) != 0) {
        printf("[خطأ فادح] فشل في تهيئة المتصفح\n");
        return EXIT_FAILURE;
    }

    browser_main_loop(&g_browser);
    browser_shutdown(&g_browser);

    return EXIT_SUCCESS;
}
