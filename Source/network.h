#pragma once
#ifndef NETWORK_H
#define NETWORK_H

#include "browser.h"
#include <net/net.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// ═══════════════════════════════════════════════════════════════
//  ثوابت الشبكة
// ═══════════════════════════════════════════════════════════════
#define NET_MAX_REQUESTS        16
#define NET_MAX_HEADERS         32
#define NET_MAX_HEADER_LEN      512
#define NET_MAX_RESPONSE_SIZE   (8 * 1024 * 1024)
#define NET_CONNECT_TIMEOUT     10000
#define NET_READ_TIMEOUT        30000
#define NET_MAX_REDIRECTS       5
#define NET_CHUNK_SIZE          (64 * 1024)
#define NET_MAX_COOKIES         256
#define NET_MAX_COOKIE_LEN      1024
#define NET_MAX_URL_LEN         2048
#define NET_MAX_CACHE_ENTRIES   64
#define NET_CACHE_MAX_AGE       3600
#define NET_MAX_QUEUE           32

// ═══════════════════════════════════════════════════════════════
//  حالات الطلب
// ═══════════════════════════════════════════════════════════════
typedef enum {
    REQUEST_IDLE = 0,
    REQUEST_CONNECTING,
    REQUEST_SENDING,
    REQUEST_RECEIVING_HEADERS,
    REQUEST_RECEIVING_BODY,
    REQUEST_DONE,
    REQUEST_ERROR,
    REQUEST_CANCELLED,
    REQUEST_TIMEOUT
} RequestState;

// ═══════════════════════════════════════════════════════════════
//  أنواع الطلبات
// ═══════════════════════════════════════════════════════════════
typedef enum {
    HTTP_GET = 0,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_HEAD,
    HTTP_OPTIONS
} HTTPMethod;

// ═══════════════════════════════════════════════════════════════
//  هيكل رأس HTTP
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char name [NET_MAX_HEADER_LEN];
    char value[NET_MAX_HEADER_LEN];
} HTTPHeader;

// ═══════════════════════════════════════════════════════════════
//  هيكل الكوكيز
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    name    [256];
    char    value   [NET_MAX_COOKIE_LEN];
    char    domain  [256];
    char    path    [256];
    time_t  expires;
    int     secure;
    int     http_only;
    int     same_site;
} Cookie;

// ═══════════════════════════════════════════════════════════════
//  هيكل إدخال الكاش
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char    url         [NET_MAX_URL_LEN];
    char*   data;
    size_t  data_size;
    int     status_code;
    char    content_type[128];
    time_t  cached_at;
    time_t  expires_at;
    u32     etag_hash;
    int     valid;
} CacheEntry;

// ═══════════════════════════════════════════════════════════════
//  هيكل الطلب HTTP
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // الطلب
    HTTPMethod      method;
    char            url         [NET_MAX_URL_LEN];
    char            host        [256];
    char            path        [NET_MAX_URL_LEN];
    char            query       [NET_MAX_URL_LEN];
    u16             port;
    int             use_https;

    // الرؤوس المُرسَلة
    HTTPHeader      req_headers [NET_MAX_HEADERS];
    int             req_header_count;

    // جسم الطلب
    char*           body;
    size_t          body_size;

    // الاستجابة
    int             status_code;
    char            status_text [64];
    HTTPHeader      res_headers [NET_MAX_HEADERS];
    int             res_header_count;
    char*           response;
    size_t          response_size;
    size_t          response_received;
    char            content_type[128];
    s64             content_length;
    int             chunked;

    // الحالة
    RequestState    state;
    int             socket_fd;
    int             redirect_count;
    char            redirect_url[NET_MAX_URL_LEN];

    // التوقيت
    u64             start_time;
    u64             connect_time;
    u64             first_byte_time;
    u64             end_time;

    // التقدم
    f32             progress;
    size_t          bytes_downloaded;
    size_t          bytes_total;
    f32             speed_kbps;

    // معرف الطلب
    int             id;
    int             priority;

    // رد النداء
    void            (*on_progress)(int id, f32 progress, size_t received, size_t total);
    void            (*on_done)    (int id, int status, const char* data, size_t size);
    void            (*on_error)   (int id, const char* error_msg);
    void*           user_data;

    // الذاكرة
    void*           memory_pool;

} HTTPRequest;

// ═══════════════════════════════════════════════════════════════
//  طابور الطلبات
// ═══════════════════════════════════════════════════════════════
typedef struct {
    HTTPRequest*    requests[NET_MAX_QUEUE];
    int             head;
    int             tail;
    int             count;
} RequestQueue;

// ═══════════════════════════════════════════════════════════════
//  هيكل سياق الشبكة
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // الخادم الوسيط
    char            server_url  [NET_MAX_URL_LEN];
    char            server_host [256];
    u16             server_port;

    // الطلبات الفعّالة
    HTTPRequest     active_requests[NET_MAX_REQUESTS];
    int             active_count;

    // طابور الانتظار
    RequestQueue    queue;

    // الكوكيز
    Cookie          cookies     [NET_MAX_COOKIES];
    int             cookie_count;

    // الكاش
    CacheEntry      cache       [NET_MAX_CACHE_ENTRIES];
    int             cache_count;
    size_t          cache_size;
    size_t          cache_max_size;

    // الإعدادات
    char            user_agent  [512];
    int             follow_redirects;
    int             verify_ssl;
    int             use_cache;
    int             use_cookies;
    int             max_connections;
    int             timeout_ms;

    // الإحصائيات
    u64             total_bytes_down;
    u64             total_bytes_up;
    u32             total_requests;
    u32             failed_requests;
    u32             cached_hits;
    f32             avg_speed_kbps;

    // حالة الاتصال
    int             online;
    int             connection_type;
    f32             signal_strength;

    // الذاكرة
    void*           memory_pool;
    size_t          pool_size;

    // الطلب الحالي للتصفح
    int             current_fetch_id;
    int             current_fetch_done;
    char*           current_fetch_data;
    size_t          current_fetch_size;
    int             current_fetch_status;

} NetworkContext;

// ═══════════════════════════════════════════════════════════════
//  واجهة برمجة الشبكة
// ═══════════════════════════════════════════════════════════════
#ifdef __cplusplus
extern "C" {
#endif

// تهيئة وإيقاف
NetworkContext*  network_init            (const char* server_url, void* pool);
void             network_shutdown        (NetworkContext* ctx);
void             network_process_queue   (NetworkContext* ctx);

// طلبات HTTP
int              network_fetch_url       (NetworkContext* ctx, const char* url,
                                          void* tab);
int              network_get             (NetworkContext* ctx, const char* url,
                                          void (*on_done)(int, int, const char*, size_t),
                                          void* user_data);
int              network_post            (NetworkContext* ctx, const char* url,
                                          const char* body, size_t body_size,
                                          void (*on_done)(int, int, const char*, size_t),
                                          void* user_data);
int              network_cancel          (NetworkContext* ctx, int request_id);
int              network_is_fetch_done   (NetworkContext* ctx);
const char*      network_get_fetch_data  (NetworkContext* ctx, size_t* out_size);

// الكاش
int              network_cache_get       (NetworkContext* ctx, const char* url,
                                          char** out_data, size_t* out_size);
void             network_cache_set       (NetworkContext* ctx, const char* url,
                                          const char* data, size_t size,
                                          int status_code, const char* content_type);
void             network_cache_clear     (NetworkContext* ctx);
void             network_cache_evict_old (NetworkContext* ctx);

// الكوكيز
void             network_cookie_set      (NetworkContext* ctx, const char* domain,
                                          const char* name, const char* value,
                                          time_t expires, int secure);
const char*      network_cookie_get      (NetworkContext* ctx, const char* domain,
                                          const char* name);
void             network_cookie_delete   (NetworkContext* ctx, const char* domain,
                                          const char* name);
void             network_cookie_clear    (NetworkContext* ctx);
void             network_cookie_build_header(NetworkContext* ctx, const char* domain,
                                          char* out, size_t out_len);

// أدوات URL
void             url_parse               (const char* url, char* host, u16* port,
                                          char* path, int* use_https);
void             url_encode              (const char* in, char* out, size_t out_len);
void             url_decode              (const char* in, char* out, size_t out_len);
void             url_build_proxy         (NetworkContext* ctx, const char* target_url,
                                          char* out, size_t out_len);
int              url_is_local            (const char* url);

// أدوات HTTP
const char*      http_method_str         (HTTPMethod m);
int              http_parse_headers      (const char* raw, size_t len,
                                          HTTPHeader* out, int max,
                                          int* status_code, char* status_text);
const char*      http_get_header         (HTTPHeader* headers, int count,
                                          const char* name);
void             http_add_header         (HTTPRequest* req, const char* name,
                                          const char* value);
void             http_build_request      (HTTPRequest* req, char* out,
                                          size_t out_len);

// فحص الاتصال
int              network_check_online    (NetworkContext* ctx);
f32              network_get_speed       (NetworkContext* ctx);
const char*      network_get_ip          (NetworkContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_H
