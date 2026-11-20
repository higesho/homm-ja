/* Japanese text in HoMM4's bitmap fonts: the t_font functions 0x71bd50-0x71c7e4 are replaced with
   versions that read EUC-JP pairs. A Japanese character becomes a glyph record in the game's own format
   (t_font_bitmap: one byte per pixel, high nibble = text alpha, low nibble = shadow alpha), rendered with
   GDI from the bundled Noto Serif JP subset (resource H4JP/2), and the game's glyph blitter 0x71b820 draws
   it, so colours and shadows work as for the English letters. Strings without Japanese go to the
   original word wrap code. */
#include "h4jp.h"
#include <stdlib.h>
#include <string.h>

typedef struct {               /* t_font_bitmap, 44 bytes */
    const void *vt; int w, h, pitch; uint8_t *bits; int bear, adv;
    uint8_t al, pad[3]; uint8_t *vf, *vl, *ve;
} Glyph;
typedef struct {               /* t_font */
    const void *vt; int asc, first, height, b10, count; Glyph *g;
} TFont;
typedef struct { void *alloc; char *ptr; uint32_t len, res; } mstr;
typedef struct { void *alloc; mstr *first, *last, *end; } mvec;
typedef struct { Glyph g; int dy; } JGlyph;

typedef void (__fastcall *gdraw_t)(Glyph *, void *, void *target, int x, int y, int c1, int c2, int c3);
typedef void *(__fastcall *erase_t)(mvec *, void *, mstr *first, mstr *last);
typedef void *(__fastcall *insert_t)(mvec *, void *, mstr *pos, const mstr *s);
typedef char (__fastcall *grow_t)(mstr *, void *, uint32_t, char);
typedef void (__cdecl *free_t)(void *);
typedef int (__fastcall *wrap_t)(TFont *, void *, mvec *, const char *, int);
typedef int (__fastcall *layout_t)(TFont *, void *, mvec *, const char *, int, int);
static const gdraw_t g_draw = (gdraw_t)0x71b820;
static const erase_t v_erase = (erase_t)0x5abc70;
static const insert_t v_insert = (insert_t)0x440f50;
static const grow_t s_grow = (grow_t)0x401df0;
static const free_t op_delete = (free_t)0x8dbd90;
static wrap_t o_wrap;
static layout_t o_layout;

/* ---------------------------------------------------------------- EUC-JP */
static int is_jp(const uint8_t *s) { return s[0] >= 0xa1 && s[0] <= 0xfe && s[1] >= 0xa1 && s[1] <= 0xfe; }
static int has_jp(const uint8_t *s)
{
    for (; *s; s++) if (*s >= 0xa1 && is_jp(s)) return 1;
    return 0;
}
static WCHAR g_uni[94 * 94];
static WCHAR euc_uni(int a, int b)
{
    int idx = (a - 0xa1) * 94 + (b - 0xa1);
    if (!g_uni[idx]) {
        int j1 = a - 0x80, j2 = b - 0x80;
        char sj[2]; WCHAR w = 0x30fb;
        sj[0] = (char)(((j1 + 1) >> 1) + (j1 <= 0x5e ? 0x70 : 0xb0));
        sj[1] = (char)(j2 + ((j1 & 1) ? (j2 >= 0x60 ? 0x20 : 0x1f) : 0x7e));
        if (MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, sj, 2, &w, 1) != 1) w = 0x30fb;
        g_uni[idx] = w;
    }
    return g_uni[idx];
}
static int in_list(const uint8_t *p, const char *utf8, WCHAR *codes, int *nc)
{
    WCHAR c; int i;
    if (*nc < 0) *nc = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, codes, 64) - 1;
    c = euc_uni(p[0], p[1]);
    for (i = 0; i < *nc; i++) if (codes[i] == c) return 1;
    return 0;
}
static int no_start(const uint8_t *p)    /* kinsoku: may not begin a line */
{
    static WCHAR codes[64]; static int nc = -1;
    if (!is_jp(p)) return *p == '.' || *p == ',' || *p == '!' || *p == '?' || *p == ')' || *p == ':' || *p == ';' || *p == '%';
    return in_list(p, "、。，．・：；？！ー）」』】〕］｝〉》ゃゅょっぁぃぅぇぉャュョッァィゥェォ…～〜", codes, &nc);
}
static int no_end(const uint8_t *p)      /* kinsoku: may not end a line */
{
    static WCHAR codes[64]; static int nc = -1;
    if (!is_jp(p)) return *p == '(' || *p == '"';
    return in_list(p, "「『（【〔［｛〈《", codes, &nc);
}

/* ---------------------------------------------------------------- glyphs */
#define GC_SIZE 32768
typedef struct { TFont *f; uint32_t c; JGlyph *g; } GEnt;
static GEnt *g_gc;
static HDC g_dc;
static HFONT g_hf[96];
static WCHAR g_face[32] = L"H4JP Serif";
static CRITICAL_SECTION g_cs;

/* Japanese em for a font of cell height h (the game's line height) */
static int jp_em(int h)
{
    if (h <= 13) return h;
    if (h <= 17) return h - 1;
    return h - 2;
}

static JGlyph *render(TFont *f, int a, int b)
{
    int em = jp_em(f->height), W, H, x, y, base, pitch, x0, y0;
    JGlyph *j;
    WCHAR wc = euc_uni(a, b);
    GLYPHMETRICS gm;
    MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    DWORD sz;
    uint8_t *buf = 0, *fg;
    if (em < 6) em = 6;
    if (em > 90) em = 90;
    W = em + 1; H = em + 1;
    j = (JGlyph *)calloc(1, sizeof(JGlyph) + W * H * 2);
    if (!j) return 0;
    fg = (uint8_t *)(j + 1) + W * H;
    j->g.vt = (const void *)0x988798;
    j->g.w = W; j->g.h = H; j->g.pitch = W; j->g.bits = (uint8_t *)(j + 1);
    j->g.vf = j->g.bits; j->g.vl = j->g.ve = j->g.bits + W * H;
    j->dy = (f->height - em) / 2;
    if (!g_hf[em])
        g_hf[em] = CreateFontW(-em, 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, OUT_TT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, g_face);
    SelectObject(g_dc, g_hf[em]);
    sz = GetGlyphOutlineW(g_dc, wc, GGO_GRAY4_BITMAP, &gm, 0, NULL, &mat);
    if (sz != GDI_ERROR && sz && (buf = (uint8_t *)malloc(sz)) != 0
        && GetGlyphOutlineW(g_dc, wc, GGO_GRAY4_BITMAP, &gm, sz, buf, &mat) != GDI_ERROR) {
        pitch = (gm.gmBlackBoxX + 3) & ~3;
        base = (em * 88 + 50) / 100;                        /* ideographic em box: 0.88 above the baseline */
        x0 = gm.gmptGlyphOrigin.x + (em - (int)gm.gmCellIncX) / 2;
        y0 = base - gm.gmptGlyphOrigin.y;
        for (y = 0; y < (int)gm.gmBlackBoxY; y++)
            for (x = 0; x < (int)gm.gmBlackBoxX; x++) {
                int v = buf[y * pitch + x], xx = x0 + x, yy = y0 + y;
                if (xx < 0 || yy < 0 || xx >= W - 1 || yy >= H - 1 || !v) continue;
                fg[yy * W + xx] = (uint8_t)(v >= 16 ? 15 : v);
            }
    }
    free(buf);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int s = (x && y) ? fg[(y - 1) * W + x - 1] : 0;
            j->g.bits[y * W + x] = (uint8_t)((fg[y * W + x] << 4) | s);
        }
    return j;
}

static JGlyph *jglyph(TFont *f, int a, int b)
{
    uint32_t c = (uint32_t)(a << 8 | b), h = (((uint32_t)f >> 4) * 2654435761u ^ c * 40503u) & (GC_SIZE - 1);
    JGlyph *g;
    EnterCriticalSection(&g_cs);
    for (;;) {
        GEnt *e = &g_gc[h];
        if (e->f == f && e->c == c) { g = e->g; break; }
        if (!e->f) {
            g = render(f, a, b);
            if (g) { e->f = f; e->c = c; e->g = g; }
            break;
        }
        h = (h + 1) & (GC_SIZE - 1);
    }
    LeaveCriticalSection(&g_cs);
    return g;
}

/* glyph of the character at s: *k = bytes, *dy = vertical offset */
static Glyph *cg(TFont *f, const uint8_t *s, int *k, int *dy)
{
    int i;
    *dy = 0;
    if (s[0] >= 0xa1 && is_jp(s)) {
        JGlyph *j = jglyph(f, s[0], s[1]);
        *k = 2;
        if (j) { *dy = j->dy; return &j->g; }
        i = 0;
    } else {
        *k = 1;
        i = s[0] - f->first;
        if (i < 0 || i >= f->count) i = 0;
    }
    return &f->g[i];
}

/* pixel width of n bytes of s (n < 0: whole string), as 0x71bde0 measures it */
static int str_w(TFont *f, const uint8_t *s, int n)
{
    int x = 0, first = 1, k, dy;
    const uint8_t *e = n < 0 ? 0 : s + n;
    while (*s && (!e || s < e)) {
        Glyph *g = cg(f, s, &k, &dy);
        if (!first) x += g->bear;
        x += g->w; first = 0; s += k;
        if (*s && (!e || s < e)) x += g->adv;
    }
    return x;
}

/* ---------------------------------------------------------------- replaced functions */
static int __fastcall h_width(TFont *f, void *e, const char *s)
{
    return str_w(f, (const uint8_t *)s, -1);
}

/* index (bytes) of the character boundary nearest to pixel x */
static int __fastcall h_fit(TFont *f, void *e, const char *str, int w)
{
    const uint8_t *s = (const uint8_t *)str;
    int acc = 0, first = 1, pos = 0, k, dy;
    while (s[pos]) {
        Glyph *g = cg(f, s + pos, &k, &dy);
        int cw = (first ? 0 : g->bear) + g->w + (s[pos + k] ? g->adv : 0);
        first = 0;
        acc += cw;
        if (acc >= w) return (acc - w < cw / 2) ? pos + k : pos;
        pos += k;
    }
    return pos;
}

static void __fastcall h_draw_str(TFont *f, void *e, void *target, int x, int y, const char *str, int c1, int c2, int c3)
{
    const uint8_t *s = (const uint8_t *)str;
    int first = 1, k, dy;
    while (*s) {
        Glyph *g = cg(f, s, &k, &dy);
        if (!first) x += g->bear;
        first = 0;
        g_draw(g, 0, target, x, y + dy, c1, c2, c3);
        x += g->adv + g->w;
        s += k;
    }
}

static void __fastcall h_draw_lines(TFont *f, void *e, void *target, const int *r, const mvec *v, int c1, int c2, int c3)
{
    const mstr *l;
    int y = r[1];
    if (r[2] - r[0] <= 0 || r[3] - r[1] <= 0 || !v->first) return;
    for (l = v->first; l != v->last; l++) {
        const uint8_t *s = (const uint8_t *)(l->ptr ? l->ptr : "");
        int x = r[0], first = 1, k, dy;
        if (y + f->height > r[3]) break;
        while (*s) {
            Glyph *g = cg(f, s, &k, &dy);
            if (!first) x += g->bear;
            first = 0;
            if (x + g->w > r[2]) break;
            g_draw(g, 0, target, x, y + dy, c1, c2, c3);
            x += g->adv + g->w;
            s += k;
        }
        y += f->height;
    }
}

/* ---------------------------------------------------------------- word wrap for Japanese */
static void push_line(mvec *v, const uint8_t *s, int n)
{
    mstr t = { 0, 0, 0, 0 };
    if (n > 0 && s_grow(&t, 0, (uint32_t)n, 1)) { memcpy(t.ptr, s, n); t.len = n; t.ptr[n] = 0; }
    v_insert(v, 0, v->last, &t);
    if (t.ptr) {
        uint8_t c = (uint8_t)t.ptr[-1];
        if (c == 0 || c == 0xff) op_delete(t.ptr - 1); else t.ptr[-1] = (char)(c - 1);
    }
}

/* bytes of the next line (*skip: bytes dropped after it, *wout: its width) */
static int next_line(TFont *f, const uint8_t *s, int maxw, int *skip, int *wout)
{
    int pos = 0, prev = -1, x = 0, padv = 0, brk = -1, brk_w = 0, brk_skip = 0;
    *skip = 0;
    while (s[pos]) {
        int k, dy, nx;
        Glyph *g;
        if (s[pos] == '\n') { *skip = 1; *wout = x; return pos; }
        g = cg(f, s + pos, &k, &dy);
        if (s[pos] == ' ') { brk = pos; brk_w = x; brk_skip = 1; }
        else if (prev >= 0 && (k == 2 || is_jp(s + prev)) && s[prev] != ' '
                 && !no_start(s + pos) && !no_end(s + prev)) { brk = pos; brk_w = x; brk_skip = 0; }
        nx = prev < 0 ? g->w : x + padv + g->bear + g->w;
        if (nx > maxw && prev >= 0 && s[pos] != ' ') {
            if (brk > 0) { *skip = brk_skip; *wout = brk_w; return brk; }
            *wout = x; return pos;
        }
        x = nx; padv = g->adv; prev = pos; pos += k;
    }
    *wout = x;
    return pos;
}

static int __fastcall h_wrap(TFont *f, void *e, mvec *out, const char *text, int width)
{
    const uint8_t *s = (const uint8_t *)text;
    int maxw = 0;
    if (!s || !has_jp(s)) return o_wrap(f, e, out, text, width);
    v_erase(out, 0, out->first, out->last);
    while (*s) {
        int skip, lw, len = next_line(f, s, width, &skip, &lw);
        if (len > 0 || skip) push_line(out, s, len);
        if (lw > maxw) maxw = lw;
        if (len == 0 && skip == 0) len = is_jp(s) ? 2 : 1;   /* never loop */
        s += len + skip;
    }
    return maxw;
}

/* 0x71c5e0: wrap at the narrowest width >= the longest word that keeps the box wider than 4x its height */
static int __fastcall h_layout(TFont *f, void *e, mvec *out, const char *text, int minw, int maxw)
{
    const uint8_t *s = (const uint8_t *)text;
    int longest = 0, w, r, mx;
    if (!s || !has_jp(s)) return o_layout(f, e, out, text, minw, maxw);
    while (*s) {
        const uint8_t *b;
        int ww;
        while (*s == ' ' || *s == '\n') s++;
        if (!*s) break;
        b = s;
        if (is_jp(s)) { s += 2; while (*s && no_start(s)) s += is_jp(s) ? 2 : 1; }
        else while (*s && *s != ' ' && *s != '\n' && !is_jp(s)) s++;
        ww = str_w(f, b, (int)(s - b));
        if (ww > longest) longest = ww;
    }
    mx = maxw < 0 ? 0 : maxw;
    w = longest < mx ? longest : mx;
    if (w < minw) w = minw;
    for (;;) {
        int n;
        r = h_wrap(f, 0, out, text, w);
        n = out->first ? (int)(out->last - out->first) : 0;
        if (n == 1 || r == 0) return r;
        if (w >= maxw) return r;
        if (f->height * n * 4 <= w) return r;
        w = w * 3 / 2;
        if (w > maxw) w = maxw;
    }
}

void font_init(void)
{
    DWORD sz = 0, n = 0;
    const void *p = res_get(2, &sz);
    if (p) AddFontMemResourceEx((void *)p, sz, NULL, &n);
    if (!n) lstrcpyW(g_face, L"MS Gothic");
    InitializeCriticalSection(&g_cs);
    g_gc = (GEnt *)calloc(GC_SIZE, sizeof(GEnt));
    g_dc = CreateCompatibleDC(NULL);
    o_wrap = (wrap_t)make_tramp(0x71c100, 7);
    o_layout = (layout_t)make_tramp(0x71c5e0, 5);
    hook_jmp(0x71bd50, h_draw_str);
    hook_jmp(0x71bde0, h_width);
    hook_jmp(0x71be40, h_fit);
    hook_jmp(0x71c100, h_wrap);
    hook_jmp(0x71c5e0, h_layout);
    hook_jmp(0x71c6e0, h_draw_lines);
    logf_("font: %u face(s)", n);
}
