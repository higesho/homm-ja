/* Test mode only: a minimal software DirectDraw (16-bit RGB565 system-memory surfaces) handed to the
   game instead of DDrawCompat, so tests never touch the real display (other programs may hold
   exclusive mode) and screenshots come straight from the primary surface. */
#include "h3jp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DD_OK 0
#define DDERR_GENERIC 0x80004005
typedef struct { DWORD dwSize, dwFlags, dwFourCC, dwRGBBitCount, dwR, dwG, dwB, dwA; } PF;
typedef struct { DWORD lo, hi; } CK;
typedef struct {
    DWORD dwSize, dwFlags, dwHeight, dwWidth; LONG lPitch; DWORD dwBackBufferCount, dwRefreshRate, dwAlphaBitDepth,
    dwReserved; void *lpSurface; CK ckDestOverlay, ckDestBlt, ckSrcOverlay, ckSrcBlt; PF ddpf; DWORD caps;
} SD;   /* DDSURFACEDESC, 0x6c bytes */

typedef struct Surf { void **vt; LONG ref; int w, h, pitch, primary; uint8_t *bits; CK ck; int hasck; struct Surf *back; } Surf;
typedef struct { void **vt; LONG ref; } Obj;

static Surf *g_primary;
static HWND g_ddhwnd;

static void fill_pf(PF *p) { memset(p, 0, sizeof *p); p->dwSize = 32; p->dwFlags = 0x40; p->dwRGBBitCount = 16; p->dwR = 0xF800; p->dwG = 0x07E0; p->dwB = 0x001F; }

/* ---------- generic ---------- */
static HRESULT __stdcall o_QI(Obj *o, const void *iid, void **out) { *out = o; o->ref++; return DD_OK; }
static ULONG __stdcall o_AddRef(Obj *o) { return ++o->ref; }
static ULONG __stdcall o_Release(Obj *o) { return --o->ref > 0 ? o->ref : 0; }   /* never freed (tiny leak, test only) */
static HRESULT __stdcall ok0(void *t) { return DD_OK; }
static HRESULT __stdcall ok1(void *t, void *a) { return DD_OK; }
static HRESULT __stdcall ok2(void *t, void *a, void *b) { return DD_OK; }
static HRESULT __stdcall ok3(void *t, void *a, void *b, void *c) { return DD_OK; }
static HRESULT __stdcall ok4(void *t, void *a, void *b, void *c, void *d) { return DD_OK; }
static HRESULT __stdcall ok5(void *t, void *a, void *b, void *c, void *d, void *e) { return DD_OK; }

/* ---------- clipper ---------- */
static void *g_clipvt[] = { o_QI, o_AddRef, o_Release, ok3 /*GetClipList*/, ok1 /*GetHWnd*/, ok2 /*Initialize*/,
                            ok1 /*IsClipListChanged*/, ok2 /*SetClipList*/, ok2 /*SetHWnd*/ };

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
static void copy_rect(Surf *d, int dx, int dy, Surf *s, RECT sr, int useck)
{
    int y, x, w = sr.right - sr.left, h = sr.bottom - sr.top;
    if (dx < 0) { sr.left -= dx; w += dx; dx = 0; }
    if (dy < 0) { sr.top -= dy; h += dy; dy = 0; }
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
static HRESULT __stdcall s_Blt(Surf *d, RECT *dr, Surf *s, RECT *sr, DWORD fl, void *fx)
{
    RECT D, S;
    static int nlog;
    rect_of(d, dr, &D);
    if (d->primary && dr && g_ddhwnd) {   /* windowed: destination is in screen coordinates */
        POINT p = { 0, 0 }; ClientToScreen(g_ddhwnd, &p);
        OffsetRect(&D, -p.x, -p.y);
    }
    if (nlog < 10) { nlog++; logf_("Blt prim=%d dst %d,%d-%d,%d src %p fl %x", d->primary, D.left, D.top, D.right, D.bottom, s, fl); }
    if (fl & 0x400) {   /* DDBLT_COLORFILL */
        uint16_t c = fx ? (uint16_t)((DWORD *)fx)[20] : 0; int x, y;   /* DDBLTFX.dwFillColor at +0x50 */
        for (y = max(D.top, 0); y < min(D.bottom, d->h); y++)
            for (x = max(D.left, 0); x < min(D.right, d->w); x++) ((uint16_t *)(d->bits + y * d->pitch))[x] = c;
        return DD_OK;
    }
    if (!s) return DD_OK;
    rect_of(s, sr, &S);
    copy_rect(d, D.left, D.top, s, S, (fl & 0x8000) != 0);   /* DDBLT_KEYSRC */
    return DD_OK;
}
static HRESULT __stdcall s_BltFast(Surf *d, DWORD x, DWORD y, Surf *s, RECT *sr, DWORD tr)
{
    RECT S; if (!s) return DD_OK;
    rect_of(s, sr, &S);
    copy_rect(d, (int)x, (int)y, s, S, (tr & 1) != 0);   /* DDBLTFAST_SRCCOLORKEY */
    return DD_OK;
}
static HRESULT __stdcall s_Flip(Surf *s, Surf *o, DWORD f)
{
    if (s->back) copy_rect(s, 0, 0, s->back, (RECT){ 0, 0, s->back->w, s->back->h }, 0);
    return DD_OK;
}
static HRESULT __stdcall s_GetAttached(Surf *s, void *caps, Surf **out)
{
    if (!s->back) s->back = new_surf(s->w, s->h, 0);
    *out = s->back; s->back->ref++; return DD_OK;
}
static HRESULT __stdcall s_GetCaps(Surf *s, DWORD *c) { *c = s->primary ? 0x200 : 0x840; return DD_OK; }
static HRESULT __stdcall s_GetColorKey(Surf *s, DWORD f, CK *k) { *k = s->ck; return DD_OK; }
static HRESULT __stdcall s_SetColorKey(Surf *s, DWORD f, CK *k) { if (k) { s->ck = *k; s->hasck = 1; } return DD_OK; }
static HRESULT __stdcall s_GetPF(Surf *s, PF *p) { fill_pf(p); return DD_OK; }
static void desc(Surf *s, SD *d)
{
    DWORD sz = d->dwSize ? d->dwSize : sizeof(SD);
    memset(d, 0, sz); d->dwSize = sz;
    d->dwFlags = 0x100f | 0x1000; d->dwWidth = s->w; d->dwHeight = s->h; d->lPitch = s->pitch; d->lpSurface = s->bits;
    fill_pf(&d->ddpf); d->caps = s->primary ? 0x200 : 0x840;
}
static HRESULT __stdcall s_GetDesc(Surf *s, SD *d) { desc(s, d); d->lpSurface = 0; return DD_OK; }
static HRESULT __stdcall s_Lock(Surf *s, RECT *r, SD *d, DWORD f, HANDLE e)
{
    desc(s, d);
    if (r) d->lpSurface = s->bits + r->top * s->pitch + r->left * 2;
    return DD_OK;
}
static HRESULT __stdcall s_GetDC(Surf *s, HDC *dc) { *dc = CreateCompatibleDC(0); return DD_OK; }
static HRESULT __stdcall s_ReleaseDC(Surf *s, HDC dc) { DeleteDC(dc); return DD_OK; }
static HRESULT __stdcall s_IsLost(Surf *s) { return DD_OK; }
static void *g_surfvt_[] = { o_QI, o_AddRef, o_Release, ok1 /*AddAttached*/, ok1 /*AddOverlayDirtyRect*/, s_Blt,
    ok3 /*BltBatch*/, s_BltFast, ok2 /*DeleteAttached*/, ok2 /*EnumAttached*/, ok3 /*EnumOverlayZ*/, s_Flip,
    s_GetAttached, ok1 /*GetBltStatus*/, s_GetCaps, ok1 /*GetClipper*/, s_GetColorKey, s_GetDC, ok1 /*GetFlipStatus*/,
    ok2 /*GetOverlayPosition*/, ok1 /*GetPalette*/, s_GetPF, s_GetDesc, ok2 /*Initialize*/, s_IsLost, s_Lock,
    s_ReleaseDC, ok0 /*Restore*/, ok1 /*SetClipper*/, s_SetColorKey, ok2 /*SetOverlayPosition*/, ok1 /*SetPalette*/,
    ok1 /*Unlock*/, ok5 /*UpdateOverlay*/, ok1 /*UpdateOverlayDisplay*/, ok2 /*UpdateOverlayZOrder*/,
    /* IDirectDrawSurface2/3 extras */ ok1, ok1, ok1, ok2 };

/* ---------- IDirectDraw ---------- */
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
    int prim = (sd->caps & 0x200) != 0;
    int w = (sd->dwFlags & 4) ? sd->dwWidth : 800, h = (sd->dwFlags & 2) ? sd->dwHeight : 600;
    Surf *s;
    if (prim) w = 800, h = 600;
    s = new_surf(w, h, prim);
    if (prim) g_primary = s;
    if (sd->dwFlags & 0x10000 && sd->caps & 0x200) {}   /* colour keys etc. ignored */
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
    memset(sd, 0, sz); sd->dwSize = sz; sd->dwFlags = 0x100e; sd->dwWidth = 800; sd->dwHeight = 600; sd->lPitch = 1600;
    fill_pf(&sd->ddpf); return DD_OK;
}
static HRESULT __stdcall d_SetCoop(Obj *d, HWND h, DWORD f) { g_ddhwnd = h; return DD_OK; }
static HRESULT __stdcall d_EnumModes(Obj *d, DWORD f, SD *sd, void *ctx, HRESULT (__stdcall *cb)(SD *, void *))
{
    SD m; memset(&m, 0, sizeof m); m.dwSize = sizeof m; m.dwFlags = 0x100e; m.dwWidth = 800; m.dwHeight = 600; m.lPitch = 1600;
    fill_pf(&m.ddpf); cb(&m, ctx); return DD_OK;
}
static HRESULT __stdcall d_GetVBStatus(Obj *d, BOOL *b) { if (b) *b = TRUE; return DD_OK; }
static HRESULT __stdcall d_GetScanLine(Obj *d, DWORD *l) { if (l) *l = 0; return DD_OK; }
static HRESULT __stdcall d_GetMonFreq(Obj *d, DWORD *f) { if (f) *f = 60; return DD_OK; }
static void *g_ddvt[] = { o_QI, o_AddRef, o_Release, ok0 /*Compact*/, d_CreateClipper, d_CreatePalette, d_CreateSurface,
    ok2 /*DuplicateSurface*/, d_EnumModes, ok4 /*EnumSurfaces*/, ok0 /*FlipToGDI*/, d_GetCaps, d_GetDisplayMode,
    ok2 /*GetFourCC*/, ok1 /*GetGDISurface*/, d_GetMonFreq, d_GetScanLine, d_GetVBStatus,
    ok1 /*Initialize*/, ok0 /*RestoreDisplayMode*/, d_SetCoop, ok3 /*SetDisplayMode*/, ok2 /*WaitForVB*/,
    /* IDirectDraw2+ */ ok3, ok2, ok2, ok1, ok1 };
static Obj g_dd = { g_ddvt, 1 };

static HRESULT WINAPI fake_DirectDrawCreate(void *guid, Obj **out, void *outer)
{
    g_surfvt = g_surfvt_;
    *out = &g_dd; g_dd.ref++;
    logf_("fake DirectDrawCreate");
    return DD_OK;
}

/* screenshot of the primary surface (24-bit BMP) */
int fakedd_shot(const char *path)
{
    FILE *f; int y, x, w, h, rs;
    BITMAPFILEHEADER fh; BITMAPINFOHEADER ih; uint8_t *row;
    if (!g_primary) return 0;
    w = g_primary->w; h = g_primary->h; rs = (w * 3 + 3) & ~3;
    f = fopen(path, "wb"); if (!f) return 0;
    memset(&fh, 0, sizeof fh); memset(&ih, 0, sizeof ih);
    fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + rs * h;
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = h; ih.biPlanes = 1; ih.biBitCount = 24;
    fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
    row = calloc(1, rs);
    for (y = h - 1; y >= 0; y--) {
        uint16_t *s = (uint16_t *)(g_primary->bits + y * g_primary->pitch);
        for (x = 0; x < w; x++) {
            uint16_t c = s[x];
            row[x * 3 + 0] = (uint8_t)((c & 31) * 255 / 31);
            row[x * 3 + 1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
            row[x * 3 + 2] = (uint8_t)((c >> 11) * 255 / 31);
        }
        fwrite(row, rs, 1, f);
    }
    free(row); fclose(f);
    return 1;
}

void fakedd_init(void)
{
    iat_hook("xdd.dll", "DirectDrawCreate", fake_DirectDrawCreate);
}
