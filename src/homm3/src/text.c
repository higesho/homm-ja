/* translation tables: resource 1 = map/campaign strings (FNV-1a 64 of English -> EUC-JP),
   resource 3 = translated TXT files, written to %LOCALAPPDATA%\H3JP\txt and served on open */
#include "h3jp.h"
#include <string.h>
#include <stdio.h>
extern int g_test;

typedef struct { uint64_t h; uint32_t off, len; } TEnt;
static const TEnt *g_ent; static uint32_t g_n; static const char *g_blob;

static uint64_t fnv(const char *s, uint32_t n)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    while (n--) { h ^= (uint8_t)*s++; h *= 0x100000001b3ULL; }
    return h;
}

const char *tr_lookup(const char *s, uint32_t len, uint32_t *outlen)
{
    uint64_t h; int lo = 0, hi = (int)g_n - 1;
    if (!g_n) return 0;
    h = fnv(s, len);
    while (lo <= hi) {
        int m = (lo + hi) >> 1;
        if (g_ent[m].h == h) { *outlen = g_ent[m].len; return g_blob + g_ent[m].off; }
        if (g_ent[m].h < h) lo = m + 1; else hi = m - 1;
    }
    return 0;
}

void text_init(void)
{
    DWORD sz; const uint8_t *r = res_get(1, &sz);
    if (!r || memcmp(r, "H3TR", 4)) return;
    g_n = *(uint32_t *)(r + 4);
    g_ent = (const TEnt *)(r + 8);
    g_blob = (const char *)(g_ent + g_n);
    logf_("map strings: %u", g_n);
}

/* ---- TXT files ---- */
typedef struct { char name[24]; char path[MAX_PATH]; } TxtF;
static TxtF g_txt[128]; static int g_ntxt;

void txt_redirect_init(void)
{
    DWORD sz; const uint8_t *r = res_get(3, &sz); uint32_t n, i;
    char dir[MAX_PATH];
    if (!r || memcmp(r, "H3TX", 4)) return;
    if (!GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH)) GetTempPathA(MAX_PATH, dir);
    strcat(dir, "\\H3JP"); CreateDirectoryA(dir, 0);
    strcat(dir, "\\txt"); CreateDirectoryA(dir, 0);
    n = *(uint32_t *)(r + 4);
    for (i = 0; i < n && g_ntxt < 128; i++) {
        const uint8_t *e = r + 8 + i * 32;
        uint32_t off = *(uint32_t *)(e + 24), len = *(uint32_t *)(e + 28);
        TxtF *t = &g_txt[g_ntxt];
        HANDLE h; DWORD w;
        memcpy(t->name, e, 24); t->name[23] = 0;
        sprintf(t->path, "%s\\%s", dir, t->name);
        /* rewrite only when different */
        h = CreateFileA(t->path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD fs = GetFileSize(h, 0); int same = 0;
            if (fs == len) {
                char *b = HeapAlloc(GetProcessHeap(), 0, len + 1);
                if (b && ReadFile(h, b, len, &w, 0) && w == len && !memcmp(b, r + off, len)) same = 1;
                if (b) HeapFree(GetProcessHeap(), 0, b);
            }
            CloseHandle(h);
            if (same) { g_ntxt++; continue; }
        }
        h = CreateFileA(t->path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (h == INVALID_HANDLE_VALUE) { logf_("cannot write %s", t->path); continue; }
        WriteFile(h, r + off, len, &w, 0);
        CloseHandle(h);
        g_ntxt++;
    }
    logf_("txt files: %d", g_ntxt);
}

const char *txt_redirect(const char *path)
{
    const char *b = strrchr(path, '\\'), *b2 = strrchr(path, '/');
    int i;
    if (b2 > b) b = b2;
    b = b ? b + 1 : path;
    for (i = 0; i < g_ntxt; i++) if (!_stricmp(b, g_txt[i].name)) {
        /* only for files the game looks up in its data folder (not the editor's own files etc.) */
        return g_txt[i].path;
    }
    return 0;
}

/* ---- HotA: translated TXT handed straight to the text-table constructors (resource 5) ---- */
typedef struct { char name[24]; const char *data; uint32_t len; } MemTxt;
static MemTxt g_mt[160]; static int g_nmt;
typedef void *(__fastcall *TxtCtor)(void *self, void *edx, const char *name, int size, const char *buf);
static TxtCtor o_tbl, o_file;
static const MemTxt *mt_find(const char *name)
{
    int i;
    if (!name) return 0;
    for (i = 0; i < g_nmt; i++) if (!_stricmp(name, g_mt[i].name)) return &g_mt[i];
    return 0;
}
static void dump_txt(const char *name, int size, const char *buf)
{
    char dir[MAX_PATH], path[MAX_PATH]; FILE *f;
    if (g_test < 3 || !name || !buf || size <= 0 || strchr(name, 92)) return;
    if (!GetEnvironmentVariableA("H3JP_DUMP", dir, MAX_PATH)) return;
    sprintf(path, "%s/%s", dir, name);
    f = fopen(path, "wb"); if (f) { fwrite(buf, 1, size, f); fclose(f); }
}
static void *__fastcall h_tbl(void *self, void *edx, const char *name, int size, const char *buf)
{
    dump_txt(name, size, buf);
    const MemTxt *t = mt_find(name);
    font_late_hooks();
    if (g_test > 1) logf_("txt table %s %d%s", name, size, t ? " (ja)" : "");
    if (t) { buf = t->data; size = (int)t->len; }
    return o_tbl(self, 0, name, size, buf);
}
static void *__fastcall h_file(void *self, void *edx, const char *name, int size, const char *buf)
{
    dump_txt(name, size, buf);
    const MemTxt *t = mt_find(name);
    font_late_hooks();
    if (g_test > 1) logf_("txt file %s %d%s", name, size, t ? " (ja)" : "");
    if (t) { buf = t->data; size = (int)t->len; }
    return o_file(self, 0, name, size, buf);
}
void txt_hook_init(void)
{
    DWORD sz; const uint8_t *r; uint32_t n, i;
    if (!A->hota) return;
    r = res_get(5, &sz);
    if (!r || memcmp(r, "H3TX", 4)) { logf_("no HotA txt resource"); return; }
    n = *(uint32_t *)(r + 4);
    for (i = 0; i < n && g_nmt < 160; i++) {
        const uint8_t *e = r + 8 + i * 32;
        memcpy(g_mt[g_nmt].name, e, 24); g_mt[g_nmt].name[23] = 0;
        g_mt[g_nmt].data = (const char *)r + *(uint32_t *)(e + 24);
        g_mt[g_nmt].len = *(uint32_t *)(e + 28);
        g_nmt++;
    }
    o_tbl = make_tramp(A->txt_table, 5); hook_jmp(A->txt_table, h_tbl);
    o_file = make_tramp(A->txt_file, 5); hook_jmp(A->txt_file, h_file);
    logf_("HotA txt files: %d", g_nmt);
}

/* ---- HotA.dat: rebuilt with translated strings, handed to HotA.dll only ---- */
static int dat_ext(const uint8_t *s, uint32_t n)
{
    static const char *ext[] = { ".def", ".str", ".pcx", ".wav", ".smk", ".bik", ".msk", ".txt", ".h3m", ".fnt", 0 };
    int i;
    for (i = 0; ext[i]; i++) if (n >= 4 && !_strnicmp((const char *)s + n - 4, ext[i], 4)) return 1;
    return 0;
}
static int dat_key(const uint8_t *s, uint32_t n)   /* identifiers and comments (same rule as tools/hota_extract2.py) */
{
    uint32_t i; int lo = n <= 10, up = n <= 10;
    for (i = 0; i < n && n <= 10; i++) {
        uint8_t c = s[i];
        int dig = (c >= '0' && c <= '9') || c == '_';
        if (!(dig || (c >= 'a' && c <= 'z'))) lo = 0;
        if (!(dig || (c >= 'A' && c <= 'Z'))) up = 0;
    }
    if (lo || up) return 1;
    if (n && s[0] == '#') return 1;
    {   uint32_t k; int up = 0;
        for (k = 0; k < n; k++) { if (s[k] == 9) return 1; if (s[k] >= 'A' && s[k] <= 'Z') up = 1; }
        if (!up) return 1;      /* embedded tables / lowercase key lists stay English */
    }
    {   int sp = 0; uint32_t k;
        for (k = 0; k < n; k++) if (s[k] == ' ') sp = 1;
        if (!sp) for (k = 1; k < n; k++) if ((s[k] >= 'A' && s[k] <= 'Z') || (s[k] >= '0' && s[k] <= '9') || s[k] == '_') return 1;
    }
    return 0;
}
/* numbered text lists ('# 0' / '# Names' / '# Texts' / '... rumors'): translate each value line or quoted block
   (same rules as tools/hota_extract2.py dat_list / dat_parse) */
static int dat_list(const uint8_t *s, uint32_t n)
{
    uint32_t e = 0, k;
    while (e < n && s[e] != 13) e++;
    if (e >= 3 && s[0] == '#' && s[1] == ' ') {
        k = 2; while (k < e && s[k] >= '0' && s[k] <= '9') k++;
        if (k > 2 && (k == e || (k + 1 == e && s[k] == ' '))) return 1;
    }
    if ((e == 7 && !memcmp(s, "# Names", 7)) || (e == 7 && !memcmp(s, "# Texts", 7))) return 1;
    if (e >= 6 && !_strnicmp((const char *)s + e - 6, "rumors", 6)) return 1;
    return 0;
}
static uint32_t dat_put_list(const uint8_t *s, uint32_t n, uint8_t *o, uint32_t *nrep)
{
    uint32_t i = 0, op = 0;
    static uint8_t tmp[400000];
    while (i < n) {
        uint32_t e, tn = 0, jn; const char *j;
        if (s[i] == '"') {
            uint32_t q = i + 1;
            while (q < n) {
                if (s[q] == '"') { if (q + 1 < n && s[q + 1] == '"') { tmp[tn++] = '"'; q += 2; continue; } break; }
                tmp[tn++] = s[q++];
            }
            j = tr_lookup((const char *)tmp, tn, &jn);
            if (j) {
                uint32_t k; o[op++] = '"';
                for (k = 0; k < jn; k++) { if (j[k] == '"') o[op++] = '"'; o[op++] = (uint8_t)j[k]; }
                o[op++] = '"'; (*nrep)++;
            } else { memcpy(o + op, s + i, (q < n ? q + 1 : n) - i); op += (q < n ? q + 1 : n) - i; }
            i = q < n ? q + 1 : n;
            continue;   /* the rest of the line (normally just CRLF) is copied below as raw text */
        }
        e = i; while (e < n && !(s[e] == 13 && e + 1 < n && s[e + 1] == 10)) e++;
        {   int letter = 0; uint32_t k;
            for (k = i; k < e; k++) if ((s[k] | 32) >= 'a' && (s[k] | 32) <= 'z') letter = 1;
            j = 0;
            if (s[i] != '#' && letter) j = tr_lookup((const char *)s + i, e - i, &jn);
            if (j) { memcpy(o + op, j, jn); op += jn; (*nrep)++; }
            else { memcpy(o + op, s + i, e - i); op += e - i; }
        }
        if (e < n) { o[op++] = 13; o[op++] = 10; i = e + 2; } else i = n;
    }
    return op;
}
static char g_datpath[MAX_PATH];
static int g_datdone;
static void dat_build(const char *src)
{
    HANDLE h; DWORD sz, rd, w; uint8_t *d, *o; uint32_t i = 12, op = 0, nrep = 0;
    char dir[MAX_PATH];
    g_datdone = 1;
    h = CreateFileA(src, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    sz = GetFileSize(h, 0);
    d = HeapAlloc(GetProcessHeap(), 0, sz + 16); o = HeapAlloc(GetProcessHeap(), 0, sz * 4 + 64);
    if (!d || !o || !ReadFile(h, d, sz, &rd, 0) || rd != sz || sz < 12 || memcmp(d, "HDAT", 4)) { CloseHandle(h); return; }
    CloseHandle(h);
    memcpy(o, d, 12); op = 12;
    while (i < sz) {
        if (i + 4 <= sz) {
            uint32_t L = *(uint32_t *)(d + i);
            if (L >= 1 && L < 400000 && i + 4 + L <= sz) {
                const uint8_t *b = d + i + 4; uint32_t k; int ok = 0, letter = 0;
                for (k = 0; k < L; k++) {
                    uint8_t c = b[k];
                    if (!((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 0x80)) break;
                    if ((c | 32) >= 'a' && (c | 32) <= 'z') letter = 1;
                }
                ok = (k == L) && letter;
                if (ok) {
                    uint32_t jn; const char *j = 0;
                    if (b[0] == '#' && dat_list(b, L)) {
                        uint32_t n0 = nrep, ln = dat_put_list(b, L, o + op + 4, &nrep);
                        if (nrep != n0) { *(uint32_t *)(o + op) = ln; op += 4 + ln; i += 4 + L; continue; }
                    }
                    if (!dat_ext(b, L) && !dat_key(b, L) && L >= 3) j = tr_lookup((const char *)b, L, &jn);
                    if (j) { *(uint32_t *)(o + op) = jn; memcpy(o + op + 4, j, jn); op += 4 + jn; nrep++; }
                    else { memcpy(o + op, d + i, 4 + L); op += 4 + L; }
                    i += 4 + L; continue;
                }
            }
        }
        o[op++] = d[i++];
    }
    if (!GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH)) GetTempPathA(MAX_PATH, dir);
    strcat(dir, "\\H3JP"); CreateDirectoryA(dir, 0);
    sprintf(g_datpath, "%s\\HotA.dat", dir);
    h = CreateFileA(g_datpath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (h == INVALID_HANDLE_VALUE) { g_datpath[0] = 0; return; }
    WriteFile(h, o, op, &w, 0); CloseHandle(h);
    logf_("HotA.dat: %u strings translated", nrep);
}
const char *dat_redirect(const char *path, void *caller)
{
    const char *b = strrchr(path, 92), *b2 = strrchr(path, '/');
    HMODULE m = 0; char mn[MAX_PATH];
    if (b2 > b) b = b2;
    b = b ? b + 1 : path;
    if (_stricmp(b, "HotA.dat")) return 0;
    {   /* opened (directly or through the CRT) by HotA.dll? */
        void *fr[16]; USHORT nf = RtlCaptureStackBackTrace(0, 16, fr, 0), k; int hit = 0;
        HMODULE hota = GetModuleHandleA("HotA.dll"), hd = GetModuleHandleA("_hd3_.dll");
        (void)m; (void)mn;
        {   HMODULE cm = 0;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)caller, &cm) && cm && (cm == hota || cm == hd)) hit = 1;
        }
        for (k = 0; k < nf && !hit; k++) {
            HMODULE mm = 0;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)fr[k], &mm) && mm && (mm == hota || mm == hd)) { hit = 1; break; }
            if (g_test > 1) { char nm[MAX_PATH] = "?"; if (mm) GetModuleFileNameA(mm, nm, MAX_PATH); logf_("  frame %p %s", fr[k], nm); }
        }
        if (g_test > 1) logf_("HotA.dat open, from HotA.dll: %d", hit);
        if (!hit) return 0;
    }
    if (!g_datdone) dat_build(path);
    return g_datpath[0] ? g_datpath : 0;
}
