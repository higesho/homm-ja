/* Japanese (EUC-JP) aware replacements for the game's H3Font text routines.
   Single bytes are drawn with the game's own font data; JP characters use bundled
   1-bit glyph sets (main colour + diagonal shadow, like the original fonts). */
#include "h3jp.h"
#include <string.h>

typedef struct { int l, w, r; } CW;
#define FB(f)    ((uint8_t *)(f))
#define FH(f)    (*(uint8_t *)(FB(f) + 0x21))
#define FYO(f)   (*(int8_t *)(FB(f) + 0x22))
#define FCW(f)   ((CW *)(FB(f) + 0x3C))
#define FOFF(f)  ((uint32_t *)(FB(f) + 0xC3C))
#define FPAL(f)  ((uint16_t *)(FB(f) + 0x1058))
#define FBMP(f)  (*(uint8_t **)(FB(f) + 0x1258))
#define PCX_W(p) (*(int *)((uint8_t *)(p) + 0x24))
#define PCX_H(p) (*(int *)((uint8_t *)(p) + 0x28))
#define PCX_PITCH(p) (*(int *)((uint8_t *)(p) + 0x2C))
#define PCX_BUF(p)   (*(uint8_t **)((uint8_t *)(p) + 0x30))

typedef struct { int w, h, adv, n, bpr; const uint16_t *codes; const uint8_t *bits; } GSet;
static GSet g_sets[8]; static int g_nsets;

typedef struct { void *font; int set, y0, adv; } FInfo;
static FInfo g_fi[32]; static int g_nfi;

int is_jp(const uint8_t *p) { return p[0] >= 0xA1 && p[0] <= 0xFE && p[1] >= 0xA1 && p[1] <= 0xFE; }

static FInfo *finfo(void *f)
{
    int i, H, top = -1, set;
    for (i = 0; i < g_nfi; i++) if (g_fi[i].font == f) return &g_fi[i];
    H = FH(f);
    {   /* top row of 'A' */
        CW *c = &FCW(f)[(uint8_t)'A']; uint8_t *b = FBMP(f) + FOFF(f)['A']; int y, x;
        for (y = 0; y < H && top < 0; y++) for (x = 0; x < c->w; x++) if (b[y * c->w + x] == 0xFF) { top = y; break; }
        if (top < 0) top = H / 5;
    }
    set = H <= 13 ? 0 : H <= 18 ? 1 : H <= 22 ? 2 : H <= 27 ? 3 : 4;
    if (set >= g_nsets) set = g_nsets - 1;
    i = g_nfi < 32 ? g_nfi++ : 31;
    g_fi[i].font = f; g_fi[i].set = set;
    g_fi[i].y0 = top - 1;
    if (g_fi[i].y0 + g_sets[set].h + 1 > H) g_fi[i].y0 = H - g_sets[set].h - 1;
    if (g_fi[i].y0 < 0) g_fi[i].y0 = 0;
    g_fi[i].adv = g_sets[set].adv;
    if (A->hota) {   /* other mods draw byte by byte: make each EUC byte half a JP glyph wide */
        int c;
        for (c = 0xA1; c <= 0xFE; c++) { FCW(f)[c].l = 0; FCW(f)[c].w = g_fi[i].adv / 2; FCW(f)[c].r = 0; }
    }
    return &g_fi[i];
}

/* width of the character at p (JP = 2 bytes) */
static int cwidth(void *f, const uint8_t *p, int *n)
{
    if (is_jp(p)) { *n = 2; return finfo(f)->adv; }
    *n = 1;
    { CW *c = &FCW(f)[*p]; return c->l + c->w + c->r; }
}

static const uint8_t *glyph(GSet *s, int code)
{
    int lo = 0, hi = s->n - 1;
    while (lo <= hi) {
        int m = (lo + hi) >> 1;
        if (s->codes[m] == code) return s->bits + (size_t)m * s->bpr * s->h;
        if (s->codes[m] < code) lo = m + 1; else hi = m - 1;
    }
    return 0;
}

/* for subs.c: glyph bitmap of an EUC-JP code in a set (0 if missing) */
const uint8_t *jp_glyph(int set, int code, int *w, int *h, int *bpr, int *adv)
{
    GSet *s;
    if (set >= g_nsets) set = g_nsets - 1;
    if (set < 0) return 0;
    s = &g_sets[set]; *w = s->w; *h = s->h; *bpr = s->bpr; *adv = s->adv;
    return glyph(s, code);
}

static void put(void *pcx, int x, int y, uint16_t c)
{
    if (x < 0 || y < 0 || x >= PCX_W(pcx) || y >= PCX_H(pcx)) return;
    if (PCX_PITCH(pcx) >= PCX_W(pcx) * 4) {   /* 32-bit surface (HD Mod true colour) */
        uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
        *(uint32_t *)(PCX_BUF(pcx) + y * PCX_PITCH(pcx) + x * 4) = 0xFF000000u | (r * 255 / 31) << 16 | (g * 255 / 63) << 8 | (b * 255 / 31);
        return;
    }
    *(uint16_t *)(PCX_BUF(pcx) + y * PCX_PITCH(pcx) + x * 2) = c;
}

static void draw_jp(void *f, const uint8_t *p, void *pcx, int x, int y, int color)
{
    FInfo *fi = finfo(f); GSet *s = &g_sets[fi->set];
    const uint8_t *g = glyph(s, (p[0] << 8) | p[1]);
    uint16_t fg = FPAL(f)[color], sh = FPAL(f)[32];
    int gx, gy;
    y += fi->y0;
    if (!g) {   /* unknown glyph: hollow box */
        for (gx = 1; gx < s->w - 1; gx++) { put(pcx, x + gx, y + 1, fg); put(pcx, x + gx, y + s->h - 2, fg); }
        for (gy = 1; gy < s->h - 1; gy++) { put(pcx, x + 1, y + gy, fg); put(pcx, x + s->w - 2, y + gy, fg); }
        return;
    }
#define BIT(xx, yy) ((xx) >= 0 && (yy) >= 0 && (xx) < s->w && (yy) < s->h && (g[(yy) * s->bpr + ((xx) >> 3)] & (0x80 >> ((xx) & 7))))
    for (gy = 0; gy <= s->h; gy++)
        for (gx = 0; gx <= s->w; gx++) {
            if (BIT(gx, gy)) put(pcx, x + gx, y + gy, fg);
            else if (BIT(gx - 1, gy - 1)) put(pcx, x + gx, y + gy, sh);
        }
#undef BIT
}

/* HotA/HD Mod: their DrawLine draws byte by byte through DrawChar. We sit on top of their DrawChar,
   keep the lead byte and draw the whole JP glyph when the trail byte arrives. */
static void (__fastcall *o_drawchar)(void *f, void *edx, int ch, void *pcx, int x, int y, int color);
static struct { void *f, *pcx; int x, y; uint8_t lead; } g_pend;
static void draw_jp(void *f, const uint8_t *p, void *pcx, int x, int y, int color);
static const uint8_t *glyph(GSet *s, int code);
/* draw a JP glyph with the other mod's DrawChar: put it into font slot 0xA0 for this one call */
static void jp_via_slot(void *f, const uint8_t *p, void *pcx, int x, int y, int color)
{
    static uint8_t buf[40 * 40];
    FInfo *fi = finfo(f); GSet *s = &g_sets[fi->set];
    const uint8_t *g = glyph(s, (p[0] << 8) | p[1]);
    int H = FH(f), W = s->w + 1, gx, gy;
    if (H > 40 || W > 40) return;
    memset(buf, 0, W * H);
#define BIT(xx, yy) (g && (xx) >= 0 && (yy) >= 0 && (xx) < s->w && (yy) < s->h && (g[(yy) * s->bpr + ((xx) >> 3)] & (0x80 >> ((xx) & 7))))
    for (gy = 0; gy <= s->h && fi->y0 + gy < H; gy++)
        for (gx = 0; gx <= s->w; gx++) {
            if (BIT(gx, gy)) buf[(fi->y0 + gy) * W + gx] = 0xFF;
            else if (BIT(gx - 1, gy - 1)) buf[(fi->y0 + gy) * W + gx] = 0x01;
        }
#undef BIT
    if (!g) for (gx = 1; gx < s->w - 1; gx++) { buf[(fi->y0 + 1) * W + gx] = 0xFF; buf[(fi->y0 + s->h - 2) * W + gx] = 0xFF; }
    FCW(f)[0xA0].l = 0; FCW(f)[0xA0].w = W; FCW(f)[0xA0].r = 0;
    FOFF(f)[0xA0] = (uint32_t)(buf - FBMP(f));
    o_drawchar(f, 0, 0xA0, pcx, x, y, color);
}
static void __fastcall h_DrawChar(void *f, void *edx, int ch, void *pcx, int x, int y, int color)
{
    ch &= 255;
    finfo(f);
    if (ch >= 0xA1 && ch <= 0xFE) {
        if (g_pend.lead && g_pend.f == f && g_pend.pcx == pcx && g_pend.y == y && x > g_pend.x) {
            uint8_t b[2]; b[0] = g_pend.lead; b[1] = (uint8_t)ch;
            g_pend.lead = 0;
            jp_via_slot(f, b, pcx, g_pend.x, y, color);
            return;
        }
        g_pend.lead = (uint8_t)ch; g_pend.f = f; g_pend.pcx = pcx; g_pend.x = x; g_pend.y = y;
        return;
    }
    g_pend.lead = 0;
    o_drawchar(f, 0, ch, pcx, x, y, color);
}
void font_late_hooks(void)
{
    static int done;
    uint8_t *p = (uint8_t *)A->drawchar;
    if (done || !A->hota) return;
    done = 1;
    if (p[0] == 0xE9) o_drawchar = (void *)(A->drawchar + 5 + *(int32_t *)(p + 1));   /* on top of another mod's hook */
    else o_drawchar = make_tramp(A->drawchar, 6);
    hook_jmp(A->drawchar, h_DrawChar);
    logf_("DrawChar hooked on top of %p", o_drawchar);
}

/* original single-byte glyph draw: through the game's DrawChar (mods may hook it) */
static void draw_sb(void *f, int ch, void *pcx, int x, int y, int color)
{
    if (A->drawchar) {
        ((void (__fastcall *)(void *, void *, int, void *, int, int, int))A->drawchar)(f, 0, ch & 255, pcx, x, y, color);
        return;
    }
    {
    CW *c = &FCW(f)[ch & 255]; uint8_t *src = FBMP(f) + FOFF(f)[ch & 255];
    uint16_t fg = FPAL(f)[color], sh = FPAL(f)[32];
    int H = FH(f), xx, yy;
    x += c->l;
    for (yy = 0; yy < H; yy++)
        for (xx = 0; xx < c->w; xx++) {
            uint8_t v = *src++;
            if (v) put(pcx, x + xx, y + yy, v == 0xFF ? fg : sh);
        }
    }
}

/* 0x4b5260 DrawLine(text, len, pcx, x, y, color, clipX, clipY, clipW, clipH, cursor) */
static void __fastcall DrawLine(void *f, void *edx, const uint8_t *t, int len, void *pcx, int x, int y,
                                int color, int cx, int cy, int cw, int ch, int cursor)
{
    int hl = 0, idx = 0, i = 0, n, w;
    y += FYO(f);
    if (len > 0 && !is_jp(t) && FCW(f)[t[0]].l < 0) x -= FCW(f)[t[0]].l;
    if (y < cy || y + FH(f) > cy + ch) return;
    while (i < len) {   /* skip chars left of the clip */
        if (is_jp(t + i)) { if (x >= cx) break; w = finfo(f)->adv; n = 2; }
        else { if (FCW(f)[t[i]].l + x >= cx) break; w = cwidth(f, t + i, &n); }
        x += w; i += n;
    }
    if (color & 0x100) color &= ~0x100; else color += 9;
    if (len - i <= 0 && cursor != -1) { draw_sb(f, '_', pcx, x, y, color); return; }
    for (; i < len; i += n, idx++) {
        if (t[i] == '{') { hl = 1; n = 1; continue; }
        if (t[i] == '}') { hl = 0; n = 1; continue; }
        w = cwidth(f, t + i, &n);
        if (x + w > cx + cw) break;
        if (n == 2) draw_jp(f, t + i, pcx, x, y, color + hl); else draw_sb(f, t[i], pcx, x, y, color + hl);
        if (cursor == idx) draw_sb(f, '_', pcx, x, y, color);
        x += w;
    }
    if (cursor == idx) draw_sb(f, '_', pcx, x, y, color);
}

/* ---- line breaking ---- */
#include "kinsoku.h"
static uint8_t g_nostart[2][96 * 96 / 8 + 1], g_noend[96 * 96 / 8 + 1];
static int jbit(const uint8_t *tab, const uint8_t *p) { int k = (p[0] - 0xA0) * 96 + (p[1] - 0xA0); return (tab[k >> 3] >> (k & 7)) & 1; }
static void jset(uint8_t *tab, int code) { int k = ((code >> 8) - 0xA0) * 96 + ((code & 255) - 0xA0); tab[k >> 3] |= 1 << (k & 7); }
static int nostart(const uint8_t *p) { return is_jp(p) ? jbit(g_nostart[0], p) : (*p && strchr(",.!?:;)]%", *p) != 0); }
static int noend(const uint8_t *p) { return is_jp(p) ? jbit(g_noend, p) : (*p == '(' || *p == '['); }

/* Find the end of the line starting at t[s] that fits width W.
   Returns end index (exclusive, trailing spaces not included in *lw), *next = start of the next line. */
static int wrap_line(void *f, const uint8_t *t, int s, int W, int *next, int *lw)
{
    int i = s, x = 0, n, w, best = -1, bestw = 0, bestnext = 0, prev = -1;
    for (;;) {
        uint8_t c = t[i];
        if (!c || c == '\n') { *next = c ? i + 1 : i; *lw = x; return i; }
        if (c == '{' || c == '}') { i++; continue; }
        if (c == ' ') { best = i; bestw = x; bestnext = i + 1; }
        else if (prev >= 0 && (is_jp(t + prev) || is_jp(t + i)) && t[prev] != ' ' && !nostart(t + i) && !noend(t + prev)) {
            best = i; bestw = x; bestnext = i;
        }
        w = cwidth(f, t + i, &n);
        if (x + w > W && c != ' ') {
            if (best > s) { *next = bestnext; *lw = bestw; return best; }
            if (i == s) { i += n; x += w; }   /* at least one char */
            *next = i; *lw = x; return i;
        }
        x += w; prev = i; i += n;
    }
}

/* display-time fallback: a pure-ASCII string that is in the map/campaign table is replaced
   (campaign headers etc. that do not pass through ReadString). Same lookup in every routine,
   so measuring and drawing agree. */
static const uint8_t *xl(const uint8_t *t)
{
    const uint8_t *p; uint32_t n;
    const char *j;
    if (!t || !*t) return t;
    for (p = t; *p; p++) if (*p >= 0x80) return t;
    if (p - t < 2) return t;
    j = tr_lookup((const char *)t, (uint32_t)(p - t), &n);
    return j ? (const uint8_t *)j : t;
}

#ifdef H3JP_TEST
/* test mode: log drawn strings that still look English (once each) */
static void note_untr(const uint8_t *t)
{
    static uint32_t seen[4096]; static int n;
    const uint8_t *p; int run = 0, letters = 0; uint32_t h = 2166136261u; int i;
    for (p = t; *p; p++) {
        if (*p >= 0xA1) return;
        if ((*p | 32) >= 'a' && (*p | 32) <= 'z') { if (++run >= 3) letters = 1; } else run = 0;
        h = (h ^ *p) * 16777619u;
    }
    if (!letters) return;
    for (i = 0; i < n; i++) if (seen[i] == h) return;
    if (n < 4096) seen[n++] = h;
    logf_("UNTR|%s", t);
}
#endif
/* 0x4b5490 TextDraw(text, pcx, x, y, w, h, color, align, cursor) */
static void __fastcall TextDraw(void *f, void *edx, const uint8_t *t, void *pcx, int x, int y, int w, int h,
                                int color, int align, int cursor);
static int __fastcall LinesCount(void *f, void *edx, const uint8_t *t, int W)
{
    int s = 0, n = 0, nx, lw;
    if (!t) return 0;
    t = xl(t);
    while (t[s]) { wrap_line(f, t, s, W, &nx, &lw); n++; if (nx <= s) break; s = nx; }
    return n;
}
static void __fastcall TextDraw(void *f, void *edx, const uint8_t *t, void *pcx, int x, int y, int w, int h,
                                int color, int align, int cursor)
{
    int fh = FH(f), yoff = 0, s = 0, e, nx, lw, xo, len;
    if (!t) return;
    t = xl(t);
#ifdef H3JP_TEST
    if (g_test) note_untr(t);
#endif
    len = (int)strlen((const char *)t);
    if (!len) {
        if (cursor != -1) { if (color & 0x100) color &= ~0x100; else color += 9; draw_sb(f, '_', pcx, x, y, color); }
        return;
    }
    if (align & 4) {
        int tot = LinesCount(f, 0, t, w) * fh;
        align &= ~4;
        if (tot < h) yoff = (h - tot) / 2; else if (h < 2 * fh) yoff = (h - fh) / 2;
    }
    if (align & 8) {
        int tot = LinesCount(f, 0, t, w) * fh;
        align &= ~8;
        if (tot < h) yoff = h - tot;
    }
    while (s < len && t[s]) {
        if (fh + yoff > h && yoff != 0) break;
        e = wrap_line(f, t, s, w, &nx, &lw);
        xo = align == 1 ? (w - lw) / 2 : align == 2 ? w - lw : 0;
        ((void (__fastcall *)(void *, void *, const uint8_t *, int, void *, int, int, int, int, int, int, int, int))A->drawline)
            (f, 0, t + s, e - s, pcx, x + xo, y + yoff, color, x, y, w, h, cursor);
        yoff += fh;
        if (nx <= s) break;
        s = nx;
    }
}

/* 0x4b57d0 StrWidth(text): every byte counts */
static int __fastcall StrWidth(void *f, void *edx, const uint8_t *t)
{
    int x = 0, n;
    t = xl(t);
    while (*t) { x += cwidth(f, t, &n); t += n; }
    return x;
}
/* 0x4b5920 sum of widths, skipping \n and braces */
static int __fastcall TotalWidth(void *f, void *edx, const uint8_t *t)
{
    int x = 0, n;
    t = xl(t);
    while (*t) {
        if (*t == '\n' || *t == '{' || *t == '}') { t++; continue; }
        x += cwidth(f, t, &n); t += n;
    }
    return x;
}
/* 0x4b5990 widest '\n' line */
static int __fastcall MaxLineWidth(void *f, void *edx, const uint8_t *t)
{
    int x = 0, m = 0, n;
    t = xl(t);
    for (;; ) {
        if (!*t || *t == '\n') { if (x > m) m = x; x = 0; if (!*t) break; t++; continue; }
        if (*t == '{' || *t == '}') { t++; continue; }
        x += cwidth(f, t, &n); t += n;
    }
    return m;
}
/* 0x4b5a10 widest unbreakable unit */
static int __fastcall MaxWordWidth(void *f, void *edx, const uint8_t *t)
{
    int x = 0, m = 0, n, prev = -1, i = 0;
    t = xl(t);
    for (;;) {
        uint8_t c = t[i];
        if (!c || c == ' ' || c == '\n') { if (x > m) m = x; x = 0; prev = -1; if (!c) break; i++; continue; }
        if (c == '{' || c == '}') { i++; continue; }
        if (prev >= 0 && (is_jp(t + prev) || is_jp(t + i)) && !nostart(t + i) && !noend(t + prev)) { if (x > m) m = x; x = 0; }
        x += cwidth(f, t + i, &n); prev = i; i += n;
    }
    return m;
}
/* 0x4b5a80 widest line after wrapping to W */
static int __fastcall MaxLineWidthW(void *f, void *edx, const uint8_t *t, int W)
{
    int s = 0, m = 0, nx, lw;
    t = xl(t);
    while (t[s]) { wrap_line(f, t, s, W, &nx, &lw); if (lw > m) m = lw; if (nx <= s) break; s = nx; }
    return m;
}
/* 0x4b5b90 SplitTextIntoLines(text, W, vector<H3String>*) */
typedef void (__fastcall *StrTidy_t)(H3Str *, void *, int);
typedef void (__fastcall *StrAssign_t)(H3Str *, void *, const char *, int);
typedef void (__fastcall *VecInsert_t)(H3StrVec *, void *, H3Str *pos, int n, H3Str *val);
static void __fastcall SplitLines(void *f, void *edx, const uint8_t *t, int W, H3StrVec *v)
{
    StrTidy_t tidy = (StrTidy_t)A->str_tidy; StrAssign_t assign = (StrAssign_t)A->str_assign;
    VecInsert_t ins = (VecInsert_t)A->vec_insert;
    H3Str tmp; int s = 0, e, nx, lw;
    void (__fastcall *clear)(H3StrVec *, void *, H3Str *, H3Str *) = (void *)A->vec_clear;
    memset(&tmp, 0, sizeof tmp);
    t = xl(t);
    clear(v, 0, v->beg, v->end);
    while (t[s]) {
        e = wrap_line(f, t, s, W, &nx, &lw);
        assign(&tmp, 0, (const char *)t + s, e - s);
        ins(v, 0, v->end, 1, &tmp);
        if (nx <= s) break;
        s = nx;
    }
    tidy(&tmp, 0, 1);
}

static void load_sets(void)
{
    DWORD sz; const uint8_t *r = res_get(2, &sz); uint32_t i, ns;
    if (!r || memcmp(r, "H3GL", 4)) { logf_("no glyph resource"); return; }
    ns = *(uint32_t *)(r + 4);
    for (i = 0; i < ns && i < 8; i++) {
        const uint8_t *h = r + 8 + i * 16;
        GSet *s = &g_sets[i];
        s->w = h[0]; s->h = h[1]; s->adv = h[2]; s->bpr = (s->w + 7) / 8;
        s->n = *(uint32_t *)(h + 4);
        s->codes = (const uint16_t *)(r + *(uint32_t *)(h + 8));
        s->bits = r + *(uint32_t *)(h + 12);
    }
    g_nsets = ns;
    /* kinsoku tables (EUC-JP codes) */
    {
        const uint16_t *p;
        for (p = k_nostart; *p; p++) jset(g_nostart[0], *p);
        for (p = k_noend; *p; p++) jset(g_noend, *p);
    }
}

void font_init(void)
{
    load_sets();
    if (!g_nsets) return;
    hook_jmp(A->drawline, DrawLine);
    hook_jmp(A->textdraw, TextDraw);
    hook_jmp(A->strwidth, StrWidth);
    hook_jmp(A->linescount, LinesCount);
    hook_jmp(A->totalwidth, TotalWidth);
    hook_jmp(A->maxlinewidth, MaxLineWidth);
    hook_jmp(A->maxwordwidth, MaxWordWidth);
    hook_jmp(A->maxlinewidthw, MaxLineWidthW);
    hook_jmp(A->splitlines, SplitLines);
}
