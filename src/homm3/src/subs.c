/* Japanese subtitles for the narrated movies (opening H3Intro, Restoration ending Endgame).
   Both are played by the game's Smacker path (video index >= 0x1c), decoded by SmackDoFrame straight
   into the 16-bit back buffer; the game then blits only the union of SmackToBufferRect rectangles.
   - SmackToBuffer: remember where the movie goes (buffer, left/top, pitch, flags).
   - SmackDoFrame: put back the pixels under the previous subtitle, decode, then save the band under
     the new subtitle and draw the cue for the current frame time over the bottom of the picture.
   - SmackToBufferRect: the first rectangle after each frame reports the whole picture, so the
     subtitle band (and its removal) always reaches the screen. */
#include "h3jp.h"
#include <string.h>

#define SUB_SET 3          /* BIZ UDGothic Bold 18px */
#define LINE_GAP 5
#define MARGIN_B 12

typedef struct { uint32_t s, e, off; } Cue;          /* ms; off -> EUC-JP text, '\n' between lines */
typedef struct { uint32_t w, h, frames, n; const Cue *c; } Vid;
static Vid g_vid[8]; static int g_nvid;
static const uint8_t *g_res;

#define SM(b, o) (*(int *)((uint8_t *)(b) + (o)))
/* Smack: +4 Width, +8 Height, +0xC Frames, +0x10 MSPerFrame (<0: 1/100 ms units), +0x374 FrameNum (0-based),
   +0x380 LastRectx, +0x384 LastRecty, +0x388 LastRectw, +0x38C LastRecth */

static const Vid *find(void *s)
{
    int i;
    if (!s) return 0;
    for (i = 0; i < g_nvid; i++)
        if ((uint32_t)SM(s, 4) == g_vid[i].w && (uint32_t)SM(s, 8) == g_vid[i].h && (uint32_t)SM(s, 0xC) == g_vid[i].frames) return &g_vid[i];
    return 0;
}

typedef struct { uint8_t *buf; int pitch, w, h; } Dst;   /* 16-bit picture */
static void px(const Dst *d, int x, int y, int white)
{
    if (x < 0 || y < 0 || x >= d->w || y >= d->h) return;
    *(uint16_t *)(d->buf + y * d->pitch + x * 2) = white ? 0xFFFF : 0x0000;
}

static int line_chars(const uint8_t *t, int *nb)
{
    int n = 0, i = 0;
    while (t[i] && t[i] != '\n') { i += is_jp(t + i) ? 2 : 1; n++; }
    *nb = i; return n;
}

static void draw_line(const Dst *d, const uint8_t *t, int nb, int x, int y, int pass)
{
    int i = 0, w, h, bpr, adv;
    while (i < nb) {
        if (is_jp(t + i)) {
            const uint8_t *g = jp_glyph(SUB_SET, (t[i] << 8) | t[i + 1], &w, &h, &bpr, &adv);
            if (g) {
                int gx, gy, ox, oy;
                for (gy = 0; gy < h; gy++)
                    for (gx = 0; gx < w; gx++) {
                        if (!(g[gy * bpr + (gx >> 3)] & (0x80 >> (gx & 7)))) continue;
                        if (pass == 0) {   /* black outline + drop shadow */
                            for (oy = -1; oy <= 1; oy++) for (ox = -1; ox <= 1; ox++) px(d, x + gx + ox, y + gy + oy, 0);
                            px(d, x + gx + 2, y + gy + 2, 0);
                        } else px(d, x + gx, y + gy, 1);
                    }
            }
            x += adv; i += 2;
        } else { x += 9; i++; }
    }
}

/* top row used by the text (for the backup band) */
static int text_top(const Dst *d, const uint8_t *t, int *lh, int *adv)
{
    int w, h, bpr, nl = 1;
    jp_glyph(SUB_SET, 0xA1A1, &w, &h, &bpr, adv);
    for (; *t; t++) if (*t == '\n') nl++;
    *lh = h + LINE_GAP;
    return d->h - MARGIN_B - nl * *lh + LINE_GAP;
}

static void draw_text(const Dst *d, const uint8_t *t)
{
    int lh, adv, y0 = text_top(d, t, &lh, &adv), pass;
    for (pass = 0; pass < 2; pass++) {
        const uint8_t *p = t; int y = y0;
        for (;;) {
            int nb, n = line_chars(p, &nb);
            draw_line(d, p, nb, (d->w - n * adv) / 2, y, pass);
            p += nb; y += lh;
            if (*p != '\n') break;
            p++;
        }
    }
}

/* where the current Smacker movie is drawn */
static struct { void *s; uint8_t *buf; int left, top, pitch; unsigned flags; } g_to;
/* pixels under the subtitle (rows y0..h-1 of the picture) */
static uint16_t g_save[640 * 120]; static void *g_save_s; static int g_save_y0, g_save_rows, g_save_w;
static void *g_full;   /* report the whole picture on the next SmackToBufferRect */

static int dst_of(void *s, Dst *d)
{
    if (s != g_to.s || !g_to.buf || !(g_to.flags & 0x80000000u)) return 0;   /* 16-bit buffers only */
    d->buf = g_to.buf + g_to.top * g_to.pitch + g_to.left * 2;
    d->pitch = g_to.pitch; d->w = SM(s, 4); d->h = SM(s, 8);
    return 1;
}

static void (__stdcall *o_tobuf)(void *, unsigned, unsigned, unsigned, unsigned, void *, unsigned);
static void __stdcall h_tobuf(void *s, unsigned l, unsigned t, unsigned pitch, unsigned dh, void *buf, unsigned flags)
{
    g_to.s = s; g_to.buf = buf; g_to.left = l; g_to.top = t; g_to.pitch = pitch; g_to.flags = flags;
    g_save_rows = 0;
    o_tobuf(s, l, t, pitch, dh, buf, flags);
}

static unsigned (__stdcall *o_doframe)(void *);
static unsigned __stdcall h_doframe(void *s)
{
    const Vid *v = find(s);
    Dst d;
    unsigned r;
    int ok = v && dst_of(s, &d);
    if (ok && g_save_rows && g_save_s == s) {   /* undo the previous subtitle: the decoder only rewrites changed blocks */
        int y;
        for (y = 0; y < g_save_rows; y++) memcpy(d.buf + (g_save_y0 + y) * d.pitch, g_save + y * g_save_w, g_save_w * 2);
    }
    g_save_rows = 0;
    r = o_doframe(s);
    if (g_test && v && SM(s, 0x374) % 50 == 0) logf_("smk frame %d/%d", SM(s, 0x374), SM(s, 0xC));
    if (ok) {
        int mspf100 = SM(s, 0x10) > 0 ? SM(s, 0x10) * 100 : -SM(s, 0x10);   /* 1/100 ms per frame */
        unsigned ms = (unsigned)((__int64)SM(s, 0x374) * mspf100 / 100);
        uint32_t i;
        g_full = s;
        for (i = 0; i < v->n; i++)
            if (ms >= v->c[i].s && ms < v->c[i].e) {
                const uint8_t *t = g_res + v->c[i].off;
                int lh, adv, y0 = text_top(&d, t, &lh, &adv) - 2, y;
                if (y0 < 0) y0 = 0;
                if (d.w <= 640 && (d.h - y0) * d.w <= (int)(sizeof g_save / 2)) {
                    for (y = y0; y < d.h; y++) memcpy(g_save + (y - y0) * d.w, d.buf + y * d.pitch, d.w * 2);
                    g_save_s = s; g_save_y0 = y0; g_save_rows = d.h - y0; g_save_w = d.w;
                    draw_text(&d, t);
                }
                break;
            }
    }
    return r;
}

static unsigned (__stdcall *o_torect)(void *, unsigned);
static unsigned __stdcall h_torect(void *s, unsigned flags)
{
    unsigned r = o_torect(s, flags);
    if (s && s == g_full && s == g_to.s) {
        g_full = 0;
        SM(s, 0x380) = g_to.left; SM(s, 0x384) = g_to.top; SM(s, 0x388) = SM(s, 4); SM(s, 0x38C) = SM(s, 8);
        r = 1;
    }
    return r;
}

void subs_init(void)
{
    DWORD sz; const uint8_t *r = res_get(6, &sz); uint32_t i, nv;
    if (!r || sz < 8 || memcmp(r, "H3SB", 4)) { logf_("no subtitle resource"); return; }
    g_res = r; nv = *(uint32_t *)(r + 4);
    for (i = 0; i < nv && i < 8; i++) {
        const uint32_t *h = (const uint32_t *)(r + 8 + i * 20);
        g_vid[i].w = h[0]; g_vid[i].h = h[1]; g_vid[i].frames = h[2]; g_vid[i].n = h[3];
        g_vid[i].c = (const Cue *)(r + h[4]);
    }
    g_nvid = nv < 8 ? nv : 8;
    o_tobuf = iat_hook("smackw32.dll", "_SmackToBuffer@28", h_tobuf);
    o_doframe = iat_hook("smackw32.dll", "_SmackDoFrame@4", h_doframe);
    o_torect = iat_hook("smackw32.dll", "_SmackToBufferRect@8", h_torect);
    if (!o_tobuf || !o_doframe || !o_torect) {   /* all or nothing */
        if (o_tobuf) iat_hook("smackw32.dll", "_SmackToBuffer@28", o_tobuf);
        if (o_doframe) iat_hook("smackw32.dll", "_SmackDoFrame@4", o_doframe);
        if (o_torect) iat_hook("smackw32.dll", "_SmackToBufferRect@8", o_torect);
        logf_("subs: Smacker imports not found"); return;
    }
    logf_("subs: %d movies", g_nvid);
}
