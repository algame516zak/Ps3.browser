// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - network.cpp
//  نظام الشبكة والتواصل مع الخادم الوسيط
// ═══════════════════════════════════════════════════════════════

#include "network.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════
//  دوال داخلية
// ═══════════════════════════════════════════════════════════════

static u64 net_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

static void net_log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("[شبكة] %s\n", buf);
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة سياق الشبكة
// ═══════════════════════════════════════════════════════════════
NetworkContext* network_init(const char* server_url, void* pool) {
    NetworkContext* ctx = (NetworkContext*)memory_pool_alloc_zero(
                           (MemoryPool*)pool, sizeof(NetworkContext));
    if (!ctx) return NULL;

    ctx->memory_pool      = pool;
    ctx->cache_max_size   = 8 * 1024 * 1024; // 8 ميغابايت
    ctx->follow_redirects = 1;
    ctx->verify_ssl       = 0;
    ctx->use_cache        = 1;
    ctx->use_cookies      = 1;
    ctx->max_connections  = 4;
    ctx->timeout_ms       = 30000;

    // نسخ عنوان الخادم
    strncpy(ctx->server_url, server_url, NET_MAX_URL_LEN - 1);

    // تحليل عنوان الخادم
    url_parse(server_url, ctx->server_host, &ctx->server_port, NULL, NULL);

    // وكيل المستخدم
    snprintf(ctx->user_agent, sizeof(ctx->user_agent),
             "PS3UltraBrowser/1.0 (PLAYSTATION 3; CFW; PSL1GHT)");

    ctx->current_fetch_id   = -1;
    ctx->current_fetch_done = 0;
    ctx->current_fetch_data = NULL;
    ctx->current_fetch_size = 0;

    net_log("تم تهيئة الشبكة - الخادم: %s:%d", ctx->server_host, ctx->server_port);
    return ctx;
}

// ═══════════════════════════════════════════════════════════════
//  إيقاف تشغيل الشبكة
// ═══════════════════════════════════════════════════════════════
void network_shutdown(NetworkContext* ctx) {
    if (!ctx) return;

    // إغلاق جميع الاتصالات
    for (int i = 0; i < NET_MAX_REQUESTS; i++) {
        if (!ctx->active_requests[i].state == REQUEST_IDLE &&
             ctx->active_requests[i].socket_fd > 0) {
            close(ctx->active_requests[i].socket_fd);
            ctx->active_requests[i].socket_fd = -1;
        }
        if (ctx->active_requests[i].response) {
            memory_pool_free((MemoryPool*)ctx->memory_pool,
                             ctx->active_requests[i].response);
        }
    }

    // تحرير كاش
    network_cache_clear(ctx);

    net_log("إحصائيات الشبكة: تنزيل=%llu KB، رفع=%llu KB، طلبات=%u",
            ctx->total_bytes_down / 1024,
            ctx->total_bytes_up   / 1024,
            ctx->total_requests);
}

// ═══════════════════════════════════════════════════════════════
//  تحليل عنوان URL
// ═══════════════════════════════════════════════════════════════
void url_parse(const char* url, char* host, u16* port, char* path, int* use_https) {
    if (!url) return;

    int https = (strncmp(url, "https://", 8) == 0);
    if (use_https) *use_https = https;

    const char* start = url + (https ? 8 : 7); // تخطي http:// أو https://
    if (strncmp(url, "http://", 7) != 0 && !https) start = url;

    // البحث عن نهاية المضيف
    const char* slash = strchr(start, '/');
    const char* colon = strchr(start, ':');

    size_t host_len;
    u16    default_port = https ? 443 : 80;

    if (colon && (!slash || colon < slash)) {
        // منفذ مخصص
        host_len = colon - start;
        if (port) *port = (u16)atoi(colon + 1);
    } else {
        host_len = slash ? (size_t)(slash - start) : strlen(start);
        if (port) *port = default_port;
    }

    if (host) {
        size_t copy_len = host_len < 255 ? host_len : 255;
        memcpy(host, start, copy_len);
        host[copy_len] = '\0';
    }

    if (path) {
        if (slash) strncpy(path, slash, NET_MAX_URL_LEN - 1);
        else       strcpy(path, "/");
    }
}

// ═══════════════════════════════════════════════════════════════
//  بناء رابط الوكيل
// ═══════════════════════════════════════════════════════════════
void url_build_proxy(NetworkContext* ctx, const char* target_url,
                     char* out, size_t out_len) {
    // ترميز عنوان الهدف
    char encoded[NET_MAX_URL_LEN];
    url_encode(target_url, encoded, sizeof(encoded));

    // بناء رابط الطلب للخادم الوسيط
    snprintf(out, out_len, "%s/fetch?url=%s", ctx->server_url, encoded);
}

// ═══════════════════════════════════════════════════════════════
//  ترميز URL
// ═══════════════════════════════════════════════════════════════
void url_encode(const char* in, char* out, size_t out_len) {
    static const char hex[] = "0123456789ABCDEF";
    size_t out_pos = 0;

    while (*in && out_pos + 3 < out_len) {
        unsigned char c = (unsigned char)*in;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~'  || c == ':' || c == '/' ||
            c == '?' || c == '&'  || c == '=' || c == '#') {
            out[out_pos++] = c;
        } else {
            out[out_pos++] = '%';
            out[out_pos++] = hex[c >> 4];
            out[out_pos++] = hex[c & 0xF];
        }
        in++;
    }
    out[out_pos] = '\0';
}

// ═══════════════════════════════════════════════════════════════
//  فك ترميز URL
// ═══════════════════════════════════════════════════════════════
void url_decode(const char* in, char* out, size_t out_len) {
    size_t out_pos = 0;
    while (*in && out_pos + 1 < out_len) {
        if (*in == '%' && *(in+1) && *(in+2)) {
            char hex[3] = { *(in+1), *(in+2), 0 };
            out[out_pos++] = (char)strtol(hex, NULL, 16);
            in += 3;
        } else if (*in == '+') {
            out[out_pos++] = ' ';
            in++;
        } else {
            out[out_pos++] = *in++;
        }
    }
    out[out_pos] = '\0';
}

// ═══════════════════════════════════════════════════════════════
//  فتح اتصال TCP
// ═══════════════════════════════════════════════════════════════
static int tcp_connect(const char* host, u16 port, int timeout_ms) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        net_log("فشل حل DNS للمضيف: %s", host);
        return -1;
    }

    int sock = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;

        // تعيين مهلة الاتصال
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);
    return sock;
}

// ═══════════════════════════════════════════════════════════════
//  إرسال طلب HTTP
// ═══════════════════════════════════════════════════════════════
static int http_send_request(int sock, const char* host, const char* path,
                              const char* user_agent, const char* extra_headers) {
    char request[4096];
    int  len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: %s\r\n"
        "Accept: application/json,text/html,*/*\r\n"
        "Accept-Language: ar,en;q=0.9\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        path, host, user_agent,
        extra_headers ? extra_headers : "");

    int sent = 0;
    while (sent < len) {
        int n = send(sock, request + sent, len - sent, 0);
        if (n <= 0) { net_log("خطأ في إرسال الطلب"); return -1; }
        sent += n;
    }
    return sent;
}

// ═══════════════════════════════════════════════════════════════
//  استقبال استجابة HTTP
// ═══════════════════════════════════════════════════════════════
static char* http_receive_response(int sock, size_t* out_size, int* out_status) {
    char   header_buf[8192];
    int    header_len  = 0;
    int    headers_done= 0;
    size_t content_len = 0;
    int    status_code = 0;
    int    chunked     = 0;

    // قراءة الرؤوس
    while (header_len < (int)sizeof(header_buf) - 1) {
        char c;
        int  n = recv(sock, &c, 1, 0);
        if (n <= 0) break;
        header_buf[header_len++] = c;

        // فحص نهاية الرؤوس
        if (header_len >= 4 &&
            header_buf[header_len-4] == '\r' &&
            header_buf[header_len-3] == '\n' &&
            header_buf[header_len-2] == '\r' &&
            header_buf[header_len-1] == '\n') {
            headers_done = 1;
            break;
        }
    }

    if (!headers_done) return NULL;
    header_buf[header_len] = '\0';

    // استخراج رمز الحالة
    sscanf(header_buf, "HTTP/1.%*d %d", &status_code);
    if (out_status) *out_status = status_code;

    // استخراج طول المحتوى
    char* cl = strstr(header_buf, "Content-Length:");
    if (cl) content_len = (size_t)atol(cl + 15);

    // فحص التقطيع
    if (strstr(header_buf, "Transfer-Encoding: chunked")) chunked = 1;

    // تخصيص مخزن الاستجابة
    size_t buf_size = content_len > 0 ? content_len + 1 : NET_MAX_RESPONSE_SIZE;
    char*  buf      = (char*)malloc(buf_size);
    if (!buf) return NULL;

    size_t received = 0;

    if (!chunked) {
        // استقبال عادي
        while (received < buf_size - 1) {
            size_t to_read = buf_size - 1 - received;
            if (to_read > NET_CHUNK_SIZE) to_read = NET_CHUNK_SIZE;
            int n = recv(sock, buf + received, to_read, 0);
            if (n <= 0) break;
            received += n;
            if (content_len > 0 && received >= content_len) break;
        }
    } else {
        // استقبال مقطّع
        char chunk_header[64];
        while (1) {
            // قراءة حجم القطعة
            int ch_len = 0;
            while (ch_len < 63) {
                char c;
                if (recv(sock, &c, 1, 0) <= 0) goto done;
                if (c == '\r') { recv(sock, &c, 1, 0); break; }
                chunk_header[ch_len++] = c;
            }
            chunk_header[ch_len] = '\0';
            size_t chunk_size = (size_t)strtol(chunk_header, NULL, 16);
            if (chunk_size == 0) break;

            // قراءة بيانات القطعة
            size_t remaining = chunk_size;
            while (remaining > 0) {
                size_t to_read = remaining < NET_CHUNK_SIZE ? remaining : NET_CHUNK_SIZE;
                if (received + to_read >= buf_size - 1) {
                    buf_size *= 2;
                    char* new_buf = (char*)realloc(buf, buf_size);
                    if (!new_buf) goto done;
                    buf = new_buf;
                }
                int n = recv(sock, buf + received, to_read, 0);
                if (n <= 0) goto done;
                received  += n;
                remaining -= n;
            }
            // تخطي CRLF بعد القطعة
            char crlf[2];
            recv(sock, crlf, 2, 0);
        }
    }

done:
    buf[received] = '\0';
    if (out_size) *out_size = received;
    return buf;
}

// ═══════════════════════════════════════════════════════════════
//  جلب عنوان URL عبر الخادم الوسيط
// ═══════════════════════════════════════════════════════════════
int network_fetch_url(NetworkContext* ctx, const char* url, void* tab) {
    if (!ctx || !url) return -1;

    // فحص الكاش أولاً
    char*  cached_data = NULL;
    size_t cached_size = 0;
    if (ctx->use_cache &&
        network_cache_get(ctx, url, &cached_data, &cached_size) == 0) {
        net_log("كاش: %s", url);
        ctx->current_fetch_done = 1;
        ctx->current_fetch_data = cached_data;
        ctx->current_fetch_size = cached_size;
        ctx->cached_hits++;
        return 0;
    }

    // بناء رابط الوكيل
    char proxy_url[NET_MAX_URL_LEN];
    url_build_proxy(ctx, url, proxy_url, sizeof(proxy_url));

    // تحليل رابط الخادم
    char host[256];
    char path[NET_MAX_URL_LEN];
    u16  port;
    url_parse(proxy_url, host, &port, path, NULL);

    net_log("جلب: %s", url);
    u64 start = net_time_ms();

    // فتح اتصال
    int sock = tcp_connect(host, port, ctx->timeout_ms);
    if (sock < 0) {
        net_log("فشل الاتصال بالخادم");
        ctx->failed_requests++;
        ctx->current_fetch_done = 1;
        ctx->current_fetch_data = NULL;
        return -1;
    }

    // إرسال الطلب
    if (http_send_request(sock, host, path, ctx->user_agent, NULL) < 0) {
        close(sock);
        ctx->current_fetch_done = 1;
        return -1;
    }

    // استقبال الاستجابة
    size_t resp_size = 0;
    int    status    = 0;
    char*  response  = http_receive_response(sock, &resp_size, &status);
    close(sock);

    u64 elapsed = net_time_ms() - start;
    net_log("تم الجلب: %d، حجم=%zu بايت، وقت=%llu ms", status, resp_size, elapsed);

    ctx->total_requests++;
    ctx->total_bytes_down += resp_size;

    if (!response) {
        net_log("لم يتم استقبال بيانات");
        ctx->failed_requests++;
        ctx->current_fetch_done = 1;
        return -1;
    }

    // حفظ في الكاش
    if (ctx->use_cache && status == 200) {
        network_cache_set(ctx, url, response, resp_size, status, "text/html");
    }

    ctx->current_fetch_done   = 1;
    ctx->current_fetch_data   = response;
    ctx->current_fetch_size   = resp_size;
    ctx->current_fetch_status = status;

    // تحديث متوسط السرعة
    if (elapsed > 0) {
        f32 speed = (f32)(resp_size / 1024.0f) / ((f32)elapsed / 1000.0f);
        ctx->avg_speed_kbps = ctx->avg_speed_kbps * 0.8f + speed * 0.2f;
    }

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  معالجة طابور الطلبات
// ═══════════════════════════════════════════════════════════════
void network_process_queue(NetworkContext* ctx) {
    if (!ctx) return;
    // معالجة الطلبات المنتظرة
    // (سيتم توسيعها لاحقاً لدعم الطلبات المتوازية)
}

// ═══════════════════════════════════════════════════════════════
//  فحص انتهاء الجلب
// ═══════════════════════════════════════════════════════════════
int network_is_fetch_done(NetworkContext* ctx) {
    return ctx ? ctx->current_fetch_done : 1;
}

const char* network_get_fetch_data(NetworkContext* ctx, size_t* out_size) {
    if (!ctx) return NULL;
    if (out_size) *out_size = ctx->current_fetch_size;
    ctx->current_fetch_done = 0; // إعادة تعيين
    return ctx->current_fetch_data;
}

// ═══════════════════════════════════════════════════════════════
//  الكاش
// ═══════════════════════════════════════════════════════════════
int network_cache_get(NetworkContext* ctx, const char* url,
                      char** out_data, size_t* out_size) {
    if (!ctx || !url) return -1;
    time_t now = time(NULL);

    for (int i = 0; i < ctx->cache_count; i++) {
        CacheEntry* e = &ctx->cache[i];
        if (!e->valid) continue;
        if (strcmp(e->url, url) != 0) continue;
        if (e->expires_at > 0 && now > e->expires_at) {
            e->valid = 0;
            continue;
        }
        if (out_data) *out_data = e->data;
        if (out_size) *out_size = e->data_size;
        return 0;
    }
    return -1;
}

void network_cache_set(NetworkContext* ctx, const char* url,
                       const char* data, size_t size,
                       int status_code, const char* content_type) {
    if (!ctx || !url || !data) return;

    // البحث عن مكان في الكاش
    CacheEntry* slot = NULL;

    // البحث عن مدخل موجود أو فارغ
    for (int i = 0; i < ctx->cache_count; i++) {
        if (!ctx->cache[i].valid || strcmp(ctx->cache[i].url, url) == 0) {
            slot = &ctx->cache[i];
            break;
        }
    }

    // إذا لم يوجد مكان، استبدل الأقدم
    if (!slot && ctx->cache_count < NET_MAX_CACHE_ENTRIES) {
        slot = &ctx->cache[ctx->cache_count++];
    }

    if (!slot) {
        // طرد الأقدم
        network_cache_evict_old(ctx);
        if (ctx->cache_count < NET_MAX_CACHE_ENTRIES) {
            slot = &ctx->cache[ctx->cache_count++];
        } else {
            slot = &ctx->cache[0]; // استبدال الأول
        }
    }

    // تحرير البيانات القديمة
    if (slot->data) free(slot->data);

    slot->data = (char*)malloc(size + 1);
    if (!slot->data) return;

    memcpy(slot->data, data, size);
    slot->data[size]  = '\0';
    slot->data_size   = size;
    slot->status_code = status_code;
    slot->cached_at   = time(NULL);
    slot->expires_at  = slot->cached_at + NET_CACHE_MAX_AGE;
    slot->valid       = 1;
    strncpy(slot->url,          url,          NET_MAX_URL_LEN - 1);
    strncpy(slot->content_type, content_type, 127);

    ctx->cache_size += size;
}

void network_cache_clear(NetworkContext* ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->cache_count; i++) {
        if (ctx->cache[i].data) { free(ctx->cache[i].data); ctx->cache[i].data = NULL; }
        ctx->cache[i].valid = 0;
    }
    ctx->cache_count = 0;
    ctx->cache_size  = 0;
}

void network_cache_evict_old(NetworkContext* ctx) {
    if (!ctx) return;
    time_t now = time(NULL);
    for (int i = 0; i < ctx->cache_count; i++) {
        if (ctx->cache[i].valid && now > ctx->cache[i].expires_at) {
            if (ctx->cache[i].data) {
                ctx->cache_size -= ctx->cache[i].data_size;
                free(ctx->cache[i].data);
                ctx->cache[i].data = NULL;
            }
            ctx->cache[i].valid = 0;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  الكوكيز
// ═══════════════════════════════════════════════════════════════
void network_cookie_set(NetworkContext* ctx, const char* domain,
                        const char* name, const char* value,
                        time_t expires, int secure) {
    if (!ctx || !domain || !name) return;

    // البحث عن كوكيز موجود
    for (int i = 0; i < ctx->cookie_count; i++) {
        Cookie* c = &ctx->cookies[i];
        if (strcmp(c->domain, domain) == 0 &&
            strcmp(c->name,   name)   == 0) {
            strncpy(c->value, value, NET_MAX_COOKIE_LEN - 1);
            c->expires = expires;
            c->secure  = secure;
            return;
        }
    }

    // إضافة كوكيز جديد
    if (ctx->cookie_count >= NET_MAX_COOKIES) return;
    Cookie* c = &ctx->cookies[ctx->cookie_count++];
    strncpy(c->domain, domain, 255);
    strncpy(c->name,   name,   255);
    strncpy(c->value,  value,  NET_MAX_COOKIE_LEN - 1);
    strncpy(c->path,   "/",    255);
    c->expires   = expires;
    c->secure    = secure;
    c->http_only = 0;
}

const char* network_cookie_get(NetworkContext* ctx, const char* domain,
                                const char* name) {
    if (!ctx || !domain || !name) return NULL;
    time_t now = time(NULL);
    for (int i = 0; i < ctx->cookie_count; i++) {
        Cookie* c = &ctx->cookies[i];
        if (c->expires > 0 && now > c->expires) continue;
        if (strcmp(c->domain, domain) == 0 &&
            strcmp(c->name,   name)   == 0) {
            return c->value;
        }
    }
    return NULL;
}

void network_cookie_clear(NetworkContext* ctx) {
    if (ctx) { memset(ctx->cookies, 0, sizeof(ctx->cookies)); ctx->cookie_count = 0; }
}

void network_cookie_build_header(NetworkContext* ctx, const char* domain,
                                  char* out, size_t out_len) {
    if (!ctx || !domain || !out) return;
    out[0] = '\0';
    size_t pos = 0;
    time_t now = time(NULL);

    for (int i = 0; i < ctx->cookie_count; i++) {
        Cookie* c = &ctx->cookies[i];
        if (c->expires > 0 && now > c->expires) continue;
        if (strstr(domain, c->domain) == NULL) continue;

        if (pos > 0 && pos + 2 < out_len) {
            out[pos++] = ';';
            out[pos++] = ' ';
        }
        int written = snprintf(out + pos, out_len - pos, "%s=%s", c->name, c->value);
        if (written > 0) pos += written;
        if (pos >= out_len - 4) break;
    }
}

// ═══════════════════════════════════════════════════════════════
//  فحص الاتصال
// ═══════════════════════════════════════════════════════════════
int network_check_online(NetworkContext* ctx) {
    if (!ctx) return 0;
    NetCtlInfo info;
    int online = (netCtlGetInfo(NET_CTL_INFO_IP_ADDRESS, &info) == 0);
    ctx->online = online;
    return online;
}

f32 network_get_speed(NetworkContext* ctx) {
    return ctx ? ctx->avg_speed_kbps : 0.0f;
}

const char* network_get_ip(NetworkContext* ctx) {
    static char ip[32] = "غير متصل";
    if (!ctx) return ip;
    NetCtlInfo info;
    if (netCtlGetInfo(NET_CTL_INFO_IP_ADDRESS, &info) == 0) {
        strncpy(ip, info.ip_address, 31);
    }
    return ip;
}

// ═══════════════════════════════════════════════════════════════
//  أدوات HTTP
// ═══════════════════════════════════════════════════════════════
const char* http_method_str(HTTPMethod m) {
    switch (m) {
        case HTTP_GET:     return "GET";
        case HTTP_POST:    return "POST";
        case HTTP_PUT:     return "PUT";
        case HTTP_DELETE:  return "DELETE";
        case HTTP_HEAD:    return "HEAD";
        case HTTP_OPTIONS: return "OPTIONS";
        default:           return "GET";
    }
}

void http_add_header(HTTPRequest* req, const char* name, const char* value) {
    if (!req || req->req_header_count >= NET_MAX_HEADERS) return;
    HTTPHeader* h = &req->req_headers[req->req_header_count++];
    strncpy(h->name,  name,  NET_MAX_HEADER_LEN - 1);
    strncpy(h->value, value, NET_MAX_HEADER_LEN - 1);
}

const char* http_get_header(HTTPHeader* headers, int count, const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(headers[i].name, name) == 0) return headers[i].value;
    }
    return NULL;
}

int url_is_local(const char* url) {
    if (!url) return 0;
    return strncmp(url, "ps3://", 6) == 0 ||
           strncmp(url, "file://", 7) == 0 ||
           strncmp(url, "/dev_hdd", 8) == 0;
}

int url_is_absolute(const char* url) {
    return url && (strncmp(url, "http://", 7) == 0 ||
                   strncmp(url, "https://", 8) == 0);
}

void url_resolve(const char* base, const char* rel, char* out, size_t out_len) {
    if (!base || !rel || !out) return;
    if (url_is_absolute(rel)) { strncpy(out, rel, out_len - 1); return; }
    if (rel[0] == '/') {
        // رابط مطلق من الجذر
        char host[256]; u16 port;
        url_parse(base, host, &port, NULL, NULL);
        snprintf(out, out_len, "http://%s%s", host, rel);
    } else {
        // رابط نسبي
        const char* last_slash = strrchr(base, '/');
        if (last_slash && last_slash > base + 7) {
            size_t base_len = last_slash - base + 1;
            size_t copy = base_len < out_len ? base_len : out_len - 1;
            memcpy(out, base, copy);
            out[copy] = '\0';
            strncat(out, rel, out_len - copy - 1);
        } else {
            snprintf(out, out_len, "%s/%s", base, rel);
        }
    }
}

int network_get(NetworkContext* ctx, const char* url,
                void (*on_done)(int, int, const char*, size_t), void* user_data) {
    return network_fetch_url(ctx, url, NULL);
}

int network_post(NetworkContext* ctx, const char* url,
                 const char* body, size_t body_size,
                 void (*on_done)(int, int, const char*, size_t), void* user_data) {
    return network_fetch_url(ctx, url, NULL);
}

int network_cancel(NetworkContext* ctx, int request_id) {
    if (!ctx) return -1;
    for (int i = 0; i < NET_MAX_REQUESTS; i++) {
        if (ctx->active_requests[i].id == request_id) {
            ctx->active_requests[i].state = REQUEST_CANCELLED;
            if (ctx->active_requests[i].socket_fd > 0) {
                close(ctx->active_requests[i].socket_fd);
                ctx->active_requests[i].socket_fd = -1;
            }
            return 0;
        }
    }
    return -1;
}
