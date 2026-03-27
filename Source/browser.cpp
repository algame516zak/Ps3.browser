// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - browser.cpp
//  المحرك الأساسي: تحليل HTML/CSS/JS + DOM + تخطيط + تصيير
// ═══════════════════════════════════════════════════════════════

#include "browser.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════
//  أدوات مساعدة داخلية
// ═══════════════════════════════════════════════════════════════

void str_trim(char* str) {
    if (!str) return;
    char* start = str;
    while (*start && isspace((u8)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((u8)*end)) *end-- = '\0';
}

void str_to_lower(char* str) {
    if (!str) return;
    for (; *str; str++) *str = (char)tolower((u8)*str);
}

int str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t sl = strlen(str), xl = strlen(suffix);
    if (xl > sl) return 0;
    return strcmp(str + sl - xl, suffix) == 0;
}

f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
f32 clampf(f32 v, f32 mn, f32 mx) { return v < mn ? mn : (v > mx ? mx : v); }
int clampi(int  v, int  mn, int  mx) { return v < mn ? mn : (v > mx ? mx : v); }

u32 color_to_u32(Color c) {
    return ((u32)c.a << 24) | ((u32)c.r << 16) | ((u32)c.g << 8) | c.b;
}
Color u32_to_color(u32 v) {
    return (Color){ (u8)(v >> 16), (u8)(v >> 8), (u8)v, (u8)(v >> 24) };
}
Color color_mix(Color a, Color b, f32 t) {
    return (Color){
        (u8)(a.r + (b.r - a.r) * t),
        (u8)(a.g + (b.g - a.g) * t),
        (u8)(a.b + (b.b - a.b) * t),
        (u8)(a.a + (b.a - a.a) * t)
    };
}
Color color_alpha_blend(Color src, Color dst) {
    f32 sa = src.a / 255.0f, da = 1.0f - sa;
    return (Color){
        (u8)(src.r * sa + dst.r * da),
        (u8)(src.g * sa + dst.g * da),
        (u8)(src.b * sa + dst.b * da),
        255
    };
}

int url_is_absolute(const char* url) {
    return str_starts_with(url, "http://")  ||
           str_starts_with(url, "https://") ||
           str_starts_with(url, "ps3://")   ||
           str_starts_with(url, "data:");
}

void url_resolve(const char* base, const char* rel, char* out, size_t out_len) {
    if (!base || !rel || !out) return;
    if (url_is_absolute(rel)) { strncpy(out, rel, out_len - 1); return; }
    if (str_starts_with(rel, "//")) {
        const char* scheme_end = strchr(base, ':');
        if (scheme_end) {
            size_t scheme_len = (size_t)(scheme_end - base) + 1;
            strncpy(out, base, MIN(scheme_len, out_len - 1));
            strncat(out, rel, out_len - strlen(out) - 1);
        } else strncpy(out, rel, out_len - 1);
        return;
    }
    if (rel[0] == '/') {
        const char* host_start = strstr(base, "://");
        if (host_start) {
            host_start += 3;
            const char* host_end = strchr(host_start, '/');
            if (host_end) {
                size_t host_part = (size_t)(host_end - base);
                strncpy(out, base, MIN(host_part, out_len - 1));
                out[MIN(host_part, out_len - 1)] = '\0';
            } else strncpy(out, base, out_len - 1);
        } else out[0] = '\0';
        strncat(out, rel, out_len - strlen(out) - 1);
        return;
    }
    strncpy(out, base, out_len - 1);
    char* last_slash = strrchr(out, '/');
    if (last_slash) *(last_slash + 1) = '\0'; else out[0] = '\0';
    strncat(out, rel, out_len - strlen(out) - 1);
}

void html_decode_entities(const char* in, char* out, size_t out_len) {
    if (!in || !out) return;
    size_t i = 0, j = 0;
    while (in[i] && j < out_len - 1) {
        if (in[i] == '&') {
            if      (strncmp(in+i,"&amp;", 5)==0) { out[j++]='&';  i+=5; }
            else if (strncmp(in+i,"&lt;",  4)==0) { out[j++]='<';  i+=4; }
            else if (strncmp(in+i,"&gt;",  4)==0) { out[j++]='>';  i+=4; }
            else if (strncmp(in+i,"&quot;",6)==0) { out[j++]='"';  i+=6; }
            else if (strncmp(in+i,"&apos;",6)==0) { out[j++]='\''; i+=6; }
            else if (strncmp(in+i,"&nbsp;",6)==0) { out[j++]=' ';  i+=6; }
            else out[j++] = in[i++];
        } else out[j++] = in[i++];
    }
    out[j] = '\0';
}

// ═══════════════════════════════════════════════════════════════
//  جدول العلامات
// ═══════════════════════════════════════════════════════════════
static const struct { const char* tag; NodeType type; } s_tag_map[] = {
    {"html",NODE_HTML},{"head",NODE_HEAD},{"body",NODE_BODY},
    {"div",NODE_DIV},{"span",NODE_SPAN},{"p",NODE_P},
    {"a",NODE_A},{"img",NODE_IMG},
    {"h1",NODE_H1},{"h2",NODE_H2},{"h3",NODE_H3},
    {"h4",NODE_H4},{"h5",NODE_H5},{"h6",NODE_H6},
    {"ul",NODE_UL},{"ol",NODE_OL},{"li",NODE_LI},
    {"table",NODE_TABLE},{"tr",NODE_TR},{"td",NODE_TD},
    {"th",NODE_TH},{"thead",NODE_THEAD},{"tbody",NODE_TBODY},
    {"form",NODE_FORM},{"input",NODE_INPUT},{"button",NODE_BUTTON},
    {"select",NODE_SELECT},{"option",NODE_OPTION},
    {"textarea",NODE_TEXTAREA},{"label",NODE_LABEL},
    {"header",NODE_HEADER},{"footer",NODE_FOOTER},
    {"main",NODE_MAIN},{"section",NODE_SECTION},
    {"article",NODE_ARTICLE},{"nav",NODE_NAV},{"aside",NODE_ASIDE},
    {"script",NODE_SCRIPT},{"style",NODE_STYLE},
    {"link",NODE_LINK},{"meta",NODE_META},{"title",NODE_TITLE},
    {"br",NODE_BR},{"hr",NODE_HR},
    {"strong",NODE_STRONG},{"em",NODE_EM},{"b",NODE_B},{"i",NODE_I},
    {"u",NODE_U},{"s",NODE_S},{"code",NODE_CODE},{"pre",NODE_PRE},
    {"blockquote",NODE_BLOCKQUOTE},
    {"video",NODE_VIDEO},{"audio",NODE_AUDIO},{"source",NODE_SOURCE},
    {"canvas",NODE_CANVAS},{"svg",NODE_SVG},{"iframe",NODE_IFRAME},
    {NULL,NODE_UNKNOWN}
};

NodeType html_tag_to_type(const char* tag) {
    if (!tag) return NODE_UNKNOWN;
    char lower[HTML_MAX_TAG_LEN];
    strncpy(lower, tag, sizeof(lower)-1); lower[sizeof(lower)-1]='\0';
    str_to_lower(lower);
    for (int i = 0; s_tag_map[i].tag; i++)
        if (strcmp(lower, s_tag_map[i].tag) == 0) return s_tag_map[i].type;
    return NODE_UNKNOWN;
}
const char* html_type_to_tag(NodeType type) {
    for (int i = 0; s_tag_map[i].tag; i++)
        if (s_tag_map[i].type == type) return s_tag_map[i].tag;
    return "unknown";
}

// ═══════════════════════════════════════════════════════════════
//  تهيئة المحرك
// ═══════════════════════════════════════════════════════════════
BrowserEngine* browser_engine_create(void* memory_pool, size_t pool_size) {
    (void)pool_size;
    BrowserEngine* e = (BrowserEngine*)memory_pool;
    if (!e) return NULL;
    memset(e, 0, sizeof(BrowserEngine));
    e->js_engine.enabled     = 1;
    e->image_cache.max_size  = 16*1024*1024;
    css_apply_user_agent(e);
    BROWSER_LOG("تم إنشاء محرك المتصفح");
    return e;
}
void browser_engine_destroy(BrowserEngine* e) {
    if (!e) return;
    image_cache_clear(&e->image_cache);
    BROWSER_LOG("تم تدمير محرك المتصفح");
}
void browser_engine_reset(BrowserEngine* e) {
    if (!e) return;
    e->node_count=0; e->stylesheet_count=0; e->event_count=0;
    e->page_loaded=0; e->page_loading=1; e->load_progress=0.0f;
    e->document=e->body=e->head=NULL;
    memset(e->node_pool,0,sizeof(e->node_pool));
    image_cache_clear(&e->image_cache);
}

// ═══════════════════════════════════════════════════════════════
//  تخصيص عُقدة
// ═══════════════════════════════════════════════════════════════
HTMLNode* html_alloc_node(BrowserEngine* e, NodeType type) {
    if (e->node_count >= HTML_MAX_NODES) return NULL;
    HTMLNode* node = &e->node_pool[e->node_count];
    memset(node, 0, sizeof(HTMLNode));
    node->type       = type;
    node->node_index = e->node_count++;
    return node;
}

const char* html_get_attr(HTMLNode* node, const char* name) {
    if (!node||!name) return NULL;
    for (int i=0;i<node->attr_count;i++)
        if (strcmp(node->attrs[i].name,name)==0) return node->attrs[i].value;
    return NULL;
}
void html_set_attr(HTMLNode* node, const char* name, const char* value) {
    if (!node||!name||!value) return;
    for (int i=0;i<node->attr_count;i++) {
        if (strcmp(node->attrs[i].name,name)==0) {
            strncpy(node->attrs[i].value,value,HTML_MAX_ATTR_LEN-1); return;
        }
    }
    if (node->attr_count<HTML_MAX_ATTRS) {
        strncpy(node->attrs[node->attr_count].name, name, HTML_MAX_ATTR_LEN-1);
        strncpy(node->attrs[node->attr_count].value,value,HTML_MAX_ATTR_LEN-1);
        node->attr_count++;
    }
}

// ═══════════════════════════════════════════════════════════════
//  محلل HTML
// ═══════════════════════════════════════════════════════════════
typedef struct {
    const char*    src;
    size_t         pos, len;
    BrowserEngine* engine;
} HTMLParser;

static char  p_peek(HTMLParser* p){ return p->pos<p->len?p->src[p->pos]:0; }
static char  p_next(HTMLParser* p){ return p->pos<p->len?p->src[p->pos++]:0; }
static void  p_skip_ws(HTMLParser* p){ while(p->pos<p->len&&isspace((u8)p->src[p->pos]))p->pos++; }

static void p_read_name(HTMLParser* p, char* out, size_t ol) {
    size_t i=0;
    while(p->pos<p->len && i<ol-1) {
        char c=p->src[p->pos];
        if(isalnum((u8)c)||c=='-'||c=='_'||c==':') { out[i++]=(char)tolower((u8)c); p->pos++; }
        else break;
    }
    out[i]='\0';
}
static void p_read_attr_val(HTMLParser* p, char* out, size_t ol) {
    size_t i=0; p_skip_ws(p);
    if(p_peek(p)!='='){ out[0]='\0'; return; }
    p->pos++; p_skip_ws(p);
    char q=p_peek(p);
    if(q=='"'||q=='\'') {
        p->pos++;
        while(p->pos<p->len&&p->src[p->pos]!=q&&i<ol-1) out[i++]=p->src[p->pos++];
        if(p->pos<p->len)p->pos++;
    } else {
        while(p->pos<p->len&&!isspace((u8)p->src[p->pos])&&p->src[p->pos]!='>'&&i<ol-1)
            out[i++]=p->src[p->pos++];
    }
    out[i]='\0';
}
static void p_apply_attrs(HTMLNode* n) {
    const char* v;
    if((v=html_get_attr(n,"id")))          strncpy(n->id,         v,HTML_MAX_ID_LEN-1);
    if((v=html_get_attr(n,"class")))       strncpy(n->class_name, v,HTML_MAX_CLASS_LEN-1);
    if((v=html_get_attr(n,"style")))       strncpy(n->style_str,  v,HTML_MAX_STYLE_LEN-1);
    if((v=html_get_attr(n,"href")))        strncpy(n->href,       v,HTML_MAX_HREF_LEN-1);
    if((v=html_get_attr(n,"src")))         strncpy(n->src,        v,HTML_MAX_SRC_LEN-1);
    if((v=html_get_attr(n,"alt")))         strncpy(n->alt,        v,HTML_MAX_ATTR_LEN-1);
    if((v=html_get_attr(n,"value")))       strncpy(n->value,      v,HTML_MAX_ATTR_LEN-1);
    if((v=html_get_attr(n,"placeholder"))) strncpy(n->placeholder,v,HTML_MAX_ATTR_LEN-1);
    if((v=html_get_attr(n,"type")))        strncpy(n->input_type, v,HTML_MAX_TAG_LEN-1);
    if((v=html_get_attr(n,"onclick")))     strncpy(n->onclick,    v,HTML_MAX_ATTR_LEN-1);
    if(html_get_attr(n,"hidden"))   n->hidden=1;
    if(html_get_attr(n,"disabled")) n->disabled=1;
    if(html_get_attr(n,"checked"))  n->checked=1;
}

static HTMLNode* parse_node(HTMLParser* p, HTMLNode* parent, int depth);

static void parse_children(HTMLParser* p, HTMLNode* parent, int depth) {
    if(depth>HTML_MAX_DEPTH) return;
    while(p->pos<p->len) {
        p_skip_ws(p);
        if(p->pos>=p->len) break;
        if(p->src[p->pos]=='<'&&p->pos+1<p->len&&p->src[p->pos+1]=='/') break;
        HTMLNode* child=parse_node(p,parent,depth+1);
        if(!child) break;
        if(parent&&parent->child_count<HTML_MAX_CHILDREN) {
            parent->children[parent->child_count++]=child;
            child->parent=parent;
        }
    }
}

static HTMLNode* parse_node(HTMLParser* p, HTMLNode* parent, int depth) {
    p_skip_ws(p);
    if(p->pos>=p->len) return NULL;
    // تعليق
    if(strncmp(p->src+p->pos,"<!--",4)==0) {
        p->pos+=4;
        while(p->pos+2<p->len&&strncmp(p->src+p->pos,"-->",3)!=0) p->pos++;
        p->pos+=3; return NULL;
    }
    // نص
    if(p->src[p->pos]!='<') {
        HTMLNode* n=html_alloc_node(p->engine,NODE_TEXT);
        if(!n) return NULL;
        size_t i=0;
        while(p->pos<p->len&&p->src[p->pos]!='<'&&i<HTML_MAX_TEXT_LEN-1)
            n->text[i++]=p->src[p->pos++];
        n->text[i]='\0'; str_trim(n->text);
        if(!n->text[0]) return NULL;
        html_decode_entities(n->text,n->text,HTML_MAX_TEXT_LEN);
        return n;
    }
    p->pos++;
    // DOCTYPE
    if(strncasecmp(p->src+p->pos,"!DOCTYPE",8)==0||
       strncasecmp(p->src+p->pos,"!doctype",8)==0) {
        while(p->pos<p->len&&p->src[p->pos]!='>') p->pos++;
        if(p->pos<p->len)p->pos++; return NULL;
    }
    // علامة إغلاق
    if(p_peek(p)=='/') {
        while(p->pos<p->len&&p->src[p->pos]!='>') p->pos++;
        if(p->pos<p->len)p->pos++; return NULL;
    }
    char tag[HTML_MAX_TAG_LEN]={0};
    p_read_name(p,tag,HTML_MAX_TAG_LEN);
    if(!tag[0]) {
        while(p->pos<p->len&&p->src[p->pos]!='>') p->pos++;
        if(p->pos<p->len)p->pos++; return NULL;
    }
    HTMLNode* node=html_alloc_node(p->engine,html_tag_to_type(tag));
    if(!node) return NULL;
    strncpy(node->tag,tag,HTML_MAX_TAG_LEN-1);

    while(p->pos<p->len&&p->src[p->pos]!='>'&&p->src[p->pos]!='/') {
        p_skip_ws(p);
        if(p->pos>=p->len||p->src[p->pos]=='>'||p->src[p->pos]=='/') break;
        char an[HTML_MAX_ATTR_LEN]={0}; size_t ai=0;
        while(p->pos<p->len&&!isspace((u8)p->src[p->pos])&&
              p->src[p->pos]!='='&&p->src[p->pos]!='>'&&
              p->src[p->pos]!='/'&&ai<HTML_MAX_ATTR_LEN-1)
            an[ai++]=(char)tolower((u8)p->src[p->pos++]);
        an[ai]='\0';
        char av[HTML_MAX_ATTR_LEN]={0};
        p_read_attr_val(p,av,HTML_MAX_ATTR_LEN);
        if(an[0]) html_set_attr(node,an,av);
    }
    p_apply_attrs(node);

    int self_close=0;
    if(p->pos<p->len&&p->src[p->pos]=='/'){self_close=1;p->pos++;}
    if(p->pos<p->len&&p->src[p->pos]=='>') p->pos++;

    // script / style
    if(node->type==NODE_SCRIPT||node->type==NODE_STYLE) {
        char ct[HTML_MAX_TAG_LEN+3];
        snprintf(ct,sizeof(ct),"</%s",tag);
        size_t start=p->pos;
        while(p->pos<p->len&&strncasecmp(p->src+p->pos,ct,strlen(ct))!=0) p->pos++;
        size_t raw_len=MIN(p->pos-start,(size_t)(HTML_MAX_TEXT_LEN-1));
        strncpy(node->text,p->src+start,raw_len); node->text[raw_len]='\0';
        while(p->pos<p->len&&p->src[p->pos]!='>') p->pos++;
        if(p->pos<p->len)p->pos++;
        if(node->type==NODE_SCRIPT&&p->engine->js_engine.enabled)
            js_execute(&p->engine->js_engine,node->text);
        else if(node->type==NODE_STYLE&&p->engine->stylesheet_count<16)
            css_parse(p->engine,node->text,strlen(node->text),p->engine->stylesheet_count++);
        return node;
    }

    static const char* void_tags[]={"br","hr","img","input","link","meta",
        "source","area","base","col","embed","param","track","wbr",NULL};
    int is_void=self_close;
    if(!is_void) for(int i=0;void_tags[i];i++) if(strcmp(tag,void_tags[i])==0){is_void=1;break;}
    if(!is_void) parse_children(p,node,depth);
    if(p->pos<p->len&&p->src[p->pos]=='<'&&p->pos+1<p->len&&p->src[p->pos+1]=='/') {
        while(p->pos<p->len&&p->src[p->pos]!='>') p->pos++;
        if(p->pos<p->len)p->pos++;
    }
    if(node->type==NODE_HTML)  p->engine->document=node;
    if(node->type==NODE_HEAD)  p->engine->head=node;
    if(node->type==NODE_BODY)  p->engine->body=node;
    if(node->type==NODE_TITLE&&node->child_count>0)
        strncpy(p->engine->current_title,
                p->engine->node_pool[node->children[0]->node_index].text,255);
    return node;
}

int html_parse(BrowserEngine* e, const char* html, size_t len) {
    if(!e||!html||len==0) return -1;
    browser_engine_reset(e);
    HTMLParser parser={html,0,len,e};
    HTMLNode* doc=html_alloc_node(e,NODE_DOCUMENT);
    if(!doc) return -2;
    e->document=doc;
    while(parser.pos<parser.len) {
        HTMLNode* node=parse_node(&parser,doc,0);
        if(node&&doc->child_count<HTML_MAX_CHILDREN){
            doc->children[doc->child_count++]=node; node->parent=doc;
        }
    }
    if(!e->body) e->body=doc;
    e->page_size=len; e->page_loading=0; e->page_loaded=1; e->load_progress=1.0f;
    BROWSER_LOG("تم تحليل %d عُقدة HTML",e->node_count);
    css_compute_styles(e);
    return 0;
}

HTMLNode* html_find_by_id(BrowserEngine* e, const char* id) {
    if(!e||!id) return NULL;
    for(int i=0;i<e->node_count;i++)
        if(strcmp(e->node_pool[i].id,id)==0) return &e->node_pool[i];
    return NULL;
}
HTMLNode* html_find_by_tag(BrowserEngine* e, const char* tag, int index) {
    if(!e||!tag) return NULL;
    NodeType t=html_tag_to_type(tag); int count=0;
    for(int i=0;i<e->node_count;i++)
        if(e->node_pool[i].type==t){if(count==index)return &e->node_pool[i];count++;}
    return NULL;
}
int html_find_all_by_class(BrowserEngine* e,const char* cls,HTMLNode** out,int max){
    if(!e||!cls||!out) return 0;
    int found=0;
    for(int i=0;i<e->node_count&&found<max;i++)
        if(e->node_pool[i].class_name[0]&&strstr(e->node_pool[i].class_name,cls))
            out[found++]=&e->node_pool[i];
    return found;
}

// ═══════════════════════════════════════════════════════════════
//  محلل CSS
// ═══════════════════════════════════════════════════════════════
int css_parse_color(const char* str, Color* out) {
    if(!str||!out) return 0;
    char s[64]; strncpy(s,str,63); s[63]='\0'; str_trim(s); str_to_lower(s);
    static const struct{const char* name;u32 val;}named[]={
        {"black",0xFF000000},{"white",0xFFFFFFFF},{"red",0xFFFF0000},
        {"green",0xFF008000},{"blue",0xFF0000FF},{"yellow",0xFFFFFF00},
        {"orange",0xFFFFA500},{"purple",0xFF800080},{"pink",0xFFFFC0CB},
        {"gray",0xFF808080},{"grey",0xFF808080},{"cyan",0xFF00FFFF},
        {"magenta",0xFFFF00FF},{"lime",0xFF00FF00},{"navy",0xFF000080},
        {"teal",0xFF008080},{"silver",0xFFC0C0C0},{"transparent",0x00000000},
        {NULL,0}
    };
    for(int i=0;named[i].name;i++) {
        if(strcmp(s,named[i].name)==0){
            u32 v=named[i].val;
            out->r=(v>>16)&0xFF; out->g=(v>>8)&0xFF;
            out->b=v&0xFF; out->a=(v>>24)&0xFF; return 1;
        }
    }
    if(s[0]=='#'){
        size_t hl=strlen(s)-1;
        unsigned r=0,g=0,b=0,a=255;
        if(hl==3){sscanf(s+1,"%1x%1x%1x",&r,&g,&b);r*=17;g*=17;b*=17;}
        else if(hl==6) sscanf(s+1,"%2x%2x%2x",&r,&g,&b);
        else if(hl==8) sscanf(s+1,"%2x%2x%2x%2x",&r,&g,&b,&a);
        else return 0;
        *out=(Color){(u8)r,(u8)g,(u8)b,(u8)a}; return 1;
    }
    if(str_starts_with(s,"rgb")){
        unsigned r=0,g=0,b=0; float a=1.0f;
        if(str_starts_with(s,"rgba")) sscanf(s,"rgba(%u,%u,%u,%f)",&r,&g,&b,&a);
        else sscanf(s,"rgb(%u,%u,%u)",&r,&g,&b);
        *out=(Color){(u8)r,(u8)g,(u8)b,(u8)(a*255)}; return 1;
    }
    return 0;
}

CSSValue css_parse_value(const char* str) {
    if(!str) return CSS_NONE;
    char s[CSS_MAX_VALUE_LEN]; strncpy(s,str,sizeof(s)-1); s[sizeof(s)-1]='\0'; str_trim(s);
    if(strcmp(s,"auto")==0) return CSS_AUTO;
    if(strcmp(s,"none")==0) return CSS_NONE;
    if(strcmp(s,"0")   ==0) return CSS_PX(0);
    float v=0.0f;
    if(str_ends_with(s,"px")) {sscanf(s,"%f",&v);return CSS_PX(v);}
    if(str_ends_with(s,"%"))  {sscanf(s,"%f",&v);return CSS_PCT(v);}
    if(str_ends_with(s,"em")) {sscanf(s,"%f",&v);return CSS_EM(v);}
    if(str_ends_with(s,"rem")){sscanf(s,"%f",&v);return (CSSValue){v,UNIT_REM};}
    if(str_ends_with(s,"vw")) {sscanf(s,"%f",&v);return (CSSValue){v,UNIT_VW};}
    if(str_ends_with(s,"vh")) {sscanf(s,"%f",&v);return (CSSValue){v,UNIT_VH};}
    sscanf(s,"%f",&v); return CSS_PX(v);
}
f32 css_resolve_value(CSSValue v, f32 parent_size, f32 vp_size) {
    switch(v.unit){
        case UNIT_PX:      return v.value;
        case UNIT_PERCENT: return parent_size*v.value/100.0f;
        case UNIT_EM:      return v.value*16.0f;
        case UNIT_REM:     return v.value*16.0f;
        case UNIT_VW:      return vp_size*v.value/100.0f;
        case UNIT_VH:      return vp_size*v.value/100.0f;
        case UNIT_AUTO:    return -1.0f;
        default:           return 0.0f;
    }
}
static void css_parse_box(const char* val, CSSBox* box) {
    char s[CSS_MAX_VALUE_LEN]; strncpy(s,val,sizeof(s)-1); s[sizeof(s)-1]='\0';
    char* t[4]; int tc=0; char* tok=strtok(s," ");
    while(tok&&tc<4){t[tc++]=tok;tok=strtok(NULL," ");}
    if     (tc==1) box->top=box->right=box->bottom=box->left=css_parse_value(t[0]);
    else if(tc==2){box->top=box->bottom=css_parse_value(t[0]);box->right=box->left=css_parse_value(t[1]);}
    else if(tc==3){box->top=css_parse_value(t[0]);box->right=box->left=css_parse_value(t[1]);box->bottom=css_parse_value(t[2]);}
    else if(tc==4){box->top=css_parse_value(t[0]);box->right=css_parse_value(t[1]);box->bottom=css_parse_value(t[2]);box->left=css_parse_value(t[3]);}
}
static void css_apply_property(CSSStyle* st, const char* prop, const char* val) {
    if(!prop||!val) return;
    char p[CSS_MAX_PROPERTY_LEN],v[CSS_MAX_VALUE_LEN];
    strncpy(p,prop,sizeof(p)-1); p[sizeof(p)-1]='\0';
    strncpy(v,val, sizeof(v)-1); v[sizeof(v)-1]='\0';
    str_trim(p); str_trim(v); str_to_lower(p);

    if(!strcmp(p,"display")){
        if(!strcmp(v,"none"))         st->display=DISPLAY_NONE;
        else if(!strcmp(v,"block"))   st->display=DISPLAY_BLOCK;
        else if(!strcmp(v,"inline"))  st->display=DISPLAY_INLINE;
        else if(!strcmp(v,"inline-block")) st->display=DISPLAY_INLINE_BLOCK;
        else if(!strcmp(v,"flex"))    st->display=DISPLAY_FLEX;
        else if(!strcmp(v,"grid"))    st->display=DISPLAY_GRID;
    }
    else if(!strcmp(p,"position")){
        if(!strcmp(v,"relative")) st->position=POSITION_RELATIVE;
        else if(!strcmp(v,"absolute")) st->position=POSITION_ABSOLUTE;
        else if(!strcmp(v,"fixed"))    st->position=POSITION_FIXED;
        else if(!strcmp(v,"sticky"))   st->position=POSITION_STICKY;
    }
    else if(!strcmp(p,"width"))         st->width      =css_parse_value(v);
    else if(!strcmp(p,"height"))        st->height     =css_parse_value(v);
    else if(!strcmp(p,"min-width"))     st->min_width  =css_parse_value(v);
    else if(!strcmp(p,"min-height"))    st->min_height =css_parse_value(v);
    else if(!strcmp(p,"max-width"))     st->max_width  =css_parse_value(v);
    else if(!strcmp(p,"max-height"))    st->max_height =css_parse_value(v);
    else if(!strcmp(p,"top"))           st->top        =css_parse_value(v);
    else if(!strcmp(p,"right"))         st->right      =css_parse_value(v);
    else if(!strcmp(p,"bottom"))        st->bottom     =css_parse_value(v);
    else if(!strcmp(p,"left"))          st->left       =css_parse_value(v);
    else if(!strcmp(p,"z-index"))       st->z_index    =atoi(v);
    else if(!strcmp(p,"opacity"))       st->opacity    =(f32)atof(v);
    else if(!strcmp(p,"padding"))       css_parse_box(v,&st->padding);
    else if(!strcmp(p,"padding-top"))   st->padding.top   =css_parse_value(v);
    else if(!strcmp(p,"padding-right")) st->padding.right =css_parse_value(v);
    else if(!strcmp(p,"padding-bottom"))st->padding.bottom=css_parse_value(v);
    else if(!strcmp(p,"padding-left"))  st->padding.left  =css_parse_value(v);
    else if(!strcmp(p,"margin"))        css_parse_box(v,&st->margin);
    else if(!strcmp(p,"margin-top"))    st->margin.top   =css_parse_value(v);
    else if(!strcmp(p,"margin-right"))  st->margin.right =css_parse_value(v);
    else if(!strcmp(p,"margin-bottom")) st->margin.bottom=css_parse_value(v);
    else if(!strcmp(p,"margin-left"))   st->margin.left  =css_parse_value(v);
    else if(!strcmp(p,"color"))         css_parse_color(v,&st->color);
    else if(!strcmp(p,"background-color")||!strcmp(p,"background")){
        if(css_parse_color(v,&st->background.color)) st->background.type=BG_COLOR;
    }
    else if(!strcmp(p,"font-size"))     st->font_size  =css_parse_value(v);
    else if(!strcmp(p,"font-weight")){
        if(!strcmp(v,"bold")) st->font_weight=FONT_WEIGHT_BOLD;
        else st->font_weight=(FontWeight)atoi(v);
    }
    else if(!strcmp(p,"font-style"))    st->font_italic=!strcmp(v,"italic");
    else if(!strcmp(p,"text-align")){
        if(!strcmp(v,"center"))  st->text_align=TEXT_ALIGN_CENTER;
        else if(!strcmp(v,"right"))   st->text_align=TEXT_ALIGN_RIGHT;
        else if(!strcmp(v,"justify")) st->text_align=TEXT_ALIGN_JUSTIFY;
        else st->text_align=TEXT_ALIGN_LEFT;
    }
    else if(!strcmp(p,"flex-direction")){
        if(!strcmp(v,"column"))         st->flex_direction=FLEX_DIR_COLUMN;
        else if(!strcmp(v,"row-reverse"))    st->flex_direction=FLEX_DIR_ROW_REVERSE;
        else if(!strcmp(v,"column-reverse")) st->flex_direction=FLEX_DIR_COLUMN_REVERSE;
        else st->flex_direction=FLEX_DIR_ROW;
    }
    else if(!strcmp(p,"justify-content")){
        if(!strcmp(v,"flex-end"))      st->justify_content=JUSTIFY_FLEX_END;
        else if(!strcmp(v,"center"))   st->justify_content=JUSTIFY_CENTER;
        else if(!strcmp(v,"space-between")) st->justify_content=JUSTIFY_SPACE_BETWEEN;
        else if(!strcmp(v,"space-around"))  st->justify_content=JUSTIFY_SPACE_AROUND;
        else if(!strcmp(v,"space-evenly"))  st->justify_content=JUSTIFY_SPACE_EVENLY;
    }
    else if(!strcmp(p,"align-items")){
        if(!strcmp(v,"flex-end")) st->align_items=ALIGN_FLEX_END;
        else if(!strcmp(v,"center"))  st->align_items=ALIGN_CENTER;
        else if(!strcmp(v,"stretch")) st->align_items=ALIGN_STRETCH;
    }
    else if(!strcmp(p,"flex-grow"))   st->flex_grow  =(f32)atof(v);
    else if(!strcmp(p,"flex-shrink")) st->flex_shrink=(f32)atof(v);
    else if(!strcmp(p,"gap"))         st->gap        =css_parse_value(v);
    else if(!strcmp(p,"border-radius")){
        CSSValue r=css_parse_value(v);
        st->border.radius_tl=st->border.radius_tr=
        st->border.radius_bl=st->border.radius_br=r;
    }
    else if(!strcmp(p,"cursor"))        st->cursor_pointer=!strcmp(v,"pointer");
    else if(!strcmp(p,"overflow")){
        OverflowType ov=OVERFLOW_VISIBLE;
        if(!strcmp(v,"hidden")) ov=OVERFLOW_HIDDEN;
        else if(!strcmp(v,"scroll")) ov=OVERFLOW_SCROLL;
        else if(!strcmp(v,"auto"))   ov=OVERFLOW_AUTO;
        st->overflow_x=st->overflow_y=ov;
    }
    else if(!strcmp(p,"line-height"))   st->line_height=css_parse_value(v);
    else if(!strcmp(p,"transition-duration")) st->transition_duration=(f32)atof(v);
}

int css_parse(BrowserEngine* e, const char* css, size_t len, int sheet_idx) {
    if(!e||!css||len==0||sheet_idx>=16) return -1;
    CSSStyleSheet* sheet=&e->stylesheets[sheet_idx];
    sheet->rule_count=0;
    size_t i=0;
    while(i<len){
        if(i+1<len&&css[i]=='/'&&css[i+1]=='*'){
            i+=2; while(i+1<len&&!(css[i]=='*'&&css[i+1]=='/'))i++; i+=2; continue;
        }
        char sel[CSS_MAX_SELECTOR_LEN]={0}; size_t si=0;
        while(i<len&&css[i]!='{'&&si<CSS_MAX_SELECTOR_LEN-1) sel[si++]=css[i++];
        sel[si]='\0'; str_trim(sel);
        if(!sel[0]||i>=len){i++;continue;}
        i++;
        if(sheet->rule_count>=CSS_MAX_RULES) break;
        CSSRule* rule=&sheet->rules[sheet->rule_count++];
        strncpy(rule->selector,sel,CSS_MAX_SELECTOR_LEN-1);
        rule->prop_count=0;
        while(i<len&&css[i]!='}'){
            while(i<len&&isspace((u8)css[i]))i++;
            if(i>=len||css[i]=='}')break;
            char prop[CSS_MAX_PROPERTY_LEN]={0}; size_t pi=0;
            while(i<len&&css[i]!=':'&&css[i]!='}'&&pi<CSS_MAX_PROPERTY_LEN-1) prop[pi++]=css[i++];
            prop[pi]='\0'; str_trim(prop);
            if(i<len&&css[i]==':')i++;
            char val[CSS_MAX_VALUE_LEN]={0}; size_t vi=0;
            while(i<len&&css[i]!=';'&&css[i]!='}'&&vi<CSS_MAX_VALUE_LEN-1) val[vi++]=css[i++];
            val[vi]='\0'; str_trim(val);
            if(i<len&&css[i]==';')i++;
            if(prop[0]&&val[0]&&rule->prop_count<CSS_MAX_PROPERTIES){
                strncpy(rule->property[rule->prop_count], prop,CSS_MAX_PROPERTY_LEN-1);
                strncpy(rule->value_str[rule->prop_count],val, CSS_MAX_VALUE_LEN-1);
                rule->prop_count++;
            }
        }
        if(i<len&&css[i]=='}')i++;
    }
    return 0;
}

int css_selector_matches(const char* selector, HTMLNode* node) {
    if(!selector||!node) return 0;
    char s[CSS_MAX_SELECTOR_LEN]; strncpy(s,selector,sizeof(s)-1); s[sizeof(s)-1]='\0'; str_trim(s);
    if(s[0]=='*') return 1;
    if(s[0]=='#') return strcmp(node->id,s+1)==0;
    if(s[0]=='.') return strstr(node->class_name,s+1)!=NULL;
    if(strcmp(s,node->tag)==0) return 1;
    char* dot=strchr(s,'.');
    if(dot){*dot='\0';return(strlen(s)==0||strcmp(s,node->tag)==0)&&strstr(node->class_name,dot+1)!=NULL;}
    char* hash=strchr(s,'#');
    if(hash){*hash='\0';return(strlen(s)==0||strcmp(s,node->tag)==0)&&strcmp(node->id,hash+1)==0;}
    return 0;
}

void css_apply_user_agent(BrowserEngine* e) {
    static const char UA[]=
        "*{margin:0;padding:0;}"
        "body{display:block;font-size:16px;color:#000;background:#fff;}"
        "div,p,h1,h2,h3,h4,h5,h6,section,article,main,header,footer,nav,aside{display:block;}"
        "span,a,strong,em,b,i,u,s,code{display:inline;}"
        "h1{font-size:32px;font-weight:bold;margin:16px 0;}"
        "h2{font-size:28px;font-weight:bold;margin:14px 0;}"
        "h3{font-size:24px;font-weight:bold;margin:12px 0;}"
        "p{margin:8px 0;line-height:1.6em;}"
        "a{color:#0066cc;cursor:pointer;}"
        "ul,ol{padding-left:24px;margin:8px 0;}"
        "li{display:list-item;margin:4px 0;}"
        "button{display:inline-block;cursor:pointer;padding:6px 12px;}"
        "input{display:inline-block;padding:4px 8px;}"
        "img{display:inline-block;max-width:100%;}"
        "strong,b{font-weight:bold;}"
        "em,i{font-style:italic;}";
    css_parse(e,UA,strlen(UA),0);
    if(e->stylesheet_count<1) e->stylesheet_count=1;
}

void css_compute_node(BrowserEngine* e, HTMLNode* node, HTMLNode* parent) {
    if(!node) return;
    if(parent) node->computed_style=parent->computed_style;
    else {
        memset(&node->computed_style,0,sizeof(CSSStyle));
        node->computed_style.display=DISPLAY_BLOCK;
        node->computed_style.opacity=1.0f;
        node->computed_style.color=RGB(0,0,0);
        node->computed_style.font_size=CSS_PX(16);
        node->computed_style.font_weight=FONT_WEIGHT_NORMAL;
        node->computed_style.line_height=CSS_EM(1.6f);
        node->computed_style.visible=1;
    }
    for(int si=0;si<e->stylesheet_count;si++){
        CSSStyleSheet* sheet=&e->stylesheets[si];
        for(int ri=0;ri<sheet->rule_count;ri++)
            if(css_selector_matches(sheet->rules[ri].selector,node))
                for(int pi=0;pi<sheet->rules[ri].prop_count;pi++)
                    css_apply_property(&node->computed_style,
                                       sheet->rules[ri].property[pi],
                                       sheet->rules[ri].value_str[pi]);
    }
    if(node->style_str[0]){
        char sc[HTML_MAX_STYLE_LEN]; strncpy(sc,node->style_str,sizeof(sc)-1);
        char* tok=strtok(sc,";");
        while(tok){
            char* colon=strchr(tok,':');
            if(colon){*colon='\0';char* p=tok,*v=colon+1;str_trim(p);str_trim(v);css_apply_property(&node->computed_style,p,v);}
            tok=strtok(NULL,";");
        }
    }
    node->computed_style.visible=(node->computed_style.display!=DISPLAY_NONE)&&!node->hidden;
    node->style_dirty=0;
    for(int i=0;i<node->child_count;i++) css_compute_node(e,node->children[i],node);
}
void css_compute_styles(BrowserEngine* e) {
    if(!e||!e->document) return;
    css_compute_node(e,e->document,NULL);
    e->layout_calls++;
}

// ═══════════════════════════════════════════════════════════════
//  محرك التخطيط
// ═══════════════════════════════════════════════════════════════
void layout_text_wrap(HTMLNode* node,f32 max_w,f32 fs,f32* ow,f32* oh){
    if(!node||!ow||!oh) return;
    float cw=fs*0.55f,lh=fs*1.6f;
    size_t len=strlen(node->text);
    float lw=0,th=lh,mw=0;
    for(size_t i=0;i<len;i++){
        if(node->text[i]=='\n'){if(lw>mw)mw=lw;lw=0;th+=lh;}
        else{lw+=cw;if(lw>max_w&&max_w>0){if(lw>mw)mw=max_w;lw=cw;th+=lh;}}
    }
    if(lw>mw)mw=lw;
    *ow=MIN(mw,max_w>0?max_w:mw); *oh=th;
}

void layout_block(BrowserEngine* e,HTMLNode* node,f32 x,f32 y,f32 w,f32 h){
    if(!node||!node->computed_style.visible) return;
    CSSStyle* s=&node->computed_style;
    f32 vw=e->layout.viewport_width,vh=e->layout.viewport_height;
    f32 pt=css_resolve_value(s->padding.top,   h,vh);
    f32 pr=css_resolve_value(s->padding.right, w,vw);
    f32 pb=css_resolve_value(s->padding.bottom,h,vh);
    f32 pl=css_resolve_value(s->padding.left,  w,vw);
    f32 mt=css_resolve_value(s->margin.top,    h,vh);
    f32 ml=css_resolve_value(s->margin.left,   w,vw);
    f32 nw=css_resolve_value(s->width, w,vw);
    f32 nh=css_resolve_value(s->height,h,vh);
    if(nw<0) nw=w-ml-css_resolve_value(s->margin.right,w,vw);
    if(nh<0) nh=0;
    node->layout_x=x+ml; node->layout_y=y+mt;
    node->layout_w=nw>0?nw:w; node->layout_h=nh;
    f32 cx=node->layout_x+pl,cy=node->layout_y+pt;
    f32 cw=node->layout_w-pl-pr,rh=0;
    for(int i=0;i<node->child_count;i++){
        HTMLNode* c=node->children[i];
        if(!c||!c->computed_style.visible) continue;
        if(c->type==NODE_TEXT){
            f32 tw,th; f32 fs=css_resolve_value(c->computed_style.font_size,16,vw);
            if(fs<=0)fs=16;
            layout_text_wrap(c,cw,fs,&tw,&th);
            c->layout_x=cx;c->layout_y=cy+rh;c->layout_w=tw;c->layout_h=th;rh+=th;
        } else if(c->computed_style.display==DISPLAY_FLEX){
            c->layout_x=cx;c->layout_y=cy+rh;
            layout_flex(e,c);rh+=c->layout_h;
        } else {
            layout_block(e,c,cx,cy+rh,cw,h);
            rh+=c->layout_h+css_resolve_value(c->computed_style.margin.bottom,h,vh);
        }
    }
    if(node->layout_h<=0) node->layout_h=rh+pt+pb;
    else node->layout_h+=pt+pb;
}

void layout_flex(BrowserEngine* e,HTMLNode* node){
    if(!node) return;
    CSSStyle* s=&node->computed_style;
    f32 vw=e->layout.viewport_width,vh=e->layout.viewport_height;
    f32 w=css_resolve_value(s->width,vw,vw),h=css_resolve_value(s->height,vh,vh);
    if(w<0)w=vw; if(h<0)h=0;
    f32 gap=css_resolve_value(s->gap,w,vw);
    int is_row=(s->flex_direction==FLEX_DIR_ROW||s->flex_direction==FLEX_DIR_ROW_REVERSE);
    f32 px=node->layout_x+css_resolve_value(s->padding.left, w,vw);
    f32 py=node->layout_y+css_resolve_value(s->padding.top,  h,vh);
    f32 pw=w-css_resolve_value(s->padding.left,w,vw)-css_resolve_value(s->padding.right,w,vw);
    f32 ph=h-css_resolve_value(s->padding.top,h,vh)-css_resolve_value(s->padding.bottom,h,vh);
    f32 total_flex=0,fixed_space=0; int flex_count=0;
    for(int i=0;i<node->child_count;i++){
        HTMLNode* c=node->children[i];
        if(!c||!c->computed_style.visible) continue;
        f32 fg=c->computed_style.flex_grow;
        if(fg>0){total_flex+=fg;flex_count++;}
        else{
            f32 cw=css_resolve_value(c->computed_style.width, pw,vw);
            f32 ch=css_resolve_value(c->computed_style.height,ph,vh);
            fixed_space+=is_row?(cw>0?cw:100):(ch>0?ch:40);
        }
    }
    (void)flex_count;
    f32 avail=is_row?pw-fixed_space:ph-fixed_space; if(avail<0)avail=0;
    f32 cursor=is_row?px:py,max_cross=0;
    for(int i=0;i<node->child_count;i++){
        HTMLNode* c=node->children[i];
        if(!c||!c->computed_style.visible) continue;
        f32 fg=c->computed_style.flex_grow;
        f32 cw=css_resolve_value(c->computed_style.width, pw,vw);
        f32 ch=css_resolve_value(c->computed_style.height,ph,vh);
        if(fg>0){if(is_row)cw=avail*fg/total_flex;else ch=avail*fg/total_flex;}
        if(cw<=0)cw=is_row?100:pw; if(ch<=0)ch=is_row?ph:40;
        if(is_row){c->layout_x=cursor;c->layout_y=py;c->layout_w=cw;c->layout_h=ch;cursor+=cw+gap;if(ch>max_cross)max_cross=ch;}
        else{c->layout_x=px;c->layout_y=cursor;c->layout_w=cw;c->layout_h=ch;cursor+=ch+gap;if(cw>max_cross)max_cross=cw;}
        for(int j=0;j<c->child_count;j++) layout_block(e,c->children[j],c->layout_x,c->layout_y,c->layout_w,c->layout_h);
    }
    if(is_row){node->layout_w=w;node->layout_h=h>0?h:max_cross;}
    else{node->layout_h=cursor-py;node->layout_w=w;}
}

void layout_compute(BrowserEngine* e,f32 vw,f32 vh){
    if(!e||!e->document) return;
    e->layout.viewport_width=vw; e->layout.viewport_height=vh; e->layout.viewport_dirty=0;
    layout_block(e,e->document,0,0,vw,vh);
    e->layout.total_height=layout_get_total_height(e);
    e->layout_calls++;
}
f32 layout_get_total_height(BrowserEngine* e){
    if(!e||!e->document) return 0;
    return e->document->layout_h;
}

// ═══════════════════════════════════════════════════════════════
//  اختبار الإصابة والأحداث
// ═══════════════════════════════════════════════════════════════
static HTMLNode* hit_recursive(HTMLNode* node,f32 x,f32 y){
    if(!node||!node->computed_style.visible) return NULL;
    if(x<node->layout_x||x>node->layout_x+node->layout_w) return NULL;
    if(y<node->layout_y||y>node->layout_y+node->layout_h) return NULL;
    for(int i=node->child_count-1;i>=0;i--){
        HTMLNode* h=hit_recursive(node->children[i],x,y);
        if(h) return h;
    }
    return node;
}
HTMLNode* dom_hit_test(BrowserEngine* e,f32 x,f32 y){
    if(!e||!e->document) return NULL;
    return hit_recursive(e->document,x+e->layout.scroll_x,y+e->layout.scroll_y);
}
void dom_add_event(BrowserEngine* e,HTMLNode* node,EventType type,const char* handler){
    if(!e||!node||!handler||e->event_count>=DOM_MAX_EVENT_LISTENERS) return;
    EventListener* ev=&e->event_listeners[e->event_count++];
    ev->type=type;ev->target=node;ev->active=1;
    strncpy(ev->handler,handler,HTML_MAX_ATTR_LEN-1);
}
void dom_dispatch_event(BrowserEngine* e,EventType type,HTMLNode* target){
    if(!e||!target) return;
    for(int i=0;i<e->event_count;i++){
        EventListener* ev=&e->event_listeners[i];
        if(ev->active&&ev->type==type&&ev->target==target)
            js_execute(&e->js_engine,ev->handler);
    }
}
void dom_handle_click(BrowserEngine* e,f32 x,f32 y){
    HTMLNode* n=dom_hit_test(e,x,y);
    if(!n) return;
    n->is_active=1;
    if(n->onclick[0]) js_execute(&e->js_engine,n->onclick);
    dom_dispatch_event(e,EVENT_CLICK,n);
}

// ═══════════════════════════════════════════════════════════════
//  محرك جافاسكريبت
// ═══════════════════════════════════════════════════════════════
int js_set_var(JSEngine* js,const char* name,JSType type,const void* value){
    if(!js||!name) return -1;
    for(int i=0;i<js->var_count;i++){
        if(strcmp(js->variables[i].name,name)==0){
            js->variables[i].type=type;
            if(type==JS_TYPE_NUMBER) js->variables[i].number_val=*(f64*)value;
            else if(type==JS_TYPE_STRING) strncpy(js->variables[i].string_val,(char*)value,JS_MAX_STRING_LEN-1);
            else if(type==JS_TYPE_BOOLEAN) js->variables[i].bool_val=*(int*)value;
            return 0;
        }
    }
    if(js->var_count>=JS_MAX_VARS) return -1;
    JSVariable* v=&js->variables[js->var_count++];
    strncpy(v->name,name,127); v->type=type;
    if(type==JS_TYPE_NUMBER) v->number_val=*(f64*)value;
    else if(type==JS_TYPE_STRING) strncpy(v->string_val,(char*)value,JS_MAX_STRING_LEN-1);
    else if(type==JS_TYPE_BOOLEAN) v->bool_val=*(int*)value;
    return 0;
}
JSVariable* js_get_var(JSEngine* js,const char* name){
    if(!js||!name) return NULL;
    for(int i=0;i<js->var_count;i++)
        if(strcmp(js->variables[i].name,name)==0) return &js->variables[i];
    return NULL;
}
int js_execute(JSEngine* js,const char* code){
    if(!js||!code||!js->enabled) return 0;
    strncpy(js->code,code,JS_MAX_CODE_LEN-1);
    js->code_len=(int)strlen(js->code);
    const char* log=strstr(code,"console.log(");
    if(log){
        const char* s=log+12,*e=strchr(s,')');
        if(e){char msg[512]={0};strncpy(msg,s,MIN((size_t)(e-s),511UL));BROWSER_LOG("[JS] %s",msg);}
    }
    js->js_executions++; return 0;
}
void js_inject_dom_api(JSEngine* js,BrowserEngine* engine){
    (void)engine; f64 z=0;
    js_set_var(js,"window.innerWidth",JS_TYPE_NUMBER,&z);
    js_set_var(js,"window.innerHeight",JS_TYPE_NUMBER,&z);
    js_set_var(js,"document.readyState",JS_TYPE_STRING,"complete");
}

// ═══════════════════════════════════════════════════════════════
//  إدارة الصور
// ═══════════════════════════════════════════════════════════════
ImageResource* image_find(ImageCache* cache,const char* url){
    if(!cache||!url) return NULL;
    for(int i=0;i<cache->count;i++)
        if(strcmp(cache->images[i].url,url)==0) return &cache->images[i];
    return NULL;
}
ImageResource* image_load(ImageCache* cache,const char* url,void* data,size_t data_size){
    if(!cache||!url) return NULL;
    if(cache->count>=IMAGE_CACHE_MAX){
        if(cache->images[0].data)free(cache->images[0].data);
        memmove(&cache->images[0],&cache->images[1],sizeof(ImageResource)*(IMAGE_CACHE_MAX-1));
        cache->count--;
    }
    ImageResource* img=&cache->images[cache->count++];
    memset(img,0,sizeof(ImageResource));
    strncpy(img->url,url,HTML_MAX_SRC_LEN-1);
    if(data&&data_size>0){
        img->data=(u8*)malloc(data_size);
        if(img->data){memcpy(img->data,data,data_size);img->data_size=data_size;img->loaded=1;cache->total_size+=data_size;}
    }
    img->cached_at=time(NULL); return img;
}
void image_free(ImageCache* cache,const char* url){
    if(!cache||!url) return;
    for(int i=0;i<cache->count;i++){
        if(strcmp(cache->images[i].url,url)==0){
            if(cache->images[i].data){cache->total_size-=cache->images[i].data_size;free(cache->images[i].data);}
            memmove(&cache->images[i],&cache->images[i+1],sizeof(ImageResource)*(cache->count-i-1));
            cache->count--; return;
        }
    }
}
void image_cache_clear(ImageCache* cache){
    if(!cache) return;
    for(int i=0;i<cache->count;i++) if(cache->images[i].data)free(cache->images[i].data);
    memset(cache,0,sizeof(ImageCache));
}
