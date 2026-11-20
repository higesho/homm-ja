/* Test mode only: a minimal software DirectDraw 7 (16-bit RGB565 system-memory surfaces) handed to the
   game instead of the GOG wrapper, so tests never touch the real display (other programs may hold
   exclusive mode). The primary surface is painted into the game window with GDI after each update. */
#include "h4jp.h"
#include <string.h>
#include <stdlib.h>

#define DD_OK 0
typedef struct { DWORD dwSize, dwFlags, dwFourCC, dwRGBBitCount, dwR, dwG, dwB, dwA; } PF;
typedef struct { DWORD lo, hi; } CK;
typedef struct {
    DWORD dwSize, dwFlags, dwHeight, dwWidth; LONG lPitch; DWORD dwBackBufferCount, dwRefreshRate, dwAlphaBitDepth,
    dwReserved; void *lpSurface; CK ckDestOverlay, ckDestBlt, ckSrcOverlay, ckSrcBlt; PF ddpf; DWORD caps[4]; DWORD stage;
} SD;   /* DDSURFACEDESC2, 0x7c bytes */

typedef struct Surf { void **vt; LONG ref; int w, h, pitch, primary; uint8_t *bits; CK ck; int hasck; struct Surf *back;
                      HDC dc; HBITMAP dib; uint8_t *dibbits; int dibpitch; } Surf;
typedef struct { void **vt; LONG ref; } Obj;

static Surf *g_primary;
static HWND g_ddhwnd;
static int g_W = 800, g_H = 600;

static void fill_pf(PF *p) { memset(p, 0, sizeof *p); p->dwSize = 32; p->dwFlags = 0x40; p->dwRGBBitCount = 16; p->dwR = 0xF800; p->dwG = 0x07E0; p->dwB = 0x001F; }

static void present(void)
{
    HDC dc; struct { BITMAPINFOHEADER h; DWORD m[3]; } bi;
    if (!g_primary || !g_ddhwnd) return;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = g_primary->w; bi.h.biHeight = -g_primary->h; bi.h.biPlanes = 1;
    bi.h.biBitCount = 16; bi.h.biCompression = BI_BITFIELDS; bi.m[0] = 0xF800; bi.m[1] = 0x07E0; bi.m[2] = 0x001F;
    dc = GetDC(g_ddhwnd);
    SetDIBitsToDevice(dc, 0, 0, g_primary->w, g_primary->h, 0, 0, 0, g_primary->h, g_primary->bits, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    ReleaseDC(g_ddhwnd, dc);
}

/* ---------- generic ---------- */
static HRESULT __stdcall o_QI(Obj *o, const void *iid, void **out) { *out = o; o->ref++; return DD_OK; }
static ULONG __stdcall o_AddRef(Obj *o) { return ++o->ref; }
static ULONG __stdcall o_Release(Obj *o) { return --o->ref > 0 ? o->ref : 0; }   /* never freed (test only) */
static HRESULT __stdcall ok0(void *t) { return DD_OK; }
static HRESULT __stdcall ok1(void *t, void *a) { return DD_OK; }
static HRESULT __stdcall ok2(void *t, void *a, void *b) { return DD_OK; }
static HRESULT __stdcall ok3(void *t, void *a, void *b, void *c) { return DD_OK; }
static HRESULT __stdcall ok4(void *t, void *a, void *b, void *c, void *d) { return DD_OK; }
static HRESULT __stdcall ok5(void *t, void *a, void *b, void *c, void *d, void *e) { return DD_OK; }

/* ---------- clipper / palette ---------- */
static void *g_clipvt[] = { o_QI, o_AddRef, o_Release, ok3, ok1, ok2, ok1, ok2, ok2, ok1, ok1, ok1 };

/* ---------- surface ---------- */
static void **g_surfvt;
static Surf *new_surf(int w, int h, int primary)
{
    Surf *s = calloc(1, sizeof *s);
    s->vt = g_surfvt; s->ref = 1; s->w = w; s->h = h; s->pitch = w * 2; s->primary = primary;
    s->bits = calloc(1, (size_t)s->pitch * h + 16);
    return s;
}
static void rect_of(Surf *s, const RECT *r, RECT *o) { if (r) *o = *r; else { o->left = o->top = 0; o->right = s->w; o->bottom = s->h; } }
static void copy_rect(Surf *d, RECT D, Surf *s, RECT sr, int useck)
{
    int y, x, w = sr.right - sr.left, h = sr.bottom - sr.top, dw = D.right - D.left, dh = D.bottom - D.top;
    int dx = D.left, dy = D.top;
    if (dw != w || dh != h) {          /* stretch (nearest) */
        for (y = 0; y < dh; y++) {
            int yy = dy + y, sy = sr.top + y * h / dh;
            if (yy < 0 || yy >= d->h || sy < 0 || sy >= s->h) continue;
            for (x = 0; x < dw; x++) {
                int xx = dx + x, sx = sr.left + x * w / dw; uint16_t c;
                if (xx < 0 || xx >= d->w || sx < 0 || sx >= s->w) continue;
                c = ((uint16_t *)(s->bits + sy * s->pitch))[sx];
                if (!useck || c != (uint16_t)s->ck.lo) ((uint16_t *)(d->bits + yy * d->pitch))[xx] = c;
            }
        }
        return;
    }
    if (dx < 0) { sr.left -= dx; w += dx; dx = 0; }
    if (dy < 0) { sr.top -= dy; h += dy; dy = 0; }
    if (sr.left < 0) { dx -= sr.left; w += sr.left; sr.left = 0; }
    if (sr.top < 0) { dy -= sr.top; h += sr.top; sr.top = 0; }
    if (dx + w > d->w) w = d->w - dx;
    if (dy + h > d->h) h = d->h - dy;
    if (sr.left + w > s->w) w = s->w - sr.left;
    if (sr.top + h > s->h) h = s->h - sr.top;
    if (w <= 0 || h <= 0) return;
    for (y = 0; y < h; y++) {
        uint16_t *dp = (uint16_t *)(d->bits + (dy + y) * d->pitch) + dx;
        uint16_t *sp = (uint16_t *)(s->bits + (sr.top + y) * s->pitch) + sr.left;
        if (!useck) memmove(dp, sp, w * 2);
        else for (x = 0; x < w; x++) if (sp[x] != (uint16_t)s->ck.lo) dp[x] = sp[x];
    }
}
static int g_blog;
static HRESULT __stdcall s_Blt(Surf *d, RECT *dr, Surf *s, RECT *sr, DWORD fl, void *fx)
{
    RECT D, S;
    rect_of(d, dr, &D);
    if (g_blog > 0 && d->primary) { g_blog--; logf_("Blt prim %d,%d-%d,%d from %p(%dx%d) %d,%d-%d,%d fl %x", D.left, D.top, D.right, D.bottom, s, s ? s->w : 0, s ? s->h : 0,
        sr ? sr->left : -1, sr ? sr->top : -1, sr ? sr->right : -1, sr ? sr->bottom : -1, fl); }
    /* windowed: the destination is in screen coordinates, which testio.c makes equal to client coordinates */
    if (fl & 0x400) {   /* DDBLT_COLORFILL */
        uint16_t c = fx ? (uint16_t)((DWORD *)fx)[20] : 0; int x, y;
        for (y = max(D.top, 0); y < min(D.bottom, d->h); y++)
            for (x = max(D.left, 0); x < min(D.right, d->w); x++) ((uint16_t *)(d->bits + y * d->pitch))[x] = c;
    } else if (s) {
        rect_of(s, sr, &S);
        copy_rect(d, D, s, S, (fl & 0x8000) != 0);
    }
    if (d->primary) present();
    return DD_OK;
}
static HRESULT __stdcall s_BltFast(Surf *d, DWORD x, DWORD y, Surf *s, RECT *sr, DWORD tr)
{
    if (g_blog > 0 && d->primary) { g_blog--; logf_("BltFast prim %d,%d", x, y); }
    RECT S, D; if (!s) return DD_OK;
    rect_of(s, sr, &S);
    D.left = x; D.top = y; D.right = x + S.right - S.left; D.bottom = y + S.bottom - S.top;
    copy_rect(d, D, s, S, (tr & 1) != 0);
    if (d->primary) present();
    return DD_OK;
}
static HRESULT __stdcall s_Flip(Surf *s, Surf *o, DWORD f)
{
    if (g_blog > 0) { g_blog--; logf_("Flip"); }
    if (s->back) copy_rect(s, (RECT){ 0, 0, s->w, s->h }, s->back, (RECT){ 0, 0, s->back->w, s->back->h }, 0);
    present();
    return DD_OK;
}
static HRESULT __stdcall s_GetAttached(Surf *s, DWORD *caps, Surf **out)
{
    if (!s->back) s->back = new_surf(s->w, s->h, 0);
    *out = s->back; s->back->ref++; return DD_OK;
}
static HRESULT __stdcall s_GetCaps(Surf *s, DWORD *c) { c[0] = s->primary ? 0x200 : 0x840; c[1] = c[2] = c[3] = 0; return DD_OK; }
static HRESULT __stdcall s_GetColorKey(Surf *s, DWORD f, CK *k) { *k = s->ck; return DD_OK; }
static HRESULT __stdcall s_SetColorKey(Surf *s, DWORD f, CK *k) { if (k) { s->ck = *k; s->hasck = 1; } return DD_OK; }
static HRESULT __stdcall s_GetPF(Surf *s, PF *p) { fill_pf(p); return DD_OK; }
static void desc(Surf *s, SD *d)
{
    DWORD sz = d->dwSize ? d->dwSize : sizeof(SD);
    memset(d, 0, sz); d->dwSize = sz;
    d->dwFlags = 0x100f | 0x1000; d->dwWidth = s->w; d->dwHeight = s->h; d->lPitch = s->pitch; d->lpSurface = s->bits;
    fill_pf(&d->ddpf); d->caps[0] = s->primary ? 0x200 : 0x840;
}
static HRESULT __stdcall s_GetDesc(Surf *s, SD *d) { desc(s, d); d->lpSurface = 0; return DD_OK; }
static HRESULT __stdcall s_Lock(Surf *s, RECT *r, SD *d, DWORD f, HANDLE e)
{
    if (g_blog > 0 && s->primary) { g_blog--; logf_("Lock prim %p", r); }
    desc(s, d);
    if (r) d->lpSurface = s->bits + r->top * s->pitch + r->left * 2;
    return DD_OK;
}
static HRESULT __stdcall s_Unlock(Surf *s, RECT *r) { if (s->primary) present(); return DD_OK; }
/* GDI drawing into a surface (the adventure map view is drawn this way): a DIB section copy */
static HRESULT __stdcall s_GetDC(Surf *s, HDC *dc)
{
    int y;
    if (!s->dc) {
        struct { BITMAPINFOHEADER h; DWORD m[3]; } bi;
        memset(&bi, 0, sizeof bi);
        bi.h.biSize = sizeof bi.h; bi.h.biWidth = s->w; bi.h.biHeight = -s->h; bi.h.biPlanes = 1;
        bi.h.biBitCount = 16; bi.h.biCompression = BI_BITFIELDS; bi.m[0] = 0xF800; bi.m[1] = 0x07E0; bi.m[2] = 0x001F;
        s->dc = CreateCompatibleDC(0);
        s->dib = CreateDIBSection(s->dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&s->dibbits, 0, 0);
        SelectObject(s->dc, s->dib);
        s->dibpitch = (s->w * 2 + 3) & ~3;
    }
    GdiFlush();
    for (y = 0; y < s->h; y++) memcpy(s->dibbits + y * s->dibpitch, s->bits + y * s->pitch, s->w * 2);
    *dc = s->dc;
    return DD_OK;
}
static HRESULT __stdcall s_ReleaseDC(Surf *s, HDC dc)
{
    int y;
    if (!s->dc) return DD_OK;
    GdiFlush();
    for (y = 0; y < s->h; y++) memcpy(s->bits + y * s->pitch, s->dibbits + y * s->dibpitch, s->w * 2);
    if (s->primary) present();
    return DD_OK;
}
static HRESULT __stdcall s_IsLost(Surf *s) { return DD_OK; }
static HRESULT __stdcall s_GetDD(Surf *s, void **o);
static HRESULT __stdcall s_Uniq(Surf *s, DWORD *v) { if (v) *v = 1; return DD_OK; }
static void *g_surfvt_[] = { o_QI, o_AddRef, o_Release, ok1 /*AddAttached*/, ok1 /*AddOverlayDirtyRect*/, s_Blt,
    ok3 /*BltBatch*/, s_BltFast, ok2 /*DeleteAttached*/, ok2 /*EnumAttached*/, ok3 /*EnumOverlayZ*/, s_Flip,
    s_GetAttached, ok1 /*GetBltStatus*/, s_GetCaps, ok1 /*GetClipper*/, s_GetColorKey, s_GetDC, ok1 /*GetFlipStatus*/,
    ok2 /*GetOverlayPosition*/, ok1 /*GetPalette*/, s_GetPF, s_GetDesc, ok2 /*Initialize*/, s_IsLost, s_Lock,
    s_ReleaseDC, ok0 /*Restore*/, ok1 /*SetClipper*/, s_SetColorKey, ok2 /*SetOverlayPosition*/, ok1 /*SetPalette*/,
    s_Unlock, ok5 /*UpdateOverlay*/, ok1 /*UpdateOverlayDisplay*/, ok2 /*UpdateOverlayZOrder*/,
    s_GetDD, ok1 /*PageLock*/, ok1 /*PageUnlock*/, ok2 /*SetSurfaceDesc*/,
    ok4 /*SetPrivateData*/, ok3 /*GetPrivateData*/, ok1 /*FreePrivateData*/, s_Uniq, ok0 /*ChangeUniqueness*/,
    ok1 /*SetPriority*/, ok1 /*GetPriority*/, ok1 /*SetLOD*/, ok1 /*GetLOD*/ };

/* ---------- IDirectDraw7 ---------- */
static HRESULT __stdcall d_CreateClipper(Obj *d, DWORD f, Obj **out, void *u)
{
    Obj *c = calloc(1, sizeof *c); c->vt = g_clipvt; c->ref = 1; *out = c; return DD_OK;
}
static HRESULT __stdcall d_CreatePalette(Obj *d, DWORD f, void *e, Obj **out, void *u)
{
    Obj *c = calloc(1, sizeof *c); c->vt = g_clipvt; c->ref = 1; *out = c; return DD_OK;
}
static HRESULT __stdcall d_CreateSurface(Obj *d, SD *sd, Surf **out, void *u)
{
    int prim = (sd->caps[0] & 0x200) != 0;
    int w = (sd->dwFlags & 4) ? sd->dwWidth : g_W, h = (sd->dwFlags & 2) ? sd->dwHeight : g_H;
    Surf *s;
    if (prim) w = g_W, h = g_H;
    s = new_surf(w, h, prim);
    if (prim) g_primary = s;
    *out = s;
    return DD_OK;
}
static HRESULT __stdcall d_GetCaps(Obj *d, void *a, void *b)
{
    if (a) { DWORD sz = *(DWORD *)a; memset(a, 0, sz); *(DWORD *)a = sz; }
    if (b) { DWORD sz = *(DWORD *)b; memset(b, 0, sz); *(DWORD *)b = sz; }
    return DD_OK;
}
static HRESULT __stdcall d_GetDisplayMode(Obj *d, SD *sd)
{
    DWORD sz = sd->dwSize ? sd->dwSize : sizeof(SD);
    memset(sd, 0, sz); sd->dwSize = sz; sd->dwFlags = 0x100e; sd->dwWidth = g_W; sd->dwHeight = g_H; sd->lPitch = g_W * 2;
    fill_pf(&sd->ddpf); return DD_OK;
}
static HRESULT __stdcall d_SetCoop(Obj *d, HWND h, DWORD f) { g_ddhwnd = h; logf_("SetCooperativeLevel %p %x", h, f); return DD_OK; }
static HRESULT __stdcall d_SetMode(Obj *d, DWORD w, DWORD h, DWORD bpp, DWORD r, DWORD f)
{
    logf_("SetDisplayMode %ux%u %u", w, h, bpp); g_W = w; g_H = h; return DD_OK;
}
static HRESULT __stdcall d_EnumModes(Obj *d, DWORD f, SD *sd, void *ctx, HRESULT (__stdcall *cb)(SD *, void *))
{
    static const int m[][2] = { {800, 600}, {1024, 768}, {1280, 1024} };
    int i;
    for (i = 0; i < 3; i++) {
        SD s; memset(&s, 0, sizeof s); s.dwSize = sizeof s; s.dwFlags = 0x100e; s.dwWidth = m[i][0]; s.dwHeight = m[i][1];
        s.lPitch = m[i][0] * 2; fill_pf(&s.ddpf);
        if (!cb(&s, ctx)) break;
    }
    return DD_OK;
}
static HRESULT __stdcall d_GetVBStatus(Obj *d, BOOL *b) { if (b) *b = TRUE; return DD_OK; }
static HRESULT __stdcall d_GetScanLine(Obj *d, DWORD *l) { if (l) *l = 0; return DD_OK; }
static HRESULT __stdcall d_GetMonFreq(Obj *d, DWORD *f) { if (f) *f = 60; return DD_OK; }
static HRESULT __stdcall d_GetVidMem(Obj *d, void *c, DWORD *t, DWORD *f) { if (t) *t = 64 << 20; if (f) *f = 32 << 20; return DD_OK; }
static HRESULT __stdcall d_DevId(Obj *d, void *id, DWORD f) { if (id) memset(id, 0, 0x438); return DD_OK; }
static void *g_ddvt[] = { o_QI, o_AddRef, o_Release, ok0 /*Compact*/, d_CreateClipper, d_CreatePalette, d_CreateSurface,
    ok2 /*DuplicateSurface*/, d_EnumModes, ok4 /*EnumSurfaces*/, ok0 /*FlipToGDI*/, d_GetCaps, d_GetDisplayMode,
    ok2 /*GetFourCC*/, ok1 /*GetGDISurface*/, d_GetMonFreq, d_GetScanLine, d_GetVBStatus,
    ok1 /*Initialize*/, ok0 /*RestoreDisplayMode*/, d_SetCoop, d_SetMode, ok2 /*WaitForVB*/,
    d_GetVidMem, ok2 /*GetSurfaceFromDC*/, ok0 /*RestoreAllSurfaces*/, ok0 /*TestCooperativeLevel*/, d_DevId,
    ok3 /*StartModeTest*/, ok2 /*EvaluateMode*/ };
static Obj g_dd = { g_ddvt, 1 };
static HRESULT __stdcall s_GetDD(Surf *s, void **o) { *o = &g_dd; return DD_OK; }

static HRESULT WINAPI fake_DirectDrawCreateEx(void *guid, Obj **out, const void *iid, void *outer)
{
    g_surfvt = g_surfvt_;
    *out = &g_dd; g_dd.ref++;
    logf_("fake DirectDrawCreateEx");
    test_rehook();
    return DD_OK;
}

void fakedd_log(int n) { g_blog = n; }
void fakedd_init(void)
{
    iat_hook("ddraw.dll", "DirectDrawCreateEx", fake_DirectDrawCreateEx);
}
