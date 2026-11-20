/*
 * Heroes of Might and Magic II Gold (GOG, DOS) Japanese patch - runtime part.
 * Loaded into DOSBox.exe (GOG 0.74-2.1, also Steam-style 0.74-3) as a winmm.dll proxy.
 *  - fopen("HEROES2.EXE") is redirected to a copy in %LOCALAPPDATA%\H2JP where the data object is enlarged
 *    and the entry jump calls back into this DLL.
 *  - at the entry callback the translated strings are written into the new room of the data object and every
 *    pointer to an English string (the LE fixup sites) is pointed at its translation; the text engine is patched:
 *      0x98ffe char width, 0x98ebd char draw (Japanese glyphs drawn here), 0x9915a/0x992e8 word-wrap breaks.
 *  - map files (.MP2/.MX2) and campaign files are redirected to translated copies.
 * Japanese text uses a private 2-byte code: lead 0x80-0xBF, trail 0xC0-0xFF (glyph = (lead-0x80)*64 + trail-0xC0);
 * 0x1E is a zero-width line-break opportunity.
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "resource.h"

void proxy_init(void);

/* ---------------------------------------------------------------- logging */
static char g_dir[MAX_PATH];
static FILE *g_log;
static int g_logon;
static void dbg(const char *fmt, ...)
{
    va_list ap;
    if (!g_logon) return;
    if (!g_log) {
        char p[MAX_PATH];
        wsprintfA(p, "%s\\log.txt", g_dir);
        g_log = fopen(p, "a");
        if (!g_log) return;
    }
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

/* ------------------------------------------------- DOSBox internals */
static uint32_t g_dbx, g_dbx_code;   /* Steam 0.74-3 = 0, GOG 0.74-2.1 = +0xe000 / -0x10, SVN = +0x3b000 / +0xbc0 */
#define A_CB_HANDLERS (0x74d460u + g_dbx)
#define A_CB_ILLEGAL (0x415e40u + g_dbx_code)
#define A_REGS (0x74d1e0u + g_dbx)
#define A_SEGVAL (0x74d100u + g_dbx)
#define A_SEGPHYS (0x74d110u + g_dbx)
#define A_TLB_READ (0x74d6d0u + g_dbx)
#define A_TLB_WRITE (0xb4d6d0u + g_dbx)
#define A_TLB_RH (0xf4d6d0u + g_dbx)
#define A_TLB_WH (0x134d6d0u + g_dbx)

typedef uint32_t (__cdecl *ph_rb)(void *self, uint32_t a);
typedef void (__cdecl *ph_wb)(void *self, uint32_t a, uint32_t v);

static int verify_dosbox(void)
{
    static const struct { uint32_t s1, s2, s3, dbx, dcode; } builds[] = {
        {0x415e66u, 0x41897fu, 0x4351ccu, 0u, 0u},
        {0x415e56u, 0x41896fu, 0x4351bcu, 0xe000u, 0xfffffff0u},
        {0x416a26u, 0x41965fu, 0x431da2u, 0x3b000u, 0xbc0u},
    };
    HMODULE h = GetModuleHandleA(NULL);
    int i;
    if ((uintptr_t)h != 0x400000) return 0;
    for (i = 0; i < 3; i++) {
        uint32_t cb = 0x74d460u + builds[i].dbx, ill = 0x415e40u + builds[i].dcode;
        uint32_t tlb = 0x74d6d0u + builds[i].dbx, regs = 0x74d1e0u + builds[i].dbx;
        uint8_t s1[11] = {0x81, 0x3c, 0x85}, s2[7] = {0x8b, 0x04, 0x95}, s3[6] = {0x66, 0xa3};
        memcpy(s1 + 3, &cb, 4); memcpy(s1 + 7, &ill, 4);
        memcpy(s2 + 3, &tlb, 4);
        memcpy(s3 + 2, &regs, 4);
        if (IsBadReadPtr((void*)(uintptr_t)builds[i].s1, 11) || IsBadReadPtr((void*)(uintptr_t)builds[i].s2, 7) ||
            IsBadReadPtr((void*)(uintptr_t)builds[i].s3, 6)) continue;
        if (memcmp((void*)(uintptr_t)builds[i].s1, s1, 11) || memcmp((void*)(uintptr_t)builds[i].s2, s2, 7) ||
            memcmp((void*)(uintptr_t)builds[i].s3, s3, 6)) continue;
        g_dbx = builds[i].dbx; g_dbx_code = builds[i].dcode;
        return 1;
    }
    return 0;
}

static __inline uint8_t rb(uint32_t a)
{
    uint8_t *p = ((uint8_t**)A_TLB_READ)[a >> 12];
    void *h;
    if (p) return p[a];
    h = ((void**)A_TLB_RH)[a >> 12];
    return (uint8_t)((ph_rb)((*(void***)h)[2]))(h, a);
}
static __inline void wb(uint32_t a, uint8_t v)
{
    uint8_t *p = ((uint8_t**)A_TLB_WRITE)[a >> 12];
    void *h;
    if (p) { p[a] = v; return; }
    h = ((void**)A_TLB_WH)[a >> 12];
    ((ph_wb)((*(void***)h)[5]))(h, a, v);
}
static __inline uint16_t rw(uint32_t a) { return (uint16_t)(rb(a) | (rb(a + 1) << 8)); }
static __inline uint32_t rd(uint32_t a) { return rb(a) | (rb(a + 1) << 8) | (rb(a + 2) << 16) | ((uint32_t)rb(a + 3) << 24); }
static __inline void wd(uint32_t a, uint32_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); wb(a + 2, (uint8_t)(v >> 16)); wb(a + 3, (uint8_t)(v >> 24)); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define R_EAX REG32(0)
#define R_ECX REG32(1)
#define R_EDX REG32(2)
#define R_EBX REG32(3)
#define R_ESP REG32(4)
#define R_EBP REG32(5)
#define R_ESI REG32(6)
#define R_EDI REG32(7)
#define R_EIP REG32(8)
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
#define STK(off) (PHYS_SS + R_ESP + (off))
#define DSP(p) (PHYS_DS + (uint32_t)(p))       /* linear address of a data pointer */

/* ------------------------------------------------------- patch blob */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)

static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
static const uint8_t *g_sect_s; static uint32_t g_sect_s_len;   /* strings */
static const uint8_t *g_small, *g_big; static int g_nglyph;     /* 8x8 (8 bytes) and 12x12 (24 bytes) glyphs */
static const uint8_t *g_gflags;
static int g_cb_base = 0x70;
static int g_small_adv = 9, g_big_adv = 13, g_big_y = 1, g_small_y = 0;
static uint32_t g_pool_off;                                      /* data-object offset of the string pool */
static int g_enabled;

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

static int load_blob(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_PATCH), (LPCSTR)RT_RCDATA);
    HGLOBAL g;
    const blob_hdr *h;
    const uint8_t *p;
    uint32_t i;
    if (!r) return 0;
    g = LoadResource(self, r);
    g_blob = (const uint8_t*)LockResource(g);
    g_blob_len = SizeofResource(self, r);
    if (!g_blob || g_blob_len < sizeof(blob_hdr)) return 0;
    h = (const blob_hdr*)g_blob;
    if (memcmp(h->magic, "H2JP", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {         /* u32 n; n*8 small; n*24 big; n flags */
            g_nglyph = (int)*(const uint32_t*)d;
            g_small = d + 4;
            g_big = g_small + g_nglyph * 8;
            g_gflags = g_big + g_nglyph * 24;
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0]; g_logon = (int)v[1];
            g_small_adv = (int)v[2]; g_big_adv = (int)v[3]; g_small_y = (int)v[4]; g_big_y = (int)v[5];
            g_pool_off = v[6];
        } else if (s->type == 'S') {
            g_sect_s = d; g_sect_s_len = s->len;
        }
        p = d + s->len;
    }
    return g_small && g_sect_s;
}

static const uint8_t *find_sect(uint32_t type, const char *name, uint32_t *len)
{
    const blob_hdr *h = (const blob_hdr*)g_blob;
    const uint8_t *p = g_blob + sizeof(blob_hdr);
    uint32_t i;
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == type && !_stricmp((const char*)d, name)) {
            *len = s->len - 16;
            return d + 16;
        }
        p = d + s->len;
    }
    return NULL;
}

/* ------------------------------------------------------------ game addresses (object offsets) */
#define O1_ENTRY      0x8784cu     /* sti; and esp,-4 after the entry jump (patched in the file to our callback) */
#define O1_WIDTH      0x88ffeu     /* VA 0x98ffe  char width (font eax, char dl) */
#define O1_DRAWCH     0x88ebdu     /* VA 0x98ebd  sub al,20h; movsx edx,al */
#define O1_DRAWSKIP   0x88fb6u     /* VA 0x98fb6  advance by the char width, next char */
#define O1_WRAP1      0x8915au     /* VA 0x9915a  cmp edx,20h; je */
#define O1_WRAP2      0x892e8u     /* VA 0x992e8  cmp edx,20h; je */
#define O1_WRAPNEXT1  0x89245u     /* VA 0x99245  xor ecx,ecx; mov [edi+eax],dl (wrap-draw: restore the break char) */
#define O1_WRAPNEXT2  0x8932du     /* VA 0x9932d  mov edx,[esp+4]; inc ebx (line counter: next line) */
#define O1_REF        0x31529u     /* VA 0x41529  push offset "smalfont.fnt" (data offset 0x26c6) */
#define O2_REF_TARGET 0x26c6u
#define O2_SCREEN     0x3ad70u     /* VA 0x11ad70 pointer to the screen manager (+0x46 = surface) */
#define O2_SHADE      0x297ccu     /* VA 0x1097cc shadow tables, 256 bytes each */
#define O2_REMAP1     0x29150u
#define O2_REMAP2     0x29250u
#define O2_REMAP3     0x29350u

static uint32_t g_o1, g_o2;        /* linear bases of the code and data objects */
static uint32_t g_o2ptr;           /* data object base as the program's pointers see it (DS relative) */
#define L1(o) (g_o1 + (o))
#define L2(o) (g_o2 + (o))

static int cbno(int i) { return g_cb_base + i; }
static int patch_code(uint32_t off, const uint8_t *orig, int n, const uint8_t *rep, int m)
{
    int k;
    for (k = 0; k < n; k++) if (rb(L1(off) + k) != orig[k]) { dbg("code mismatch at %x\n", off); return 0; }
    for (k = 0; k < m; k++) wb(L1(off) + k, rep[k]);
    return 1;
}
static void cbop(uint8_t *b, int i) { b[0] = 0xfe; b[1] = 0x38; b[2] = (uint8_t)cbno(i); b[3] = (uint8_t)(cbno(i) >> 8); }

/* ------------------------------------------------------------ fonts */
/* font object: +0x10 line height, +0x14 spacing, +0x18 colour switch ({ }), +0x1c ICN; ICN +0x12 = sprite headers
   (13 bytes: s16 x, s16 y, u16 w, u16 h, u8 type, u32 data offset from the headers) */
typedef struct { uint32_t icn; uint8_t row[16]; int rows, top; uint8_t shade; } fontcol;
static fontcol g_fc[8]; static int g_nfc;

static int is_big(uint32_t font) { return (int)rd(DSP(font) + 0x10) >= 14; }

/* the colour of each row of the font's 'A' (the ink ramp) and its shadow table */
static const fontcol *font_colours(uint32_t font)
{
    uint32_t icn = rd(DSP(font) + 0x1c), hdrs, h, p;
    int k, x = 0, y = 0, cnt[16][256], w, hh, oy, guard = 0;
    fontcol *f;
    for (k = 0; k < g_nfc; k++) if (g_fc[k].icn == icn) return &g_fc[k];
    if (g_nfc == 8) g_nfc = 7;
    f = &g_fc[g_nfc++];
    memset(f, 0, sizeof *f); f->icn = icn; f->shade = 0xff;
    hdrs = rd(DSP(icn) + 0x12);
    h = DSP(hdrs) + 0x21 * 13;
    oy = (int16_t)rw(h + 2); w = rw(h + 4); hh = rw(h + 6);
    p = DSP(hdrs) + rd(h + 9);
    memset(cnt, 0, sizeof cnt);
    while (guard++ < 4000) {
        uint8_t c = rb(p++);
        if (c == 0) { y++; x = 0; }
        else if (c < 0x80) { int n = c; while (n--) { uint8_t v = rb(p++); if (y < 16) cnt[y][v]++; x++; } }
        else if (c == 0x80) break;
        else if (c < 0xc0) x += c - 0x80;
        else if (c == 0xc0) { uint8_t b = rb(p++); int n = b & 3; if (!n) n = rb(p++); if (f->shade == 0xff) f->shade = (uint8_t)((b & 0x3c) >> 2); x += n; }
        else if (c == 0xc1) { int n = rb(p++); uint8_t v = rb(p++); if (y < 16) cnt[y][v] += n; x += n; }
        else { int n = c - 0xc0; uint8_t v = rb(p++); if (y < 16) cnt[y][v] += n; x += n; }
    }
    f->rows = hh < 16 ? hh : 16; f->top = oy;
    for (y = 0; y < f->rows; y++) {
        int best = -1, v;
        for (v = 0; v < 256; v++) if (cnt[y][v] && (best < 0 || cnt[y][v] > cnt[y][best])) best = v;
        f->row[y] = (uint8_t)(best < 0 ? (y ? f->row[y - 1] : 10) : best);
    }
    if (f->shade == 0xff) f->shade = 0;
    (void)w;
    dbg("font icn %x: rows %d top %d shade %d ramp %d..%d\n", icn, f->rows, f->top, f->shade, f->row[0], f->row[f->rows - 1]);
    return f;
}

static int jp_adv(uint32_t font) { return is_big(font) ? g_big_adv : g_small_adv; }

/* CB 1: char width, replaces the start of 0x98ffe (push ebx; push ecx; mov ebx,eax) by callback; ret */
static uint32_t __cdecl cb_width(void)
{
    uint32_t font = R_EAX, icn, hdrs;
    uint8_t c = (uint8_t)R_EDX;
    int idx;
    if (c == 0x7b || c == 0x7d || c == 0x1e || c >= 0xc0) { R_EAX = 0; return 0; }
    if (c >= 0x80) { R_EAX = (uint32_t)jp_adv(font); return 0; }
    if (c == 0x20) c = 0x69;
    if (c == 0x1f) c = 0x5f;
    idx = (uint8_t)(c - 0x20);
    if (idx > 0x5f) idx = 0x5f;
    icn = rd(DSP(font) + 0x1c);
    hdrs = rd(DSP(icn) + 0x12);
    R_EAX = (uint32_t)((int16_t)rw(DSP(hdrs) + idx * 13 + 4) + (int32_t)rd(DSP(font) + 0x14));
    return 0;
}

static void draw_glyph(uint32_t font, int gn, int x, int y, int mode, int cx, int cy, int cw, int ch)
{
    uint32_t scr = rd(L2(O2_SCREEN)), surf = rd(DSP(scr) + 0x46);
    int pitch = (int16_t)rw(DSP(surf) + 0x12);
    uint32_t buf = DSP(rd(DSP(surf) + 0x16)), remap = 0, shade;
    int big = is_big(font), size = big ? 12 : 8, gx, gy, pass;
    const fontcol *fc = font_colours(font);
    if (mode == 2 || (mode == 1 && rd(DSP(font) + 0x18))) remap = L2(O2_REMAP1);
    else if (mode == 4) remap = L2(O2_REMAP2);
    else if (mode != 1) remap = L2(O2_REMAP3);
    shade = L2(O2_SHADE) + fc->shade * 256;
    y += big ? g_big_y : g_small_y;
    for (pass = 0; pass < 2; pass++)
        for (gy = 0; gy < size; gy++)
            for (gx = 0; gx < size; gx++) {
                int on, px, py;
                if (big) on = (g_big[gn * 24 + gy * 2 + (gx >> 3)] >> (7 - (gx & 7))) & 1;
                else on = (g_small[gn * 8 + gy] >> (7 - gx)) & 1;
                if (!on) continue;
                px = pass ? x + gx : x + gx - 1; py = pass ? y + gy : y + gy + 1;
                if (px < cx || py < cy || px >= cx + cw || py >= cy + ch || px < 0 || py < 0) continue;
                if (pass == 0) {   /* shadow: left-below, darkens the background */
                    uint32_t a = buf + py * pitch + px;
                    wb(a, rb(shade + rb(a)));
                } else {
                    int r = fc->rows ? gy * fc->rows / size : 0;
                    uint8_t col = fc->rows ? fc->row[r] : 10;
                    if (remap) col = rb(remap + col);
                    wb(buf + py * pitch + px, col);
                }
            }
}

/* CB 2: char draw, replaces "sub al,20h; movsx edx,al" (5 bytes) in the line drawer 0x98e6c.
   esi = font, edi = x, ebp = y, [esp] = pointer to the char, [esp+14h] = mode, [esp+18h..24h] = clip x,y,w,h */
static uint32_t __cdecl cb_drawch(void)
{
    uint8_t c = (uint8_t)R_EAX;
    if (c == 0x1e || c >= 0xc0) { R_EIP = L1(O1_DRAWSKIP) - PHYS_CS; return 0; }
    if (c >= 0x80) {
        uint32_t p = rd(STK(0));
        uint8_t t = rb(DSP(p) + 1);
        if (g_logon > 1) dbg("drawch %x %x at %d,%d mode %d font %x\n", c, t, R_EDI, R_EBP, rd(STK(0x14)), R_ESI);
        if (t >= 0xc0) {
            int gn = (c - 0x80) * 64 + (t - 0xc0);
            if (gn < g_nglyph)
                draw_glyph(R_ESI, gn, (int)R_EDI, (int)R_EBP, (int)rd(STK(0x14)),
                           (int)rd(STK(0x18)), (int)rd(STK(0x1c)), (int)rd(STK(0x20)), (int)rd(STK(0x24)));
        }
        R_EIP = L1(O1_DRAWSKIP) - PHYS_CS;
        return 0;
    }
    c = (uint8_t)(c - 0x20);
    R_EAX = (R_EAX & 0xffffff00u) | c;
    R_EDX = (uint32_t)(int32_t)(int8_t)c;
    R_EIP += 1;   /* skip the filler byte */
    return 0;
}

/* a line may start at the Japanese char at p (glyph flags: 1 = no line start, 2 = no line end, 4 = katakana) */
#define IS_LEAD(c) ((c) >= 0x80 && (c) < 0xc0)
static int glyph_at(uint32_t a)
{
    uint8_t l = rb(a), t = rb(a + 1);
    int gn;
    if (!IS_LEAD(l) || t < 0xc0) return -1;
    gn = (l - 0x80) * 64 + (t - 0xc0);
    return gn < g_nglyph ? gn : -1;
}
static int jp_break_before(uint32_t a)
{
    int g = glyph_at(a), pg;
    uint8_t p1 = rb(a - 1);
    if (g < 0) return 0;
    if (g_gflags[g] & 1) return 0;
    if (p1 < 0xc0) return p1 == 0x20;              /* after ASCII only at a space (keeps "25個" together) */
    pg = glyph_at(a - 2);
    if (pg < 0) return 1;
    if (g_gflags[pg] & 2) return 0;
    if ((g_gflags[g] & 4) && (g_gflags[pg] & 4)) return 0;   /* inside a katakana word */
    return 1;
}

/* CB 3/4: word-wrap break test, replaces "cmp edx,20h; je d8" (83 fa 20 74 d8) by callback + d8;
   the char is at [ebx] (0x9915a) or [edi] (0x992e8) */
static uint32_t wrapbrk(uint32_t ptr, uint32_t font)
{
    int8_t d = (int8_t)rb(PHYS_CS + R_EIP);
    if (R_EDX == 0x20 || R_EDX == 0x1e) R_EIP = R_EIP + 1 + d;
    else if (IS_LEAD(R_EDX) && jp_break_before(DSP(ptr))) {
        /* the line ends before this char: take its width off the line width (ecx, used for centring) */
        if (font) R_ECX -= (uint32_t)jp_adv(font);
        R_EIP = R_EIP + 1 + d;
    } else R_EIP += 1;
    return 0;
}
static uint32_t __cdecl cb_wrapbrk_ebx(void) { return wrapbrk(R_EBX, R_EBP); }   /* wrap-draw: font in ebp */
static uint32_t __cdecl cb_wrapbrk_edi(void) { return wrapbrk(R_EDI, 0); }       /* line counter: width unused */

/* CB 5: after drawing a wrapped line (0x99245 "xor ecx,ecx; mov [edi+eax],dl"): the next line starts after the
   break char (inc edi follows) - unless the break was before a Japanese char, which stays */
static uint32_t __cdecl cb_wrapnext_draw(void)
{
    uint8_t dl = (uint8_t)R_EDX;
    R_ECX = 0;
    wb(DSP(R_EDI + R_EAX), dl);
    if (IS_LEAD(dl)) R_EDI -= 1;
    R_EIP += 1;
    return 0;
}
/* CB 6: same for the line counter (0x9932d "mov edx,[esp+4]; inc ebx"), char index ebx in [esp+8] */
static uint32_t __cdecl cb_wrapnext_count(void)
{
    uint8_t c = rb(DSP(rd(STK(8)) + R_EBX));
    R_EDX = rd(STK(4));
    if (!IS_LEAD(c)) R_EBX += 1;
    R_EIP += 1;
    return 0;
}

/* ------------------------------------------------------------ strings */
/* 'S': u32 nstr, u32 pool_len, pool[pool_len], then nstr records {u32 pool_off, u32 orig_off, u32 nsite, nsite * u32 site}
        (site: bit 31 = data object, else code object; low bits = object offset), then u32 ninplace, records {u32 off, u32 len, bytes} */
static int apply_strings(void)
{
    const uint8_t *p = g_sect_s, *e = g_sect_s + g_sect_s_len;
    uint32_t nstr = ((const uint32_t*)p)[0], plen = ((const uint32_t*)p)[1], i, k, done = 0, sites = 0;
    const uint8_t *pool = p + 8;
    for (i = 0; i < plen; i++) wb(L2(g_pool_off) + i, pool[i]);
    p = pool + ((plen + 3) & ~3u);
    for (i = 0; i < nstr && p + 12 <= e; i++) {
        uint32_t po = ((const uint32_t*)p)[0], oo = ((const uint32_t*)p)[1], ns = ((const uint32_t*)p)[2];
        p += 12;
        for (k = 0; k < ns; k++) {
            uint32_t s = ((const uint32_t*)p)[k];
            uint32_t a = (s & 0x80000000u) ? L2(s & 0x7fffffffu) : L1(s);
            if (rd(a) == g_o2ptr + oo) { wd(a, g_o2ptr + g_pool_off + po); sites++; }
            else dbg("site %x: %x != %x\n", s, rd(a), g_o2ptr + oo);
        }
        p += ns * 4;
        done++;
    }
    if (p + 4 <= e) {
        uint32_t n = *(const uint32_t*)p; p += 4;
        for (i = 0; i < n && p + 8 <= e; i++) {
            uint32_t off = ((const uint32_t*)p)[0], len = ((const uint32_t*)p)[1];
            p += 8;
            for (k = 0; k < len; k++) wb(L2(off) + k, p[k]);
            p += (len + 3) & ~3u;
        }
    }
    dbg("strings: %u relocated (%u sites)\n", done, sites);
    return 1;
}

/* CB 0: program start (replaces sti; and esp,-4) */
static uint32_t __cdecl cb_entry(void)
{
    uint32_t entry = PHYS_CS + R_EIP - 4;
    uint8_t b[8];
    g_o1 = entry - O1_ENTRY;
    g_o2ptr = rd(L1(O1_REF)) - O2_REF_TARGET;
    g_o2 = PHYS_DS + g_o2ptr;
    dbg("entry: code at %x, data at %x (ptr %x)\n", g_o1, g_o2, g_o2ptr);
    {   /* text engine */
        static const uint8_t w_orig[] = {0x53, 0x51, 0x89, 0xc3, 0x31};
        static const uint8_t d_orig[] = {0x2c, 0x20, 0x0f, 0xbe, 0xd0};
        static const uint8_t r_orig[] = {0x83, 0xfa, 0x20, 0x74};
        cbop(b, 1); b[4] = 0xc3; patch_code(O1_WIDTH, w_orig, 5, b, 5);
        cbop(b, 2); b[4] = 0x90; patch_code(O1_DRAWCH, d_orig, 5, b, 5);
        static const uint8_t n1_orig[] = {0x31, 0xc9, 0x88, 0x14, 0x07};
        static const uint8_t n2_orig[] = {0x8b, 0x54, 0x24, 0x04, 0x43};
        cbop(b, 3); patch_code(O1_WRAP1, r_orig, 4, b, 4);
        cbop(b, 4); patch_code(O1_WRAP2, r_orig, 4, b, 4);
        cbop(b, 5); b[4] = 0x90; patch_code(O1_WRAPNEXT1, n1_orig, 5, b, 5);
        cbop(b, 6); b[4] = 0x90; patch_code(O1_WRAPNEXT2, n2_orig, 5, b, 5);
    }
    {   /* sentences built with sprintf get their first byte "capitalized" by -20h (no a-z check): that breaks the
           lead byte of a Japanese first char. The Japanese texts need no capitals: drop these subtractions. */
        static const struct { uint32_t off; uint8_t n, cmp; uint8_t b[7]; } caps[] = {
            {0x15320, 3, 3, {0x80, 0xee, 0x20}}, {0x1b3f8, 3, 3, {0x80, 0xc7, 0xe0}}, {0x1b4aa, 3, 3, {0x80, 0xc3, 0xe0}},
            {0x55562, 7, 2, {0x80, 0x05}},       {0x6c3dc, 3, 3, {0x80, 0xc4, 0xe0}}, {0x71264, 3, 3, {0x80, 0xc2, 0xe0}},
            {0x7a481, 4, 4, {0x80, 0x04, 0x24, 0xe0}}, {0x7a50e, 4, 4, {0x80, 0x04, 0x24, 0xe0}},
            {0x7ec4d, 5, 5, {0x80, 0x44, 0x24, 0x04, 0xe0}}, {0x80d2a, 5, 5, {0x80, 0x44, 0x24, 0x04, 0xe0}},
            {0x24c6b, 7, 2, {0x80, 0x2d}}, {0x251b6, 7, 2, {0x80, 0x2d}}, {0x255a5, 7, 2, {0x80, 0x2d}},
            {0x54fb5, 7, 2, {0x80, 0x2d}}, {0x3be35, 5, 5, {0x80, 0x6c, 0x24, 0x54, 0x20}},
        };
        static const uint8_t nops[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        int i;
        for (i = 0; i < (int)(sizeof caps / sizeof caps[0]); i++)
            patch_code(caps[i].off, caps[i].b, caps[i].cmp, nops, caps[i].n);
    }
    apply_strings();
    REG32(9) |= 0x200;      /* sti */
    R_ESP &= ~3u;
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 7; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)cb_entry;
    tab[b + 1] = (uint32_t)cb_width;
    tab[b + 2] = (uint32_t)cb_drawch;
    tab[b + 3] = (uint32_t)cb_wrapbrk_ebx;
    tab[b + 4] = (uint32_t)cb_wrapbrk_edi;
    tab[b + 5] = (uint32_t)cb_wrapnext_draw;
    tab[b + 6] = (uint32_t)cb_wrapnext_count;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ----------------------------------------------------- file building */
typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;

static uint8_t *read_all(const char *path, uint32_t *len)
{
    FILE *f = fopen(path, "rb");      /* our own CRT (msvcrt FILE* must not reach our fread) */
    uint8_t *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t*)malloc(n ? n : 1);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (uint32_t)n;
    return b;
}
static int write_all(const char *path, const uint8_t *b, uint32_t n)
{
    char tmp[MAX_PATH];
    FILE *f;
    wsprintfA(tmp, "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    DeleteFileA(path);
    return MoveFileA(tmp, path);
}

/* 'X' section (the program): u32 orig_size, u32 orig_crc, then records {u32 off, u32 len, bytes};
   a 4-byte record whose bytes are FE 38 FF FF gets the entry callback number */
static uint8_t *build_exe(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1];
    const uint8_t *p = sec + 8, *e = sec + slen;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("exe mismatch\n"); return NULL; }
    out = (uint8_t*)malloc(olen);
    memcpy(out, orig, olen);
    while (p < e) {
        uint32_t off = ((const uint32_t*)p)[0], n = ((const uint32_t*)p)[1];
        p += 8;
        if (off + n > olen) { free(out); return NULL; }
        memcpy(out + off, p, n);
        if (n == 4 && p[0] == 0xfe && p[1] == 0x38 && p[2] == 0xff && p[3] == 0xff) {
            out[off + 2] = (uint8_t)cbno(0); out[off + 3] = (uint8_t)(cbno(0) >> 8);
        }
        p += (n + 3) & ~3u;
    }
    *outlen = olen;
    return out;
}

/* 'M' section (a scenario or campaign map, .MP2/.MX2/.H2C/.HXC): u32 orig_size, u32 orig_crc, u32 n, n records
   {u32 kind, u32 a, u32 b, u32 len, bytes (padded to 4)}:
     kind 0: header field, a = file offset, b = field size (bytes are NUL padded to b)
     kind 1: fixed field of info block a, b = offset | size << 16
     kind 2: message of info block a, from offset b to the end of the block (block is resized)
   map layout: u32 width, height at 420; 428 + w*h*20 tiles; u32 n + n*15 addons; 72*3 + 144*3 + 1;
   byte pairs until 0,0 (block count = last 256*h+l-1); blocks {u16 size, bytes}; tail */
#define MAXBLK 4096
static uint8_t *build_map(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], nrec = ((const uint32_t*)sec)[2];
    const uint8_t *recs = sec + 12, *e = sec + slen, *r;
    uint32_t w, h, p, na, cnt = 0, i, k, blk_at, total;
    static uint32_t boff[MAXBLK], blen[MAXBLK];
    static const uint8_t *rep[MAXBLK]; static uint32_t repoff[MAXBLK], replen[MAXBLK];
    uint8_t *out, *q;
    if (osz != olen || crc32(orig, olen) != ocrc || olen < 428) return NULL;
    w = *(const uint32_t*)(orig + 420); h = *(const uint32_t*)(orig + 424);
    if (w > 256 || h > 256) return NULL;
    p = 428 + w * h * 20;
    if (p + 4 > olen) return NULL;
    na = *(const uint32_t*)(orig + p); p += 4 + na * 15 + 72 * 3 + 144 * 3 + 1;
    for (;;) {
        if (p + 2 > olen) return NULL;
        if (!orig[p] && !orig[p + 1]) { p += 2; break; }
        cnt = 256u * orig[p + 1] + orig[p] - 1; p += 2;
    }
    blk_at = p;
    if (cnt > MAXBLK) return NULL;
    for (i = 0; i < cnt; i++) {
        if (p + 2 > olen) return NULL;
        blen[i] = *(const uint16_t*)(orig + p); boff[i] = p + 2; p += 2 + blen[i];
        if (p > olen) return NULL;
        rep[i] = NULL;
    }
    /* message replacements first (they decide the new size) */
    total = olen;
    for (r = recs, k = 0; k < nrec && r + 16 <= e; k++) {
        uint32_t kind = ((const uint32_t*)r)[0], a = ((const uint32_t*)r)[1], b = ((const uint32_t*)r)[2], n = ((const uint32_t*)r)[3];
        if (kind == 2 && a < cnt && b < blen[a] && !rep[a]) {
            rep[a] = r + 16; repoff[a] = b; replen[a] = n;
            total = total - (blen[a] - b) + n + 1;
        }
        r += 16 + ((n + 3) & ~3u);
    }
    out = (uint8_t*)malloc(total);
    memcpy(out, orig, blk_at);
    q = out + blk_at;
    for (i = 0; i < cnt; i++) {
        uint32_t nl = rep[i] ? repoff[i] + replen[i] + 1 : blen[i];
        if (nl > 0xffff) { free(out); return NULL; }
        *(uint16_t*)q = (uint16_t)nl; q += 2;
        if (rep[i]) { memcpy(q, orig + boff[i], repoff[i]); memcpy(q + repoff[i], rep[i], replen[i]); q[nl - 1] = 0; }
        else memcpy(q, orig + boff[i], blen[i]);
        boff[i] = (uint32_t)(q - out);    /* new position, for the fixed fields */
        q += nl;
    }
    memcpy(q, orig + p, olen - p); q += olen - p;
    for (r = recs, k = 0; k < nrec && r + 16 <= e; k++) {
        uint32_t kind = ((const uint32_t*)r)[0], a = ((const uint32_t*)r)[1], b = ((const uint32_t*)r)[2], n = ((const uint32_t*)r)[3];
        if (kind == 0 && a + b <= blk_at && n <= b) { memset(out + a, 0, b); memcpy(out + a, r + 16, n); }
        else if (kind == 1 && a < cnt) {
            uint32_t off = b & 0xffff, sz = b >> 16;
            if (off + sz <= (rep[a] ? repoff[a] : blen[a]) && n <= sz) { memset(out + boff[a] + off, 0, sz); memcpy(out + boff[a] + off, r + 16, n); }
        }
        r += 16 + ((n + 3) & ~3u);
    }
    *outlen = (uint32_t)(q - out);
    return out;
}

/* 'A' section (HEROES2.AGG / HEROES2X.AGG): u32 orig_size, u32 orig_crc, u32 n, n entries
   {u32 index, u32 nrep, nrep * {u32 len_off, u32 old_len, u32 new_len, bytes (padded to 4)}}:
   in the window layout entry 'index', the label at len_off (u16 length incl. NUL, text) is replaced.
   AGG: u16 count, count * {u32 hash, u32 offset, u32 size}, data, count * 15 bytes of names */
static uint8_t *build_agg(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen, **ent;
    uint32_t cnt, i, k, total, pos, *grow;
    uint8_t *out;
    if (osz != olen || olen < 2 || crc32(orig, olen) != ocrc) { dbg("agg mismatch\n"); return NULL; }
    cnt = *(const uint16_t*)orig;
    if (2 + cnt * 12 > olen) return NULL;
    ent = (const uint8_t**)calloc(cnt, sizeof *ent);
    grow = (uint32_t*)calloc(cnt, 4);
    for (k = 0; k < n && p + 8 <= e; k++) {
        uint32_t idx = ((const uint32_t*)p)[0], nr = ((const uint32_t*)p)[1], r;
        const uint8_t *q = p + 8;
        if (idx < cnt) ent[idx] = p;
        for (r = 0; r < nr; r++) {
            uint32_t ol = ((const uint32_t*)q)[1], nl = ((const uint32_t*)q)[2];
            if (idx < cnt) grow[idx] += nl - ol;
            q += 12 + ((nl + 3) & ~3u);
        }
        p = q;
    }
    total = olen;
    for (i = 0; i < cnt; i++) total += grow[i];
    out = (uint8_t*)malloc(total);
    memcpy(out, orig, 2 + cnt * 12);
    pos = 2 + cnt * 12;
    for (i = 0; i < cnt; i++) {
        uint32_t off = *(const uint32_t*)(orig + 2 + i * 12 + 4), sz = *(const uint32_t*)(orig + 2 + i * 12 + 8);
        if (off + sz > olen) { free(out); free(ent); free(grow); return NULL; }
        if (pos < off && !ent[i]) { }   /* entries are contiguous in order; gaps are copied below */
        *(uint32_t*)(out + 2 + i * 12 + 4) = pos;
        *(uint32_t*)(out + 2 + i * 12 + 8) = sz + grow[i];
        if (!ent[i]) { memcpy(out + pos, orig + off, sz); pos += sz; }
        else {
            uint32_t nr = ((const uint32_t*)ent[i])[1], r, src = 0;
            const uint8_t *q = ent[i] + 8;
            for (r = 0; r < nr; r++) {
                uint32_t lo = ((const uint32_t*)q)[0], ol = ((const uint32_t*)q)[1], nl = ((const uint32_t*)q)[2];
                if (lo < src || lo + 2 + ol > sz) break;
                memcpy(out + pos, orig + off + src, lo - src); pos += lo - src;
                *(uint16_t*)(out + pos) = (uint16_t)nl; pos += 2;
                memcpy(out + pos, q + 12, nl - 1); out[pos + nl - 1] = 0; pos += nl;
                src = lo + 2 + ol;
                q += 12 + ((nl + 3) & ~3u);
            }
            memcpy(out + pos, orig + off + src, sz - src); pos += sz - src;
        }
    }
    /* the name table at the end of the file */
    memcpy(out + pos, orig + olen - cnt * 15, cnt * 15); pos += cnt * 15;
    free(ent); free(grow);
    *outlen = pos;
    return out;
}

static const char *base_name(const char *p)
{
    const char *b = p, *q;
    for (q = p; *q; q++) if (*q == '\\' || *q == '/' || *q == ':') b = q + 1;
    return b;
}

static CRITICAL_SECTION g_cs;

static void clean_old(const char *bn)
{
    char pat[MAX_PATH], old[MAX_PATH], cur[16];
    WIN32_FIND_DATAA fd;
    HANDLE fh;
    wsprintfA(pat, "%s\\????????_%s", g_dir, bn);
    wsprintfA(cur, "%08X_", g_blob_crc);
    fh = FindFirstFileA(pat, &fd);
    if (fh == INVALID_HANDLE_VALUE) return;
    do {
        if (_strnicmp(fd.cFileName, cur, 9)) {
            wsprintfA(old, "%s\\%s", g_dir, fd.cFileName);
            DeleteFileA(old);
        }
    } while (FindNextFileA(fh, &fd));
    FindClose(fh);
}

static const char *patched_path(const char *orig_path, char *buf, int *is_exe)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *sx, *sm, *sa;
    uint8_t *orig, *out;
    *is_exe = 0;
    if (!g_enabled) return NULL;
    sx = !_stricmp(bn, "HEROES2.EXE") ? find_sect('X', bn, &slen) : NULL;
    sm = sx ? NULL : find_sect('M', bn, &slen);
    sa = (sx || sm) ? NULL : find_sect('A', bn, &slen);
    if (!sx && !sm && !sa) return NULL;
    *is_exe = sx != NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    clean_old(bn);
    orig = read_all(orig_path, &olen);
    if (!orig) return NULL;
    out = sx ? build_exe(orig, olen, sx, slen, &nlen) : sm ? build_map(orig, olen, sm, slen, &nlen) : build_agg(orig, olen, sa, slen, &nlen);
    free(orig);
    if (!out) return NULL;
    if (!write_all(buf, out, nlen)) { free(out); return NULL; }
    free(out);
    dbg("built %s\n", buf);
    return buf;
}

static FILE *__cdecl hook_fopen(const char *name, const char *mode)
{
    char buf[MAX_PATH];
    const char *pp;
    FILE *f;
    int ex;
    if (name && g_logon > 1) dbg("fopen %s %s\n", name, mode ? mode : "");
    if (!name || !g_enabled || (mode && strchr(mode, 'w'))) return real_fopen(name, mode);
    EnterCriticalSection(&g_cs);
    pp = patched_path(name, buf, &ex);
    if (pp && ex && !install_callbacks()) pp = NULL;
    LeaveCriticalSection(&g_cs);
    if (pp) {
        f = real_fopen(pp, (mode && strchr(mode, '+')) ? "rb" : mode);
        if (f) { dbg("redirect %s -> %s (%s)\n", name, pp, mode); return f; }
    }
    return real_fopen(name, mode);
}

static int hook_iat(void)
{
    uint8_t *base = (uint8_t*)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS *nh = (IMAGE_NT_HEADERS*)(base + dh->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *id = (IMAGE_IMPORT_DESCRIPTOR*)(base + dd->VirtualAddress);
    int done = 0;
    for (; id->Name; id++) {
        const char *dll = (const char*)(base + id->Name);
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp(dll, "msvcrt.dll")) continue;
        ot = (IMAGE_THUNK_DATA*)(base + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        ft = (IMAGE_THUNK_DATA*)(base + id->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME*)(base + ot->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, "fopen")) continue;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)(uintptr_t)hook_fopen;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            done = 1;
        }
    }
    return done;
}

static void init(HMODULE self)
{
    char la[MAX_PATH];
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (GetEnvironmentVariableA("H2JP_OFF", la, MAX_PATH)) return;
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\H2JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("HoMM2 JP patch active, blob %08X, DOSBox +%x\n", g_blob_crc, g_dbx);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init(h);
    }
    return TRUE;
}
