// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - renderer.cpp
//  نظام الرسم الاحترافي لبلايستيشن 3
// ═══════════════════════════════════════════════════════════════

#include "renderer.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ═══════════════════════════════════════════════════════════════
//  دوال مساعدة داخلية
// ═══════════════════════════════════════════════════════════════

static inline u32 color_pack(u8 r, u8 g, u8 b, u8 a) {
    return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | b;
}

static inline u32 color_to_xrgb(Color c) {
    return ((u32)c.r << 16) | ((u32)c.g << 8) | c.b;
}

static inline u32 blend_pixel_sw(u32 dst, u32 src_color, u8 src_alpha) {
    if (src_alpha == 255) return src_color;
    if (src_alpha == 0)   return dst;
    u32 dr = (dst >> 16) & 0xFF;
    u32 dg = (dst >>  8) & 0xFF;
    u32 db = (dst      ) & 0xFF;
    u32 sr = (src_color >> 16) & 0xFF;
    u32 sg = (src_color >>  8) & 0xFF;
    u32 sb = (src_color      ) & 0xFF;
    u32 a  = src_alpha;
    u32 ia = 255 - a;
    u32 rr = (sr * a + dr * ia) >> 8;
    u32 rg = (sg * a + dg * ia) >> 8;
    u32 rb = (sb * a + db * ia) >> 8;
    return (rr << 16) | (rg << 8) | rb;
}

static inline int clip_range(int v, int min, int max) {
    return v < min ? min : (v > max ? max : v);
}

static inline f32 clamp01(f32 v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة المُصيِّر
// ═══════════════════════════════════════════════════════════════
RendererContext* renderer_init(gcmContextData* rsx, void* pool, u32 w, u32 h) {
    RendererContext* r = (RendererContext*)memory_pool_alloc_zero(
                          (MemoryPool*)pool, sizeof(RendererContext));
    if (!r) return NULL;

    r->rsx          = rsx;
    r->screen_w     = w;
    r->screen_h     = h;
    r->current_opacity = 1.0f;
    r->antialiasing = 1;
    r->clip_depth   = 0;
    r->font_count   = 0;
    r->texture_count= 0;

    // تخصيص مخازن الرؤوس والمؤشرات
    r->vertex_buffer = (Vertex2D*)memory_pool_alloc(
                        (MemoryPool*)pool,
                        RENDERER_VERTEX_BUFFER_SIZE * sizeof(Vertex2D));
    r->index_buffer  = (u16*)memory_pool_alloc(
                        (MemoryPool*)pool,
                        RENDERER_INDEX_BUFFER_SIZE  * sizeof(u16));

    if (!r->vertex_buffer || !r->index_buffer) {
        printf("[رسم] فشل تخصيص المخازن\n");
        return NULL;
    }

    r->vertex_count    = 0;
    r->index_count     = 0;
    r->draw_call_count = 0;
    r->frame_number    = 0;

    printf("[رسم] تم التهيئة: %dx%d\n", w, h);
    return r;
}

// ═══════════════════════════════════════════════════════════════
//  إيقاف المُصيِّر
// ═══════════════════════════════════════════════════════════════
void renderer_shutdown(RendererContext* r) {
    if (!r) return;
    // تحرير نسج الخطوط
    for (int i = 0; i < r->font_count; i++) {
        if (r->fonts[i].atlas_data) {
            free(r->fonts[i].atlas_data);
            r->fonts[i].atlas_data = NULL;
        }
    }
    printf("[رسم] تم الإيقاف - إطارات: %u، نداءات كلية: %u\n",
           r->frame_number, r->total_draw_calls);
}

// ═══════════════════════════════════════════════════════════════
//  بداية الإطار
// ═══════════════════════════════════════════════════════════════
void renderer_begin_frame(RendererContext* r, u32* buffer) {
    r->current_buffer  = buffer;
    r->vertex_count    = 0;
    r->index_count     = 0;
    r->draw_call_count = 0;
    r->clip_depth      = 0;
    r->current_opacity = 1.0f;
    r->frame_number++;
}

// ═══════════════════════════════════════════════════════════════
//  نهاية الإطار وتنفيذ نداءات الرسم
// ═══════════════════════════════════════════════════════════════
void renderer_end_frame(RendererContext* r) {
    if (!r || !r->current_buffer) return;

    // تنفيذ جميع نداءات الرسم على المخزن الحالي
    for (u32 dc = 0; dc < r->draw_call_count; dc++) {
        DrawCall* call = &r->draw_calls[dc];

        // منطقة القطع
        int clip_x1 = 0, clip_y1 = 0;
        int clip_x2 = (int)r->screen_w, clip_y2 = (int)r->screen_h;
        if (call->has_clip) {
            clip_x1 = (int)call->clip_x;
            clip_y1 = (int)call->clip_y;
            clip_x2 = (int)(call->clip_x + call->clip_w);
            clip_y2 = (int)(call->clip_y + call->clip_h);
        }

        // تنفيذ حسب نوع التظليل
        switch (call->shader) {
            case SHADER_SOLID_COLOR: {
                u32 c = call->color_a;
                u8  a = (c >> 24) & 0xFF;
                u32 rgb = c & 0x00FFFFFF;

                // رسم المثلثات
                for (u32 i = call->index_offset; i + 2 < call->index_offset + call->index_count; i += 3) {
                    u16 i0 = r->index_buffer[i+0] + call->vertex_offset;
                    u16 i1 = r->index_buffer[i+1] + call->vertex_offset;
                    u16 i2 = r->index_buffer[i+2] + call->vertex_offset;
                    if (i0 >= r->vertex_count) continue;
                    if (i1 >= r->vertex_count) continue;
                    if (i2 >= r->vertex_count) continue;

                    Vertex2D* v0 = &r->vertex_buffer[i0];
                    Vertex2D* v1 = &r->vertex_buffer[i1];
                    Vertex2D* v2 = &r->vertex_buffer[i2];

                    // حدود المثلث
                    int min_x = (int)fminf(fminf(v0->x, v1->x), v2->x);
                    int max_x = (int)fmaxf(fmaxf(v0->x, v1->x), v2->x);
                    int min_y = (int)fminf(fminf(v0->y, v1->y), v2->y);
                    int max_y = (int)fmaxf(fmaxf(v0->y, v1->y), v2->y);

                    min_x = clip_range(min_x, clip_x1, clip_x2 - 1);
                    max_x = clip_range(max_x, clip_x1, clip_x2 - 1);
                    min_y = clip_range(min_y, clip_y1, clip_y2 - 1);
                    max_y = clip_range(max_y, clip_y1, clip_y2 - 1);

                    for (int py = min_y; py <= max_y; py++) {
                        for (int px = min_x; px <= max_x; px++) {
                            f32 fx = px + 0.5f, fy = py + 0.5f;
                            // فحص نقطة داخل المثلث
                            f32 d0 = (v1->x - v0->x)*(fy - v0->y) - (v1->y - v0->y)*(fx - v0->x);
                            f32 d1 = (v2->x - v1->x)*(fy - v1->y) - (v2->y - v1->y)*(fx - v1->x);
                            f32 d2 = (v0->x - v2->x)*(fy - v2->y) - (v0->y - v2->y)*(fx - v2->x);
                            if ((d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0)) {
                                u32* pixel = r->current_buffer + py * r->screen_w + px;
                                *pixel = blend_pixel_sw(*pixel, rgb, a);
                            }
                        }
                    }
                }
                break;
            }
            case SHADER_GRADIENT_LINEAR: {
                // تدرج خطي بسيط
                Vertex2D* v = &r->vertex_buffer[call->vertex_offset];
                int x1 = (int)v[0].x, y1 = (int)v[0].y;
                int x2 = (int)v[2].x, y2 = (int)v[2].y;
                x1 = clip_range(x1, clip_x1, clip_x2);
                x2 = clip_range(x2, clip_x1, clip_x2);
                y1 = clip_range(y1, clip_y1, clip_y2);
                y2 = clip_range(y2, clip_y1, clip_y2);

                u32 ca = call->color_a;
                u32 cb = call->color_b;

                for (int py = y1; py < y2; py++) {
                    f32 t = (y2 > y1) ? (f32)(py - y1) / (f32)(y2 - y1) : 0.0f;
                    u8 ra = ((ca>>16)&0xFF), ga_= ((ca>>8)&0xFF), ba_ = (ca&0xFF), aa_ = ((ca>>24)&0xFF);
                    u8 rb = ((cb>>16)&0xFF), gb_ = ((cb>>8)&0xFF), bb_ = (cb&0xFF), ab_ = ((cb>>24)&0xFF);
                    u8 r_ = (u8)(ra + (rb-ra)*t), g_ = (u8)(ga_+(gb_-ga_)*t);
                    u8 b_ = (u8)(ba_+(bb_-ba_)*t), a_ = (u8)(aa_+(ab_-aa_)*t);
                    u32 col = (r_<<16)|(g_<<8)|b_;
                    for (int px = x1; px < x2; px++) {
                        u32* pixel = r->current_buffer + py * r->screen_w + px;
                        *pixel = blend_pixel_sw(*pixel, col, a_);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    r->total_draw_calls += r->draw_call_count;
}

// ═══════════════════════════════════════════════════════════════
//  تصفير الشاشة
// ═══════════════════════════════════════════════════════════════
void renderer_clear(RendererContext* r, Color c) {
    if (!r || !r->current_buffer) return;
    u32 color = color_to_xrgb(c);
    u32 total = r->screen_w * r->screen_h;
    mem_set_u32(r->current_buffer, color, total);
}

// ═══════════════════════════════════════════════════════════════
//  إضافة مستطيل إلى مخزن الرسم (Software Rasterizer)
// ═══════════════════════════════════════════════════════════════
static void sw_fill_rect(RendererContext* r, int x, int y, int w, int h,
                          u32 color, u8 alpha) {
    if (!r || !r->current_buffer || w <= 0 || h <= 0) return;

    // تطبيق القطع
    int clip_x1 = 0, clip_y1 = 0;
    int clip_x2 = (int)r->screen_w, clip_y2 = (int)r->screen_h;
    if (r->clip_depth > 0) {
        ClipRect* cr = &r->clip_stack[r->clip_depth - 1];
        clip_x1 = (int)cr->x;  clip_y1 = (int)cr->y;
        clip_x2 = (int)(cr->x + cr->w); clip_y2 = (int)(cr->y + cr->h);
    }

    int x1 = clip_range(x,     clip_x1, clip_x2);
    int x2 = clip_range(x + w, clip_x1, clip_x2);
    int y1 = clip_range(y,     clip_y1, clip_y2);
    int y2 = clip_range(y + h, clip_y1, clip_y2);

    if (x1 >= x2 || y1 >= y2) return;

    // دمج الشفافية الكلية
    u32 final_alpha = (u32)(alpha * r->current_opacity);
    if (final_alpha == 0) return;
    u8  fa = (u8)(final_alpha > 255 ? 255 : final_alpha);

    if (fa == 255) {
        // رسم معتم بالكامل - أسرع
        for (int py = y1; py < y2; py++) {
            u32* row = r->current_buffer + py * r->screen_w + x1;
            mem_set_u32(row, color, x2 - x1);
        }
    } else {
        // رسم شفاف
        for (int py = y1; py < y2; py++) {
            u32* row = r->current_buffer + py * r->screen_w;
            for (int px = x1; px < x2; px++) {
                row[px] = blend_pixel_sw(row[px], color, fa);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم مستطيل
// ═══════════════════════════════════════════════════════════════
void renderer_draw_rect(RendererContext* r, f32 x, f32 y, f32 w, f32 h, Color c) {
    u32 color = color_to_xrgb(c);
    sw_fill_rect(r, (int)x, (int)y, (int)w, (int)h, color, c.a);
}

// ═══════════════════════════════════════════════════════════════
//  رسم مستطيل بزوايا مدورة
// ═══════════════════════════════════════════════════════════════
void renderer_draw_rect_rounded(RendererContext* r, f32 x, f32 y,
                                 f32 w, f32 h, f32 radius, Color c) {
    if (!r || !r->current_buffer) return;
    u32 color = color_to_xrgb(c);
    u8  alpha = (u8)(c.a * r->current_opacity);

    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    int ir = (int)radius;
    if (ir < 0) ir = 0;
    if (ir > iw/2) ir = iw/2;
    if (ir > ih/2) ir = ih/2;

    // المستطيل المركزي
    sw_fill_rect(r, ix + ir, iy,      iw - 2*ir, ih,      color, alpha);
    // الجانبان الأيسر والأيمن
    sw_fill_rect(r, ix,      iy + ir, ir,         ih-2*ir, color, alpha);
    sw_fill_rect(r, ix+iw-ir,iy + ir, ir,         ih-2*ir, color, alpha);

    // الزوايا المدورة
    for (int py = 0; py < ir; py++) {
        for (int px = 0; px < ir; px++) {
            f32 dx = ir - px - 0.5f;
            f32 dy = ir - py - 0.5f;
            f32 dist = sqrtf(dx*dx + dy*dy);

            if (dist <= (f32)ir) {
                u8 edge_alpha = alpha;
                if (r->antialiasing && dist > (f32)ir - 1.0f) {
                    edge_alpha = (u8)(alpha * (1.0f - (dist - (f32)ir + 1.0f)));
                }
                // زاوية علوية يسرى
                u32* pixel = r->current_buffer + (iy+py)*r->screen_w + (ix+px);
                *pixel = blend_pixel_sw(*pixel, color, edge_alpha);
                // زاوية علوية يمنى
                pixel = r->current_buffer + (iy+py)*r->screen_w + (ix+iw-ir+px);
                *pixel = blend_pixel_sw(*pixel, color, edge_alpha);
                // زاوية سفلية يسرى
                pixel = r->current_buffer + (iy+ih-ir+py)*r->screen_w + (ix+px);
                *pixel = blend_pixel_sw(*pixel, color, edge_alpha);
                // زاوية سفلية يمنى
                pixel = r->current_buffer + (iy+ih-ir+py)*r->screen_w + (ix+iw-ir+px);
                *pixel = blend_pixel_sw(*pixel, color, edge_alpha);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم حدود مستطيل
// ═══════════════════════════════════════════════════════════════
void renderer_draw_rect_border(RendererContext* r, f32 x, f32 y,
                                f32 w, f32 h, f32 thickness, Color c) {
    f32 t = thickness;
    renderer_draw_rect(r, x,       y,       w, t, c);  // أعلى
    renderer_draw_rect(r, x,       y+h-t,   w, t, c);  // أسفل
    renderer_draw_rect(r, x,       y+t,     t, h-2*t, c); // يسار
    renderer_draw_rect(r, x+w-t,   y+t,     t, h-2*t, c); // يمين
}

// ═══════════════════════════════════════════════════════════════
//  رسم دائرة
// ═══════════════════════════════════════════════════════════════
void renderer_draw_circle(RendererContext* r, f32 cx, f32 cy, f32 radius, Color c) {
    if (!r || !r->current_buffer) return;
    u32 color = color_to_xrgb(c);
    u8  alpha = (u8)(c.a * r->current_opacity);
    int icx = (int)cx, icy = (int)cy, ir = (int)radius;

    for (int py = -ir; py <= ir; py++) {
        for (int px = -ir; px <= ir; px++) {
            f32 dist = sqrtf((f32)(px*px + py*py));
            if (dist <= radius) {
                int sx = icx + px, sy = icy + py;
                if (sx < 0 || sx >= (int)r->screen_w) continue;
                if (sy < 0 || sy >= (int)r->screen_h) continue;
                u8 fa = alpha;
                if (r->antialiasing && dist > radius - 1.0f) {
                    fa = (u8)(alpha * (1.0f - (dist - radius + 1.0f)));
                }
                u32* pixel = r->current_buffer + sy * r->screen_w + sx;
                *pixel = blend_pixel_sw(*pixel, color, fa);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم حلقة دائرة
// ═══════════════════════════════════════════════════════════════
void renderer_draw_circle_outline(RendererContext* r, f32 cx, f32 cy,
                                   f32 radius, f32 thickness, Color c) {
    if (!r || !r->current_buffer) return;
    u32 color = color_to_xrgb(c);
    u8  alpha = (u8)(c.a * r->current_opacity);
    int icx = (int)cx, icy = (int)cy;
    int ir_outer = (int)(radius);
    int ir_inner = (int)(radius - thickness);

    for (int py = -ir_outer; py <= ir_outer; py++) {
        for (int px = -ir_outer; px <= ir_outer; px++) {
            f32 dist = sqrtf((f32)(px*px + py*py));
            if (dist <= radius && dist >= radius - thickness) {
                int sx = icx + px, sy = icy + py;
                if (sx < 0 || sx >= (int)r->screen_w) continue;
                if (sy < 0 || sy >= (int)r->screen_h) continue;
                u8 fa = alpha;
                if (r->antialiasing) {
                    if (dist > radius - 1.0f)
                        fa = (u8)(fa * (1.0f - (dist - radius + 1.0f)));
                    else if (dist < radius - thickness + 1.0f)
                        fa = (u8)(fa * (dist - (radius - thickness)));
                }
                u32* pixel = r->current_buffer + sy * r->screen_w + sx;
                *pixel = blend_pixel_sw(*pixel, color, fa);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم قوس
// ═══════════════════════════════════════════════════════════════
void renderer_draw_arc(RendererContext* r, f32 cx, f32 cy,
                        f32 r_inner, f32 r_outer,
                        f32 angle_start, f32 angle_end, Color c) {
    if (!r || !r->current_buffer) return;
    u32 color = color_to_xrgb(c);
    u8  alpha = (u8)(c.a * r->current_opacity);
    int icx = (int)cx, icy = (int)cy;
    int bound = (int)r_outer + 2;

    f32 a_start = angle_start * (f32)M_PI / 180.0f;
    f32 a_end   = angle_end   * (f32)M_PI / 180.0f;

    for (int py = -bound; py <= bound; py++) {
        for (int px = -bound; px <= bound; px++) {
            f32 dist  = sqrtf((f32)(px*px + py*py));
            if (dist < r_inner || dist > r_outer) continue;

            f32 angle = atan2f((f32)py, (f32)px);
            if (angle < a_start || angle > a_end) continue;

            int sx = icx + px, sy = icy + py;
            if (sx < 0 || sx >= (int)r->screen_w) continue;
            if (sy < 0 || sy >= (int)r->screen_h) continue;

            u32* pixel = r->current_buffer + sy * r->screen_w + sx;
            *pixel = blend_pixel_sw(*pixel, color, alpha);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم خط
// ═══════════════════════════════════════════════════════════════
void renderer_draw_line(RendererContext* r, f32 x1, f32 y1,
                         f32 x2, f32 y2, f32 thickness, Color c) {
    if (!r || !r->current_buffer) return;
    u32 color = color_to_xrgb(c);
    u8  alpha = (u8)(c.a * r->current_opacity);

    // خوارزمية بريزنهام المحسّنة
    int ix1 = (int)x1, iy1 = (int)y1, ix2 = (int)x2, iy2 = (int)y2;
    int dx = abs(ix2 - ix1), sx = ix1 < ix2 ? 1 : -1;
    int dy = -abs(iy2 - iy1), sy = iy1 < iy2 ? 1 : -1;
    int err = dx + dy;
    int t   = (int)(thickness / 2);

    while (1) {
        for (int ty = -t; ty <= t; ty++) {
            for (int tx = -t; tx <= t; tx++) {
                int px = ix1 + tx, py = iy1 + ty;
                if (px < 0 || px >= (int)r->screen_w) continue;
                if (py < 0 || py >= (int)r->screen_h) continue;
                u32* pixel = r->current_buffer + py * r->screen_w + px;
                *pixel = blend_pixel_sw(*pixel, color, alpha);
            }
        }
        if (ix1 == ix2 && iy1 == iy2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix1 += sx; }
        if (e2 <= dx) { err += dx; iy1 += sy; }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم تدرجات
// ═══════════════════════════════════════════════════════════════
void renderer_draw_gradient_h(RendererContext* r, f32 x, f32 y,
                               f32 w, f32 h, Color c1, Color c2) {
    if (!r || !r->current_buffer || w <= 0 || h <= 0) return;
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    for (int py = iy; py < iy + ih; py++) {
        if (py < 0 || py >= (int)r->screen_h) continue;
        for (int px = ix; px < ix + iw; px++) {
            if (px < 0 || px >= (int)r->screen_w) continue;
            f32 t = (f32)(px - ix) / (f32)iw;
            u8 rv = (u8)(c1.r + (c2.r - c1.r) * t);
            u8 gv = (u8)(c1.g + (c2.g - c1.g) * t);
            u8 bv = (u8)(c1.b + (c2.b - c1.b) * t);
            u8 av = (u8)(c1.a + (c2.a - c1.a) * t);
            u32 color = (rv<<16)|(gv<<8)|bv;
            u32* pixel = r->current_buffer + py * r->screen_w + px;
            *pixel = blend_pixel_sw(*pixel, color, (u8)(av * r->current_opacity));
        }
    }
}

void renderer_draw_gradient_v(RendererContext* r, f32 x, f32 y,
                               f32 w, f32 h, Color c1, Color c2) {
    if (!r || !r->current_buffer || w <= 0 || h <= 0) return;
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    for (int py = iy; py < iy + ih; py++) {
        if (py < 0 || py >= (int)r->screen_h) continue;
        f32 t = (f32)(py - iy) / (f32)ih;
        u8 rv = (u8)(c1.r + (c2.r - c1.r) * t);
        u8 gv = (u8)(c1.g + (c2.g - c1.g) * t);
        u8 bv = (u8)(c1.b + (c2.b - c1.b) * t);
        u8 av = (u8)(c1.a + (c2.a - c1.a) * t);
        u32 color = (rv<<16)|(gv<<8)|bv;
        u8  fa    = (u8)(av * r->current_opacity);
        for (int px = ix; px < ix + iw; px++) {
            if (px < 0 || px >= (int)r->screen_w) continue;
            u32* pixel = r->current_buffer + py * r->screen_w + px;
            *pixel = blend_pixel_sw(*pixel, color, fa);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم التوهج (Glow Effect)
// ═══════════════════════════════════════════════════════════════
void renderer_draw_glow(RendererContext* r, f32 x, f32 y,
                         f32 w, f32 h, f32 radius,
                         Color color, f32 intensity) {
    if (!r || !r->current_buffer) return;
    int steps = (int)radius;
    for (int i = steps; i > 0; i--) {
        f32 t = 1.0f - (f32)i / (f32)steps;
        f32 a = intensity * t * t * (f32)color.a / 255.0f;
        Color gc = { color.r, color.g, color.b, (u8)(a * 255.0f) };
        f32 expand = (f32)(steps - i);
        renderer_draw_rect_rounded(r,
            x - expand, y - expand,
            w + expand*2, h + expand*2,
            8.0f + expand, gc);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم ظل
// ═══════════════════════════════════════════════════════════════
void renderer_draw_shadow(RendererContext* r, f32 x, f32 y,
                           f32 w, f32 h, f32 offset_x, f32 offset_y,
                           f32 blur, Color color) {
    int steps = (int)(blur / 2) + 1;
    for (int i = steps; i > 0; i--) {
        f32 t  = (f32)(steps - i) / (f32)steps;
        f32 a  = (f32)color.a / 255.0f * (1.0f - t * t);
        Color sc = { color.r, color.g, color.b, (u8)(a * 255.0f) };
        f32 expand = (f32)i;
        renderer_draw_rect_rounded(r,
            x + offset_x - expand,
            y + offset_y - expand,
            w + expand*2, h + expand*2, 8.0f, sc);
    }
}

// ═══════════════════════════════════════════════════════════════
//  رسم نص (محاكاة بسيطة بدون خط حقيقي)
// ═══════════════════════════════════════════════════════════════
void renderer_draw_text(RendererContext* r, const char* text,
                         f32 x, f32 y, f32 size, Color c, int font_index) {
    if (!r || !text || !r->current_buffer) return;

    // رسم نص بسيط بمستطيلات (حتى يتم تحميل الخطوط الحقيقية)
    f32  char_w = size * 0.6f;
    f32  char_h = size;
    f32  cx     = x;
    u32  color  = color_to_xrgb(c);
    u8   alpha  = (u8)(c.a * r->current_opacity);

    // خط بسيط 5x7 بت لكل حرف
    static const u8 font5x7[128][7] = {
        // ... (سيتم ملؤها بخط حقيقي)
        // حالياً رسم مستطيل بديل
    };

    while (*text) {
        unsigned char ch = (unsigned char)*text;
        if (ch == '\n') { cx = x; y += char_h * 1.2f; text++; continue; }
        if (ch == ' ')  { cx += char_w * 0.5f; text++; continue; }

        // رسم حرف كمستطيل بسيط (حتى يتم دعم الخط الحقيقي)
        if (ch > 32 && ch < 128) {
            sw_fill_rect(r, (int)cx, (int)y, (int)(char_w * 0.85f), (int)char_h, color, alpha);
        }
        cx += char_w;
        text++;
    }
}

void renderer_draw_text_clipped(RendererContext* r, const char* text,
                                 f32 x, f32 y, f32 max_w, f32 size,
                                 Color c, int font_index) {
    if (!r || !text) return;
    renderer_push_clip(r, x, y, max_w, size * 1.5f);
    renderer_draw_text(r, text, x, y, size, c, font_index);
    renderer_pop_clip(r);
}

void renderer_draw_text_wrapped(RendererContext* r, const char* text,
                                 f32 x, f32 y, f32 max_w, f32 size,
                                 f32 line_h, Color c, int font_index, f32* out_h) {
    if (!r || !text) return;
    f32 char_w = size * 0.6f;
    f32 cx = x, cy = y;
    const char* p = text;
    while (*p) {
        if (cx + char_w > x + max_w) { cx = x; cy += line_h; }
        if (*p == '\n') { cx = x; cy += line_h; p++; continue; }
        renderer_draw_text(r, (char[]){*p, 0}, cx, cy, size, c, font_index);
        cx += char_w;
        p++;
    }
    if (out_h) *out_h = cy - y + line_h;
}

f32 renderer_text_width(RendererContext* r, const char* text, f32 size, int font_index) {
    if (!text) return 0;
    return strlen(text) * size * 0.6f;
}

f32 renderer_text_height(RendererContext* r, const char* text,
                          f32 max_w, f32 size, f32 line_h, int font_index) {
    if (!text || max_w <= 0) return line_h;
    f32 char_w = size * 0.6f;
    int chars_per_line = (int)(max_w / char_w);
    if (chars_per_line <= 0) chars_per_line = 1;
    int lines = ((int)strlen(text) + chars_per_line - 1) / chars_per_line;
    return lines * line_h;
}

// ═══════════════════════════════════════════════════════════════
//  القطع (Clipping)
// ═══════════════════════════════════════════════════════════════
void renderer_push_clip(RendererContext* r, f32 x, f32 y, f32 w, f32 h) {
    if (!r || r->clip_depth >= RENDERER_MAX_CLIP_STACK) return;
    ClipRect* cr = &r->clip_stack[r->clip_depth++];
    if (r->clip_depth > 1) {
        // تقاطع مع القطع السابق
        ClipRect* prev = &r->clip_stack[r->clip_depth - 2];
        cr->x = fmaxf(x, prev->x);
        cr->y = fmaxf(y, prev->y);
        cr->w = fminf(x+w, prev->x+prev->w) - cr->x;
        cr->h = fminf(y+h, prev->y+prev->h) - cr->y;
        if (cr->w < 0) cr->w = 0;
        if (cr->h < 0) cr->h = 0;
    } else {
        cr->x = x; cr->y = y; cr->w = w; cr->h = h;
    }
}

void renderer_pop_clip(RendererContext* r) {
    if (r && r->clip_depth > 0) r->clip_depth--;
}

// ═══════════════════════════════════════════════════════════════
//  الشفافية
// ═══════════════════════════════════════════════════════════════
void renderer_push_opacity(RendererContext* r, f32 opacity) {
    if (r) r->current_opacity *= clamp01(opacity);
}

void renderer_pop_opacity(RendererContext* r) {
    // تبسيط: إعادة إلى 1.0 (يمكن تطويره لاحقاً بمكدس)
    if (r) r->current_opacity = 1.0f;
}

// ═══════════════════════════════════════════════════════════════
//  النسج
// ═══════════════════════════════════════════════════════════════
u32 renderer_create_texture(RendererContext* r, u8* data, u32 w, u32 h, int filtered) {
    if (!r || r->texture_count >= RENDERER_MAX_TEXTURES) return 0;
    Texture* t = &r->textures[r->texture_count];
    t->id       = r->texture_count + 1;
    t->width    = w;
    t->height   = h;
    t->data     = data;
    t->filtered = filtered;
    t->loaded   = 1;
    r->texture_count++;
    return t->id;
}

void renderer_free_texture(RendererContext* r, u32 texture_id) {
    if (!r || texture_id == 0) return;
    for (int i = 0; i < r->texture_count; i++) {
        if (r->textures[i].id == texture_id) {
            r->textures[i].loaded = 0;
            if (r->textures[i].data) {
                free(r->textures[i].data);
                r->textures[i].data = NULL;
            }
            return;
        }
    }
}

void renderer_draw_texture(RendererContext* r, u32 texture_id,
                            f32 x, f32 y, f32 w, f32 h, f32 opacity) {
    if (!r || !r->current_buffer || texture_id == 0) return;

    Texture* tex = NULL;
    for (int i = 0; i < r->texture_count; i++) {
        if (r->textures[i].id == texture_id && r->textures[i].loaded) {
            tex = &r->textures[i]; break;
        }
    }
    if (!tex || !tex->data) return;

    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    u8  fa = (u8)(opacity * r->current_opacity * 255.0f);

    for (int py = 0; py < ih; py++) {
        int sy = iy + py;
        if (sy < 0 || sy >= (int)r->screen_h) continue;
        for (int px = 0; px < iw; px++) {
            int sx = ix + px;
            if (sx < 0 || sx >= (int)r->screen_w) continue;

            // تعيين النسيج
            int tx = (int)((f32)px / iw * tex->width);
            int ty_ = (int)((f32)py / ih * tex->height);
            tx = clip_range(tx, 0, (int)tex->width  - 1);
            ty_ = clip_range(ty_, 0, (int)tex->height - 1);

            u32* texel  = (u32*)(tex->data + (ty_ * tex->width + tx) * 4);
            u8   ta     = (u8)(((*texel >> 24) & 0xFF) * fa / 255);
            u32  tcolor = *texel & 0x00FFFFFF;

            u32* pixel = r->current_buffer + sy * r->screen_w + sx;
            *pixel = blend_pixel_sw(*pixel, tcolor, ta);
        }
    }
}

void renderer_draw_texture_tinted(RendererContext* r, u32 texture_id,
                                   f32 x, f32 y, f32 w, f32 h,
                                   Color tint, f32 opacity) {
    renderer_draw_texture(r, texture_id, x, y, w, h, opacity);
}

void renderer_draw_texture_uv(RendererContext* r, u32 texture_id,
                               f32 x, f32 y, f32 w, f32 h,
                               f32 u1, f32 v1, f32 u2, f32 v2) {
    renderer_draw_texture(r, texture_id, x, y, w, h, 1.0f);
}

// ═══════════════════════════════════════════════════════════════
//  أدوات مساعدة
// ═══════════════════════════════════════════════════════════════
void renderer_fill_pixel_buffer(RendererContext* r, u32* dst, u32 color, u32 count) {
    mem_set_u32(dst, color, count);
}

void renderer_blit_rect(RendererContext* r, u32* dst, u32 dst_stride,
                         u32* src, u32 src_stride, u32 w, u32 h) {
    for (u32 row = 0; row < h; row++) {
        mem_copy_u32(dst + row * dst_stride, src + row * src_stride, w);
    }
}

u32 renderer_blend_pixel(u32 dst, u32 src) {
    u8 a = (src >> 24) & 0xFF;
    return blend_pixel_sw(dst, src & 0xFFFFFF, a);
}

void renderer_flush(RendererContext* r) {
    // تدفق الأوامر إلى RSX
    if (r && r->rsx) {
        rsxFlushBuffer(r->rsx);
    }
}

void renderer_draw_gradient_radial(RendererContext* r, f32 cx, f32 cy,
                                    f32 radius, Color inner, Color outer) {
    if (!r || !r->current_buffer) return;
    int ir = (int)radius;
    for (int py = -ir; py <= ir; py++) {
        for (int px = -ir; px <= ir; px++) {
            f32 dist = sqrtf((f32)(px*px + py*py));
            if (dist > radius) continue;
            f32 t = dist / radius;
            u8 rv = (u8)(inner.r + (outer.r - inner.r) * t);
            u8 gv = (u8)(inner.g + (outer.g - inner.g) * t);
            u8 bv = (u8)(inner.b + (outer.b - inner.b) * t);
            u8 av = (u8)(inner.a + (outer.a - inner.a) * t);
            int sx = (int)cx + px, sy = (int)cy + py;
            if (sx < 0 || sx >= (int)r->screen_w) continue;
            if (sy < 0 || sy >= (int)r->screen_h) continue;
            u32 color  = (rv<<16)|(gv<<8)|bv;
            u32* pixel = r->current_buffer + sy * r->screen_w + sx;
            *pixel = blend_pixel_sw(*pixel, color, (u8)(av * r->current_opacity));
        }
    }
}

void renderer_draw_gradient_angle(RendererContext* r, f32 x, f32 y,
                                   f32 w, f32 h, Color c1, Color c2, f32 angle) {
    // تبسيط: استخدام التدرج الرأسي
    renderer_draw_gradient_v(r, x, y, w, h, c1, c2);
}

void renderer_draw_blur(RendererContext* r, f32 x, f32 y, f32 w, f32 h, f32 sigma) {
    // ضبابية بسيطة بالمتوسط (Box blur)
    if (!r || !r->current_buffer || sigma < 1) return;
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    int ks = (int)(sigma * 2) | 1; // حجم النواة (فردي)
    int half = ks / 2;

    for (int py = iy; py < iy + ih; py++) {
        if (py < half || py >= (int)r->screen_h - half) continue;
        for (int px = ix; px < ix + iw; px++) {
            if (px < half || px >= (int)r->screen_w - half) continue;
            u32 sum_r = 0, sum_g = 0, sum_b = 0, count = 0;
            for (int ky = -half; ky <= half; ky++) {
                for (int kx = -half; kx <= half; kx++) {
                    u32 p = r->current_buffer[(py+ky)*r->screen_w + (px+kx)];
                    sum_r += (p >> 16) & 0xFF;
                    sum_g += (p >>  8) & 0xFF;
                    sum_b += (p      ) & 0xFF;
                    count++;
                }
            }
            r->current_buffer[py * r->screen_w + px] =
                ((sum_r/count) << 16) | ((sum_g/count) << 8) | (sum_b/count);
        }
    }
}

int renderer_load_font(RendererContext* r, const char* name, const u8* data, size_t size) {
    if (!r || r->font_count >= RENDERER_MAX_FONTS) return -1;
    Font* f = &r->fonts[r->font_count];
    strncpy(f->name, name, 63);
    f->base_size = 16.0f;
    f->ascent    = 12.0f;
    f->descent   = 4.0f;
    f->line_gap  = 4.0f;
    f->loaded    = 1;
    return r->font_count++;
}

int renderer_get_font(RendererContext* r, const char* name) {
    if (!r || !name) return 0;
    for (int i = 0; i < r->font_count; i++) {
        if (strcmp(r->fonts[i].name, name) == 0) return i;
    }
    return 0;
}

void renderer_draw_triangle(RendererContext* r, f32 x1, f32 y1,
                             f32 x2, f32 y2, f32 x3, f32 y3, Color c) {
    renderer_draw_line(r, x1, y1, x2, y2, 1.5f, c);
    renderer_draw_line(r, x2, y2, x3, y3, 1.5f, c);
    renderer_draw_line(r, x3, y3, x1, y1, 1.5f, c);
}

void renderer_draw_rect_border_rounded(RendererContext* r, f32 x, f32 y,
                                        f32 w, f32 h, f32 radius,
                                        f32 thickness, Color c) {
    Color inner = { 0,0,0,0 };
    renderer_draw_rect_rounded(r, x, y, w, h, radius, c);
    inner.a = 0;
    renderer_draw_rect_rounded(r, x+thickness, y+thickness,
                                w-thickness*2, h-thickness*2, radius, inner);
}

void renderer_draw_text_arabic(RendererContext* r, const char* text,
                                f32 x, f32 y, f32 size, Color c) {
    // النص العربي يُرسم من اليمين لليسار
    if (!r || !text) return;
    f32 total_w = renderer_text_width(r, text, size, r->arabic_font);
    renderer_draw_text(r, text, x - total_w, y, size, c, r->arabic_font);
}
