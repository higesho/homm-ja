/* Translations (resource 1: FNV-1a 64 of the English string -> Japanese EUC-JP).
   - Tables and string files: the table reader 0x87d820 (thiscall vector<vector<string>>*, stream*) is hooked;
     the table is identified by its first two rows (resource 3) and only its display columns are replaced.
   - Maps, campaigns: the string reader 0x879420 (bool __cdecl read_string(stream *, std::string *)) replaces
     strings flagged as map text (bit 31 of the length).
   std::string is MSVC6 COW: +4 ptr, +8 length, +0xC capacity, refcount byte at ptr[-1]. */
#include "h4jp.h"
#include <string.h>
#include <stdlib.h>
#include <intrin.h>

typedef struct { uint64_t h; uint32_t off, len; } TEnt;
typedef struct { uint64_t sig; uint32_t first, pad; uint64_t mask; } TSpec;
static const TEnt *g_ent; static uint32_t g_n; static const char *g_blob;
static const TSpec *g_spec; static uint32_t g_nspec;

static uint64_t fnv_add(uint64_t h, const char *s, uint32_t n)
{
    while (n--) { h ^= (uint8_t)*s++; h *= 0x100000001b3ULL; }
    return h;
}
#define FNV0 0xcbf29ce484222325ULL

/* *flags: bit 31 = map text */
static const char *lookup(const char *s, uint32_t len, uint32_t *outlen, uint32_t *flags)
{
    uint64_t h; int lo = 0, hi = (int)g_n - 1;
    if (!g_n) return 0;
    h = fnv_add(FNV0, s, len);
    while (lo <= hi) {
        int m = (lo + hi) >> 1;
        if (g_ent[m].h == h) {
            *outlen = g_ent[m].len & 0x7fffffff; *flags = g_ent[m].len & 0x80000000u;
            return g_blob + g_ent[m].off;
        }
        if (g_ent[m].h < h) lo = m + 1; else hi = m - 1;
    }
    return 0;
}
const char *tr_lookup(const char *s, uint32_t len, uint32_t *outlen)
{
    uint32_t f;
    return lookup(s, len, outlen, &f);
}

void text_init(void)
{
    DWORD sz; const uint8_t *r = res_get(1, &sz);
    if (r && !memcmp(r, "H4TR", 4)) {
        g_n = *(uint32_t *)(r + 4);
        g_ent = (const TEnt *)(r + 8);
        g_blob = (const char *)(g_ent + g_n);
    }
    r = res_get(3, &sz);
    if (r && !memcmp(r, "H4TS", 4)) { g_nspec = *(uint32_t *)(r + 4); g_spec = (const TSpec *)(r + 8); }
    {   char b[8];
        if (g_test && GetEnvironmentVariableA("H4JP_NOTR", b, sizeof b)) g_n = 0;   /* test: English text */
    }
    logf_("strings: %u, tables: %u", g_n, g_nspec);
}

typedef struct { void *alloc; char *ptr; uint32_t len, res; } mstr;
typedef struct { void *alloc; mstr *first, *last, *end; } mvec;
typedef struct { void *alloc; mvec *first, *last, *end; } mtab;
typedef char (__cdecl *read_string_t)(void *, mstr *);
typedef char (__fastcall *grow_t)(mstr *, void *, uint32_t, char);   /* __thiscall bool _Grow(size_t, bool) */
static read_string_t o_read;
static grow_t s_grow = (grow_t)0x401df0;
static HANDLE g_dumpf;

static void dump(const mstr *s, uint32_t tag)
{
    DWORD w, z = s->len | tag;
    if (!g_dumpf) return;
    WriteFile(g_dumpf, &z, 4, &w, 0); WriteFile(g_dumpf, s->ptr, s->len, &w, 0);
}

static void replace(mstr *s, const char *t, uint32_t tl)
{
    if (s_grow(s, 0, tl, 1)) { memcpy(s->ptr, t, tl); s->len = tl; s->ptr[tl] = 0; }
}

/* callers of read_string that read resource data (layer names, object types, archive names, movie keys):
   their strings are identifiers even when the same word appears as text in a map ("Middle") */
static const uint32_t g_resread[] = {
    0x5959f9, 0x801774, 0x80178a, 0x8017c6, 0x465f31, 0x465f97, 0x465ffd, 0x4cee64, 0x42d212, 0x42d18c,
    0x410bc7, 0x816bb6, 0x816d16, 0x75cb8b, 0x41c12f, 0x8c507c, 0
};
static int resource_reader(uint32_t ra)
{
    int i;
    for (i = 0; g_resread[i]; i++) if (g_resread[i] == ra) return 1;
    return 0;
}

static char __cdecl h_read(void *st, mstr *s)
{
    char r = o_read(st, s);
    if (r && s->ptr && s->len) {
        uint32_t tl, fl; const char *t;
        dump(s, 0);
        t = lookup(s->ptr, s->len, &tl, &fl);
        if (t && fl && !resource_reader((uint32_t)_ReturnAddress())) {
            if (g_test) logf_("map %p: %.*s", _ReturnAddress(), s->len < 60 ? s->len : 60, s->ptr);
            replace(s, t, tl);
        }
    }
    return r;
}

/* rows are only dumped (H4JP_DUMP); translation happens per table */
typedef char (__cdecl *read_row_t)(void *, mvec *);
static read_row_t o_row;
static char __cdecl h_row(void *st, mvec *v)
{
    char r = o_row(st, v);
    mstr *s;
    if (r && v->first && g_dumpf)
        for (s = v->first; s < v->last; s++) if (s->ptr && s->len) dump(s, 0x80000000u);
    return r;
}

static uint64_t row_sig(uint64_t h, const mvec *r)
{
    const mstr *s;
    if (!r->first) return h;
    for (s = r->first; s < r->last; s++) {
        if (s != r->first) h = fnv_add(h, "\x1f", 1);
        if (s->ptr) h = fnv_add(h, s->ptr, s->len);
    }
    return h;
}

static void translate_table(mtab *t)
{
    uint64_t h;
    uint32_t i, nrows, done = 0;
    const TSpec *sp = 0;
    if (!t->first || t->last == t->first) return;
    nrows = (uint32_t)(t->last - t->first);
    h = row_sig(FNV0, &t->first[0]);
    h = fnv_add(h, "\x1e", 1);
    if (nrows > 1) h = row_sig(h, &t->first[1]);
    for (i = 0; i < g_nspec; i++) if (g_spec[i].sig == h) { sp = &g_spec[i]; break; }
    if (!sp) return;
    if (g_test) {   /* H4JP_TABMASK=hex: only the tables whose bit is set (test bisection) */
        char b[16];
        if (GetEnvironmentVariableA("H4JP_TABMASK", b, sizeof b) && !(strtoul(b, 0, 16) >> (sp - g_spec) & 1)) return;
    }
    for (i = sp->first; i < nrows; i++) {
        mvec *r = &t->first[i];
        uint32_t c, nc;
        if (!r->first) continue;
        nc = (uint32_t)(r->last - r->first);
        if (nc && r->first[0].ptr && r->first[0].len > 9
            && !strcmp(r->first[0].ptr + r->first[0].len - 9, ".hot_keys")) continue;
        for (c = 0; c < nc && c < 64; c++) {
            mstr *s = &r->first[c];
            uint32_t tl, fl; const char *tx;
            if (!(sp->mask >> c & 1) || !s->ptr || !s->len) continue;
            tx = lookup(s->ptr, s->len, &tl, &fl);
            if (tx) { replace(s, tx, tl); done++; }
        }
    }
    if (g_test) logf_("table %08x%08x: %u cells", (uint32_t)(h >> 32), (uint32_t)h, done);
}

typedef char (__fastcall *read_table_t)(mtab *, void *, void *);
static read_table_t o_table;
static char __fastcall h_table(mtab *t, void *e, void *st)
{
    char r = o_table(t, e, st);
    if (r) translate_table(t);
    return r;
}

void str_hook_init(void)
{
    char p[MAX_PATH];
    if (GetEnvironmentVariableA("H4JP_DUMP", p, sizeof p))
        g_dumpf = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    o_row = (read_row_t)make_tramp(0x879b50, 6);
    hook_jmp(0x879b50, h_row);
    o_read = (read_string_t)make_tramp(0x879420, 6);
    hook_jmp(0x879420, h_read);
    /* %keyword scanner 0x74fb30: isalnum((signed char)c) counted EUC-JP bytes as keyword letters, so
       "%month月" was never replaced -> movsx becomes movzx */
    if (!memcmp((void *)0x74fbaa, "\x0f\xbe\xcb", 3)) patch_bytes(0x74fbaa, "\x0f\xb6\xcb", 3);
    if (!memcmp((void *)0x74fc06, "\x0f\xbe\xd3", 3)) patch_bytes(0x74fc06, "\x0f\xb6\xd3", 3);
    o_table = (read_table_t)make_tramp(0x87d820, 6);
    hook_jmp(0x87d820, h_table);
    logf_("string hooks set");
}
