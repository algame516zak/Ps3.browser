#pragma once
#ifndef RENDERER_H
#define RENDERER_H

#include "browser.h"
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>

// ═══════════════════════════════════════════════════════════════
//  ثوابت المُصيِّر
// ═══════════════════════════════════════════════════════════════
#define RENDERER_MAX_TEXTURES       256
#define RENDERER_MAX_FONTS          8
#define RENDERER_MAX_DRAW_CALLS     4096
#define RENDERER_FONT_ATLAS_SIZE    512
#define RENDERER_GLYPH_COUNT        256
#define RENDERER_MAX_CLIP_STACK     32
#define RENDERER_VERTEX_BUFFER_SIZE (1024 * 1024)
#define RENDERER_INDEX_BUFFER_SIZE  (512  * 1024)

// ═══════════════════════════════════════════════════════════════
//  أنواع التظليل (Shaders)
// ═══════════════════════════════════════════════════════════════
typedef enum {
    SHADER_SOLID_COLOR = 0,
    SHADER_TEXTURE,
    SHADER_TEXT,
    SHADER_GRADIENT_LINEAR,
    SHADER_GRADIENT_RADIAL,
    SHADER_BLUR,
    SHADER_GLOW,
    SHADER_ROUNDED_RECT,
    SHADER_COUNT
} ShaderType;

// ═══════════════════════════════════════════════════════════════
//  هيكل رأس الرسم (Vertex)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    f32 x, y;
    f32 u, v;
    u32 color;
} Vertex2D;

// ═══════════════════════════════════════════════════════════════
//  هيكل نداء الرسم (Draw Call)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    ShaderType  shader;
    u32         texture_id;
    u32         vertex_offset;
    u32         vertex_count;
    u32         index_offset;
    u32         index_count;
    f32         clip_x, clip_y, clip_w, clip_h;
    int         has_clip;
    u32         color_a;
    u32         color_b;
    f32         gradient_angle;
    f32         corner_radius;
    f32         glow_size;
    f32         blur_sigma;
    f32         opacity;
} DrawCall;

// ═══════════════════════════════════════════════════════════════
//  هيكل رمز الخط (Glyph)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    u32 codepoint;
    f32 atlas_x, atlas_y;
    f32 atlas_w, atlas_h;
    f32 bearing_x, bearing_y;
    f32 advance;
    int loaded;
} Glyph;

// ═══════════════════════════════════════════════════════════════
//  هيكل الخط
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    name[64];
    Glyph   glyphs[RENDERER_GLYPH_COUNT];
    u32     atlas_texture;
    u8*     atlas_data;
    f32     ascent;
    f32     descent;
    f32     line_gap;
    f32     base_size;
    int     loaded;
} Font;

// ═══════════════════════════════════════════════════════════════
//  هيكل النسيج (Texture)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    u32     id;
    u32     width;
    u32     height;
    u8*     data;
    size_t  data_size;
    int     loaded;
    int     filtered;
    char    url[512];
} Texture;

// ═══════════════════════════════════════════════════════════════
//  هيكل مستطيل القطع (Clip Rect)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    f32 x, y, w, h;
} ClipRect;

// ═══════════════════════════════════════════════════════════════
//  هيكل سياق المُصيِّر
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // RSX
    gcmContextData* rsx;
    u32*            current_buffer;
    u32             screen_w;
    u32             screen_h;

    // المخازن
    Vertex2D*       vertex_buffer;
    u16*            index_buffer;
    u32             vertex_count;
    u32             index_count;

    // نداءات الرسم
    DrawCall        draw_calls[RENDERER_MAX_DRAW_CALLS];
    u32             draw_call_count;

    // الخطوط
    Font            fonts[RENDERER_MAX_FONTS];
    int             font_count;
    int             default_font;
    int             arabic_font;

    // النسج
    Texture         textures[RENDERER_MAX_TEXTURES];
    int             texture_count;

    // القطع
    ClipRect        clip_stack[RENDERER_MAX_CLIP_STACK];
    int             clip_depth;

    // الحالة الحالية
    f32             current_opacity;
    ShaderType      current_shader;
    u32             current_texture;
    int             antialiasing;

    // الإحصائيات
    u32             total_vertices;
    u32             total_draw_calls;
    u32             frame_number;
    f32             frame_time_ms;

} RendererContext;

// ═══════════════════════════════════════════════════════════════
//  واجهة برمجة المُصيِّر
// ═══════════════════════════════════════════════════════════════
#ifdef __cplusplus
extern "C" {
#endif

// تهيئة وإيقاف
RendererContext* renderer_init         (gcmContextData* rsx, void* pool,
                                        u32 w, u32 h);
void             renderer_shutdown     (RendererContext* r);
void             renderer_begin_frame  (RendererContext* r, u32* buffer);
void             renderer_end_frame    (RendererContext* r);
void             renderer_flush        (RendererContext* r);

// رسم الأشكال الأساسية
void             renderer_clear        (RendererContext* r, Color c);
void             renderer_draw_rect    (RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, Color c);
void             renderer_draw_rect_rounded(RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, f32 radius, Color c);
void             renderer_draw_rect_border(RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, f32 thickness, Color c);
void             renderer_draw_rect_border_rounded(RendererContext* r,
                                        f32 x, f32 y, f32 w, f32 h,
                                        f32 radius, f32 thickness, Color c);
void             renderer_draw_circle  (RendererContext* r, f32 cx, f32 cy,
                                        f32 radius, Color c);
void             renderer_draw_circle_outline(RendererContext* r, f32 cx, f32 cy,
                                        f32 radius, f32 thickness, Color c);
void             renderer_draw_line    (RendererContext* r, f32 x1, f32 y1,
                                        f32 x2, f32 y2, f32 thickness, Color c);
void             renderer_draw_triangle(RendererContext* r, f32 x1, f32 y1,
                                        f32 x2, f32 y2, f32 x3, f32 y3, Color c);
void             renderer_draw_arc     (RendererContext* r, f32 cx, f32 cy,
                                        f32 r_inner, f32 r_outer,
                                        f32 angle_start, f32 angle_end, Color c);

// رسم التدرجات
void             renderer_draw_gradient_h(RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, Color c1, Color c2);
void             renderer_draw_gradient_v(RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, Color c1, Color c2);
void             renderer_draw_gradient_angle(RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, Color c1, Color c2,
                                        f32 angle);
void             renderer_draw_gradient_radial(RendererContext* r, f32 cx, f32 cy,
                                        f32 r, Color inner, Color outer);

// رسم التوهج والضبابية
void             renderer_draw_glow    (RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, f32 radius,
                                        Color color, f32 intensity);
void             renderer_draw_blur    (RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, f32 sigma);
void             renderer_draw_shadow  (RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, f32 offset_x, f32 offset_y,
                                        f32 blur, Color color);

// رسم النصوص
void             renderer_draw_text    (RendererContext* r, const char* text,
                                        f32 x, f32 y, f32 size, Color c,
                                        int font_index);
void             renderer_draw_text_arabic(RendererContext* r, const char* text,
                                        f32 x, f32 y, f32 size, Color c);
void             renderer_draw_text_clipped(RendererContext* r, const char* text,
                                        f32 x, f32 y, f32 max_w, f32 size,
                                        Color c, int font_index);
void             renderer_draw_text_wrapped(RendererContext* r, const char* text,
                                        f32 x, f32 y, f32 max_w, f32 size,
                                        f32 line_h, Color c, int font_index,
                                        f32* out_h);
f32              renderer_text_width   (RendererContext* r, const char* text,
                                        f32 size, int font_index);
f32              renderer_text_height  (RendererContext* r, const char* text,
                                        f32 max_w, f32 size, f32 line_h,
                                        int font_index);

// رسم النسج
u32              renderer_create_texture(RendererContext* r, u8* data,
                                        u32 w, u32 h, int filtered);
void             renderer_free_texture (RendererContext* r, u32 texture_id);
void             renderer_draw_texture (RendererContext* r, u32 texture_id,
                                        f32 x, f32 y, f32 w, f32 h, f32 opacity);
void             renderer_draw_texture_uv(RendererContext* r, u32 texture_id,
                                        f32 x, f32 y, f32 w, f32 h,
                                        f32 u1, f32 v1, f32 u2, f32 v2);
void             renderer_draw_texture_tinted(RendererContext* r, u32 texture_id,
                                        f32 x, f32 y, f32 w, f32 h,
                                        Color tint, f32 opacity);

// القطع
void             renderer_push_clip    (RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h);
void             renderer_pop_clip     (RendererContext* r);

// الشفافية
void             renderer_push_opacity (RendererContext* r, f32 opacity);
void             renderer_pop_opacity  (RendererContext* r);

// الخطوط
int              renderer_load_font    (RendererContext* r, const char* name,
                                        const u8* data, size_t size);
int              renderer_get_font     (RendererContext* r, const char* name);

// أدوات
void             renderer_fill_pixel_buffer(RendererContext* r, u32* dst,
                                        u32 color, u32 count);
void             renderer_blit_rect    (RendererContext* r, u32* dst,
                                        u32 dst_stride, u32* src,
                                        u32 src_stride, u32 w, u32 h);
u32              renderer_blend_pixel  (u32 dst, u32 src);

#ifdef __cplusplus
}
#endif

#endif // RENDERER_H
