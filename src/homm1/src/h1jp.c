/*
 * Heroes of Might and Magic II Gold (GOG, DOS) Japanese patch - runtime part.
 * Loaded into DOSBox.exe (GOG 0.74-2.1, also Steam-style 0.74-3) as a winmm.dll proxy.
 *  - fopen("HEROES.EXE") is redirected to a copy in %LOCALAPPDATA%\H1JP where the data object is enlarged
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
static const uint8_t *g_caps; static uint32_t g_ncaps;               /* capitalization sites to drop */
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
    if (memcmp(h->magic, "H1JP", 4)) return 0;
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
        } else if (s->type == 'C') {
            g_ncaps = *(const uint32_t*)d; g_caps = d + 4;
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
#define O1_ENTRY      0x742f8u     /* VA 0x842f8  sti; and esp,-4 after the entry jump (patched in the file to our callback) */
#define O1_DRAW1      0x74bc7u     /* VA 0x84bc7  call 0x8dfb4 in the line drawer 0x84b50 (str [ebp-10h], index si) */
#define O1_DRAW2      0x74e02u     /* VA 0x84e02  call 0x8dfb4 in the wrap drawer 0x84bf0 (str [ebp-44h], index di) */
#define O1_SPRITE     0x7dfb4u     /* VA 0x8dfb4  mono sprite draw (icn eax, x edx, y ebx, index ecx; colour, 0, 0 on the stack; ret 0Ch) */
#define O1_WRAP1      0x74cd2u     /* VA 0x84cd2  cmp ecx,20h; je  (wrap drawer, char at [ebx]) */
#define O1_WRAP2      0x74f00u     /* VA 0x84f00  cmp ecx,20h; je  (line counter, char at [ebx]) */
#define O1_NEXT1      0x74e3du     /* VA 0x84e3d  mov ax,[eax+0Eh]; inc esi; add ecx,eax (wrap drawer: next line) */
#define O1_NEXT2      0x74f3fu     /* VA 0x84f3f  mov ebx,[esp]; mov ecx,[esp+4]; inc eax (line counter: next line) */
#define O1_REF        0x2623eu     /* VA 0x3623e  mov edx, offset "smalfont.fnt" (data offset 0x13d2) */
#define O2_REF_TARGET 0x13d2u
#define O2_SURFACE    0x43698u     /* VA 0x103698 pointer to the screen surface (+10h w, +12h h, +14h pixels) */

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
/* font object: +0Eh line height (12 small, 16 big), +10h baseline, +12h ICN; ICN +10h = 12-byte sprite headers
   (s16 x, s16 y, u16 w, u16 h, u32 data). Every byte >= 80h is drawn and measured as sprite 5Fh, whose width the
   AGG patch sets to half the advance of a Japanese char: the game's own width code then measures our 2-byte chars. */
#define IS_LEAD(c) ((c) >= 0x80 && (c) < 0xc0)
static int glyph_at(uint32_t a)
{
    uint8_t l = rb(a), t = rb(a + 1);
    int gn;
    if (!IS_LEAD(l) || t < 0xc0) return -1;
    gn = (l - 0x80) * 64 + (t - 0xc0);
    return gn < g_nglyph ? gn : -1;
}

static void draw_glyph(int big, int gn, int x, int base, uint8_t col)
{
    uint32_t surf = rd(L2(O2_SURFACE));
    int w = rw(DSP(surf) + 0x10), h = rw(DSP(surf) + 0x12);
    uint32_t buf = DSP(rd(DSP(surf) + 0x14));
    int size = big ? 12 : 8, y = base - (big ? g_big_y : g_small_y), gx, gy;
    for (gy = 0; gy < size; gy++)
        for (gx = 0; gx < size; gx++) {
            int on, px = x + gx, py = y + gy;
            if (big) on = (g_big[gn * 24 + gy * 2 + (gx >> 3)] >> (7 - (gx & 7))) & 1;
            else on = (g_small[gn * 8 + gy] >> (7 - gx)) & 1;
            if (!on || px < 0 || py < 0 || px >= w || py >= h) continue;
            wb(buf + py * w + px, col);
        }
}

/* CB 1/2: the sprite call of the two text drawers (call 8dfb4 -> callback; nop).
   index 5Fh = a byte >= 80h (or 7Fh): a lead byte draws the Japanese char, a trail byte draws nothing */
static uint32_t drawcall(uint32_t str, int idx, uint32_t font)
{
    uint32_t ret = R_EIP + 1;
    if ((R_ECX & 0xff) == 0x5f) {
        uint32_t a = DSP(str) + idx;
        uint8_t c = rb(a);
        if (c >= 0x80) {
            int gn = glyph_at(a);
            if (gn >= 0) draw_glyph(rw(DSP(font) + 0xe) >= 14, gn, (int)(int16_t)R_EDX, (int)(int16_t)R_EBX, (uint8_t)rd(STK(0)));
            R_ESP += 12;                       /* the callee would pop its 3 stack arguments */
            R_EIP = ret;
            return 0;
        }
    }
    R_ESP -= 4; wd(PHYS_SS + R_ESP, ret);      /* do the original call */
    R_EIP = L1(O1_SPRITE) - PHYS_CS;
    return 0;
}
static uint32_t __cdecl cb_draw1(void) { return drawcall(rd(PHYS_SS + R_EBP - 0x10), (int16_t)R_ESI, rd(PHYS_SS + R_EBP - 0x14)); }
static uint32_t __cdecl cb_draw2(void) { return drawcall(rd(PHYS_SS + R_EBP - 0x44), (int16_t)R_EDI, rd(PHYS_SS + R_EBP - 0x30)); }

/* a line may start at the Japanese char at p (glyph flags: 1 = no line start, 2 = no line end, 4 = katakana) */
static int jp_break_before(uint32_t a)
{
    int g = glyph_at(a), pg;
    uint8_t p1 = rb(a - 1);
    if (g < 0) return 0;
    if (g_gflags[g] & 1) return 0;
    if (p1 < 0xc0) return p1 == 0x20;
    pg = glyph_at(a - 2);
    if (pg < 0) return 1;
    if (g_gflags[pg] & 2) return 0;
    if ((g_gflags[g] & 4) && (g_gflags[pg] & 4)) return 0;
    return 1;
}
/* width of a byte >= 80h in this font (sprite 5Fh width + 1) */
static int byte_w(uint32_t font)
{
    uint32_t icn = rd(DSP(font) + 0x12), hd = rd(DSP(icn) + 0x10);
    return rw(DSP(hd) + 0x5f * 12 + 4) + 1;
}

/* CB 3/4: break test, replaces "cmp ecx,20h; je d8" by callback + d8; char at [ebx].
   The wrap drawer keeps the break char's width in dx (line width for centring): take the lead byte off */
static uint32_t wrapbrk(uint32_t font)
{
    int8_t d = (int8_t)rb(PHYS_CS + R_EIP);
    uint8_t c = (uint8_t)R_ECX;
    if (c == 0x20) R_EIP = R_EIP + 1 + d;
    else if (IS_LEAD(c) && jp_break_before(DSP(R_EBX))) {
        if (font) R_EDX = (R_EDX & 0xffff0000u) | (uint16_t)((uint16_t)R_EDX - byte_w(font));
        R_EIP = R_EIP + 1 + d;
    } else R_EIP += 1;
    return 0;
}
static uint32_t g_cnt[8];
#define CNT(i) do { if (g_logon > 1 && (++g_cnt[i] & 0xffff) == 0) dbg("cb%d x%u eip %x\n", i, g_cnt[i], R_EIP); } while (0)
static uint32_t __cdecl cb_wrap1(void) { CNT(3); return wrapbrk(rd(PHYS_SS + R_EBP - 0x30)); }
static uint32_t __cdecl cb_wrap2(void) { CNT(4); return wrapbrk(0); }

/* CB 5: wrap drawer, next line (mov ax,[eax+0Eh]; inc esi; add ecx,eax): the restored break char is in dl;
   the next line starts after a space, but at a Japanese char */
static uint32_t __cdecl cb_next1(void)
{
    CNT(5);
    /* line start = [ebp-44h] - [ebp-38h]; an empty line (a char wider than the box) must still move on */
    int start = (int)(rd(PHYS_SS + R_EBP - 0x44) - rd(PHYS_SS + R_EBP - 0x38));
    R_EAX = (R_EAX & 0xffff0000u) | rw(DSP(R_EAX) + 0xe);
    if (!IS_LEAD((uint8_t)R_EDX)) R_ESI += 1;
    else if ((int)(int16_t)R_ESI <= start) R_ESI += 2;
    R_ECX += R_EAX;
    R_EIP += 3;
    return 0;
}
/* CB 6: line counter, next line (mov ebx,[esp]; mov ecx,[esp+4]; inc eax); text in esi, index ax */
static uint32_t __cdecl cb_next2(void)
{
    CNT(6);
    uint8_t c = rb(DSP(R_ESI) + (int16_t)R_EAX);
    int start = (int)(int16_t)rd(STK(8));          /* line start index */
    R_EBX = rd(STK(0)); R_ECX = rd(STK(4));
    if (!IS_LEAD(c)) R_EAX += 1;
    else if ((int)(int16_t)R_EAX <= start) R_EAX += 2;
    R_EIP += 4;
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
    if (!GetEnvironmentVariableA("H1JP_NOENGINE", NULL, 0)) {   /* text engine */
        static const uint8_t c_orig[] = {0xe8};
        static const uint8_t w1_orig[] = {0x83, 0xf9, 0x20, 0x74};
        static const uint8_t n1_orig[] = {0x66, 0x8b, 0x40, 0x0e, 0x46, 0x01, 0xc1};
        static const uint8_t n2_orig[] = {0x8b, 0x1c, 0x24, 0x8b, 0x4c, 0x24, 0x04, 0x40};
        cbop(b, 1); b[4] = 0x90; patch_code(O1_DRAW1, c_orig, 1, b, 5);
        cbop(b, 2); b[4] = 0x90; patch_code(O1_DRAW2, c_orig, 1, b, 5);
        cbop(b, 3); patch_code(O1_WRAP1, w1_orig, 4, b, 4);
        cbop(b, 4); patch_code(O1_WRAP2, w1_orig, 4, b, 4);
        cbop(b, 5); b[4] = b[5] = b[6] = 0x90; patch_code(O1_NEXT1, n1_orig, 7, b, 7);
        cbop(b, 6); b[4] = b[5] = b[6] = b[7] = 0x90; patch_code(O1_NEXT2, n2_orig, 8, b, 8);
    }
    {   /* sentences built with sprintf get their first byte "capitalized" by -20h (no a-z check); see make_patch CAPS */
        const uint8_t *p = g_caps; uint32_t i;
        static const uint8_t nops[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        for (i = 0; p && i < g_ncaps; i++, p += 12) {
            uint32_t off = *(const uint32_t*)p; uint8_t n = p[4], cmp = p[5];
            patch_code(off, p + 6, cmp, nops, n);
        }
    }
    {   /* hero records: name with title at +2 (17 bytes), short name at +13h (9 bytes). Japanese short names do not
           fit (they overflowed into the hero's data), so the table's short names stay English (make_patch
           HERO_SHORT) and the two places that show the short name ("add esi,13h" before the copy, hero info
           panel) read the long field instead. */
        static const uint8_t o[3] = {0x83, 0xC6, 0x13}, n[3] = {0x83, 0xC6, 0x02};
        /* the panel copies the name into a 9-byte allocation and cuts it at 8 letters: 18 bytes, cut at 16 */
        static const uint8_t a_o[5] = {0xBA, 0x09, 0, 0, 0}, a_n[5] = {0xBA, 0x12, 0, 0, 0};       /* mov edx,9 (size) */
        static const uint8_t c_o[4] = {0xC6, 0x43, 0x08, 0x00}, c_n[4] = {0xC6, 0x43, 0x10, 0x00}; /* mov byte [ebx+8],0 */
        static const uint32_t base[2] = {0x3956e, 0x3a5ef};      /* VA 0x4956e, 0x4a5ef */
        int k;
        for (k = 0; k < 2; k++) {
            uint32_t s = base[k];
            /* all three or none */
            if (rb(L1(s)) == 0xBA && rb(L1(s) + 1) == 0x09 && rb(L1(s + 0x0a)) == 0x83 && rb(L1(s + 0x0c)) == 0x13 &&
                rb(L1(s + 0x3a)) == 0xC6 && rb(L1(s + 0x3c)) == 0x08) {
                patch_code(s, a_o, 5, a_n, 5);
                patch_code(s + 0x0a, o, 3, n, 3);
                patch_code(s + 0x3a, c_o, 4, c_n, 4);
            } else dbg("hero name panel: code mismatch at %x\n", s);
        }
    }
    {   /* the creature names (data 0x20ac4, 28 pointers) also make sprite file names ("%s.std", "%s.wlk"):
           keep an English copy of the table at the start of our room and point those code sites at it */
        static const uint32_t sites[] = {0x1b121, 0x1b285, 0x29c2d};   /* disp32 of mov r,[x*4+20ac4h] / mov r,[edx+20ac4h] */
        uint32_t copy = g_pool_off - 0x80, k;
        for (k = 0; k < 28; k++) wd(L2(copy) + k * 4, rd(L2(0x20ac4) + k * 4));
        for (k = 0; k < 3; k++)
            if (rd(L1(sites[k])) == g_o2ptr + 0x20ac4) wd(L1(sites[k]), g_o2ptr + copy);
            else dbg("name site %x mismatch\n", sites[k]);
    }
    if (!GetEnvironmentVariableA("H1JP_NOSTR", NULL, 0)) apply_strings();
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
    tab[b + 1] = (uint32_t)cb_draw1;
    tab[b + 2] = (uint32_t)cb_draw2;
    tab[b + 3] = (uint32_t)cb_wrap1;
    tab[b + 4] = (uint32_t)cb_wrap2;
    tab[b + 5] = (uint32_t)cb_next1;
    tab[b + 6] = (uint32_t)cb_next2;
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

/* 'M' section (a scenario or campaign map, .MAP/.CMP): u32 orig_size, u32 orig_crc, u32 n, n records
   {u32 kind (0), u32 file offset, u32 field size, u32 len, bytes (padded to 4)}: fixed text fields, NUL padded */
static uint8_t *build_map(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], nrec = ((const uint32_t*)sec)[2], k;
    const uint8_t *r = sec + 12, *e = sec + slen;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) return NULL;
    out = (uint8_t*)malloc(olen);
    memcpy(out, orig, olen);
    for (k = 0; k < nrec && r + 16 <= e; k++) {
        uint32_t kind = ((const uint32_t*)r)[0], a = ((const uint32_t*)r)[1], b = ((const uint32_t*)r)[2], n = ((const uint32_t*)r)[3];
        if (kind == 0 && a + b <= olen && n <= b) { memset(out + a, 0, b); memcpy(out + a, r + 16, n); }
        r += 16 + ((n + 3) & ~3u);
    }
    *outlen = olen;
    return out;
}

/* 'A' section (HEROES.AGG): u32 orig_size, u32 orig_crc, u32 n, n entries
   {u32 index, u32 nrep, nrep * {u32 len_off, u32 old_len, u32 new_len, bytes (padded to 4)}}:
   in the window layout entry 'index', the label at len_off (u16 length incl. NUL, text) is replaced.
   AGG (HoMM1): u16 count, count * {u16 hash, u32 offset, u32 size, u32 size}, data, count * 15 bytes of names */
static uint8_t *build_agg(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen, **ent;
    uint32_t cnt, i, k, total, pos, *grow;
    uint8_t *out;
    if (osz != olen || olen < 2 || crc32(orig, olen) != ocrc) { dbg("agg mismatch\n"); return NULL; }
    cnt = *(const uint16_t*)orig;
    if (2 + cnt * 14 > olen) return NULL;
    ent = (const uint8_t**)calloc(cnt, sizeof *ent);
    grow = (uint32_t*)calloc(cnt, 4);
    for (k = 0; k < n && p + 8 <= e; k++) {
        uint32_t idx = ((const uint32_t*)p)[0], nr = ((const uint32_t*)p)[1], r;
        const uint8_t *q = p + 8;
        if (idx < cnt) ent[idx] = p;
        for (r = 0; r < nr; r++) {
            uint32_t ol = ((const uint32_t*)q)[1], nl = ((const uint32_t*)q)[2];
            if (idx < cnt && ol != 0xffffffffu) grow[idx] += nl - ol;
            q += 12 + ((nl + 3) & ~3u);
        }
        p = q;
    }
    total = olen;
    for (i = 0; i < cnt; i++) total += grow[i];
    out = (uint8_t*)malloc(total);
    memcpy(out, orig, 2 + cnt * 14);
    pos = 2 + cnt * 14;
    for (i = 0; i < cnt; i++) {
        uint32_t off = *(const uint32_t*)(orig + 2 + i * 14 + 2), sz = *(const uint32_t*)(orig + 2 + i * 14 + 6);
        if (off + sz > olen) { free(out); free(ent); free(grow); return NULL; }
        if (pos < off && !ent[i]) { }   /* entries are contiguous in order; gaps are copied below */
        *(uint32_t*)(out + 2 + i * 14 + 2) = pos;
        *(uint32_t*)(out + 2 + i * 14 + 6) = sz + grow[i];
        *(uint32_t*)(out + 2 + i * 14 + 10) = sz + grow[i];
        if (!ent[i]) { memcpy(out + pos, orig + off, sz); pos += sz; }
        else {
            uint32_t nr = ((const uint32_t*)ent[i])[1], r, src = 0;
            const uint8_t *q = ent[i] + 8;
            for (r = 0; r < nr; r++) {
                uint32_t lo = ((const uint32_t*)q)[0], ol = ((const uint32_t*)q)[1], nl = ((const uint32_t*)q)[2];
                if (ol == 0xffffffffu) {         /* raw overwrite of nl bytes (same size) */
                    if (lo < src || lo + nl > sz) break;
                    memcpy(out + pos, orig + off + src, lo - src); pos += lo - src;
                    memcpy(out + pos, q + 12, nl); pos += nl;
                    src = lo + nl;
                    q += 12 + ((nl + 3) & ~3u);
                    continue;
                }
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
    sx = !_stricmp(bn, "HEROES.EXE") ? find_sect('X', bn, &slen) : NULL;
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

static DWORD WINAPI eip_thread(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(1000);
        if (g_o1) dbg("eip %x (obj1 %x) cs %x esp %x\n", R_EIP, PHYS_CS + R_EIP - g_o1, PHYS_CS, R_ESP);
    }
}

static void init(HMODULE self)
{
    char la[MAX_PATH];
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (GetEnvironmentVariableA("H1JP_OFF", la, MAX_PATH)) return;
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\H1JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    if (GetEnvironmentVariableA("H1JP_TRACE", NULL, 0)) CreateThread(NULL, 0, eip_thread, NULL, 0, NULL);
    dbg("HoMM1 JP patch active, blob %08X, DOSBox +%x\n", g_blob_crc, g_dbx);
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
