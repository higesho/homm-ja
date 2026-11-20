/* Test mode only (H3JP_TEST=1): windowed, unique window title "H3JP TEST", input injected through
   the game's own PeekMessage/GetMessage calls (real mouse/keyboard input is dropped), screenshots
   from the game's 16-bit screen buffer. Driven by tools/win.py through shared memory H3JP_TESTIO. */
#include "h3jp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <intrin.h>

void *iat_hook(const char *dll, const char *fn, void *nf);

typedef struct { LONG type, a, b, c; } Cmd;
typedef struct {
    volatile LONG magic, wr, rd;
    Cmd cmd[256];
    volatile LONG shot_req, shot_done;
    char shot_path[260];
    volatile LONG frame, busy;
    volatile LONG hwnd;
} Shm;
enum { C_KEYDOWN = 1, C_KEYUP, C_MOVE, C_BDOWN, C_BUP, C_WAIT, C_CHAR };

static Shm *g_shm;
static BOOL (WINAPI *o_SetWindowPos)(HWND, HWND, int, int, int, int, UINT) = SetWindowPos;
static HWND g_hwnd;
static POINT g_mouse = { 400, 300 };
static uint8_t g_keys[256];
static MSG g_q[64]; static int g_qh, g_qt;
static DWORD g_wait_until;

static void qpush(UINT m, WPARAM w, LPARAM l)
{
    MSG *q = &g_q[g_qt & 63];
    if (g_qt - g_qh >= 64) return;
    memset(q, 0, sizeof *q);
    q->hwnd = g_hwnd; q->message = m; q->wParam = w; q->lParam = l; q->time = GetTickCount();
    q->pt = g_mouse; ClientToScreen(g_hwnd, &q->pt);
    g_qt++;
}

static WPARAM mflags(void)
{
    WPARAM w = 0;
    if (g_keys[VK_LBUTTON]) w |= MK_LBUTTON;
    if (g_keys[VK_RBUTTON]) w |= MK_RBUTTON;
    if (g_keys[VK_SHIFT]) w |= MK_SHIFT;
    if (g_keys[VK_CONTROL]) w |= MK_CONTROL;
    return w;
}

/* HotA/HD Mod: the game's own 16-bit screen buffer (window manager [0x6992D0] +0x40, SoD 3.2) */
static void wm_shot(const char *path)
{
    uint8_t *wm, *pcx, *buf; int w, h, pitch, x, y, rs; FILE *f; uint8_t *row;
    BITMAPFILEHEADER fh; BITMAPINFOHEADER ih;
    __try {
        wm = *(uint8_t **)0x6992D0; pcx = *(uint8_t **)(wm + 0x40);
        w = *(int *)(pcx + 0x24); h = *(int *)(pcx + 0x28); pitch = *(int *)(pcx + 0x2C); buf = *(uint8_t **)(pcx + 0x30);
    } __except (1) { logf_("wm_shot: no screen"); return; }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !buf) { logf_("wm_shot: bad %d %d", w, h); return; }
    f = fopen(path, "wb"); if (!f) return;
    rs = (w * 3 + 3) & ~3; row = calloc(1, rs);
    memset(&fh, 0, sizeof fh); memset(&ih, 0, sizeof ih);
    fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + rs * h;
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = h; ih.biPlanes = 1; ih.biBitCount = 24;
    fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
    for (y = h - 1; y >= 0; y--) {
        if (pitch >= w * 4) {
            uint32_t *s = (uint32_t *)(buf + y * pitch);
            for (x = 0; x < w; x++) { row[x*3] = (uint8_t)s[x]; row[x*3+1] = (uint8_t)(s[x] >> 8); row[x*3+2] = (uint8_t)(s[x] >> 16); }
        } else {
            uint16_t *s = (uint16_t *)(buf + y * pitch);
            for (x = 0; x < w; x++) { uint16_t c = s[x];
                row[x*3] = (uint8_t)((c & 31) * 255 / 31); row[x*3+1] = (uint8_t)(((c >> 5) & 63) * 255 / 63); row[x*3+2] = (uint8_t)((c >> 11) * 255 / 31); }
        }
        fwrite(row, rs, 1, f);
    }
    free(row); fclose(f);
}

static void pump(void)
{
    static int lastfull = -1;
    if (*(int *)A->t_fullvar != lastfull) { lastfull = *(int *)A->t_fullvar; logf_("full screen var = %d", lastfull); }
    if (!g_shm) return;
    g_shm->frame++;
    if (!g_hwnd) return;
    {   static int act;
        if (!act) { act = 1; qpush(WM_ACTIVATEAPP, 1, 0); qpush(WM_ACTIVATE, WA_ACTIVE, 0); qpush(WM_SETFOCUS, 0, 0); }
    }
    if (g_shm->shot_req != g_shm->shot_done) {
        if (!fakedd_shot(g_shm->shot_path)) wm_shot(g_shm->shot_path);
        g_shm->shot_done = g_shm->shot_req;
    }
    if (g_wait_until && (LONG)(GetTickCount() - g_wait_until) < 0) return;
    g_wait_until = 0;
    while (g_shm->rd != g_shm->wr && g_qt - g_qh < 48) {
        Cmd c = g_shm->cmd[g_shm->rd & 255];
        g_shm->rd++;
        switch (c.type) {
        case C_KEYDOWN:
            g_keys[c.a & 255] = 0x80;
            qpush(WM_KEYDOWN, c.a, 1 | (MapVirtualKeyA(c.a, 0) << 16));
            break;
        case C_KEYUP:
            g_keys[c.a & 255] = 0;
            qpush(WM_KEYUP, c.a, 1 | (MapVirtualKeyA(c.a, 0) << 16) | 0xC0000000);
            break;
        case C_CHAR: qpush(WM_CHAR, c.a, 1); break;
        case C_MOVE: g_mouse.x = c.a; g_mouse.y = c.b; qpush(WM_MOUSEMOVE, mflags(), MAKELPARAM(c.a, c.b)); break;
        case C_BDOWN:
            g_keys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0x80;
            qpush(c.a ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, mflags(), MAKELPARAM(g_mouse.x, g_mouse.y));
            break;
        case C_BUP:
            g_keys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0;
            qpush(c.a ? WM_RBUTTONUP : WM_LBUTTONUP, mflags(), MAKELPARAM(g_mouse.x, g_mouse.y));
            break;
        case C_WAIT: g_wait_until = GetTickCount() + c.a; goto out;
        }
    }
out:
    g_shm->busy = (g_shm->rd != g_shm->wr) || g_wait_until || g_qt != g_qh;
}

static int is_input(UINT m)
{
    return (m >= WM_KEYFIRST && m <= WM_KEYLAST) || (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) ||
           m == WM_NCMOUSEMOVE || (m >= WM_NCLBUTTONDOWN && m <= WM_NCMBUTTONDBLCLK);
}

static BOOL (WINAPI *o_PeekMessageA)(LPMSG, HWND, UINT, UINT, UINT);
static BOOL WINAPI h_PeekMessageA(LPMSG m, HWND h, UINT a, UINT b, UINT r)
{
    static int nlog;
    if (nlog < 30 && (a || b)) { nlog++; logf_("Peek filter %x-%x r=%x from %p", a, b, r, _ReturnAddress()); }
    pump();
    if (g_qh != g_qt && ((!a && !b) || (g_q[g_qh & 63].message >= a && g_q[g_qh & 63].message <= b))) {
        *m = g_q[g_qh & 63];
        if (r & PM_REMOVE) g_qh++;
        return TRUE;
    }
    for (;;) {
        BOOL ok = o_PeekMessageA(m, h, a, b, r);
        if (!ok) return FALSE;
        if (is_input(m->message)) {
            if (!(r & PM_REMOVE)) o_PeekMessageA(m, h, m->message, m->message, PM_REMOVE);
            continue;   /* drop real input */
        }
        if ((m->message == WM_ACTIVATEAPP && !m->wParam) || m->message == WM_KILLFOCUS) {
            if (!(r & PM_REMOVE)) o_PeekMessageA(m, h, m->message, m->message, PM_REMOVE);
            continue;
        }
        return TRUE;
    }
}
static BOOL (WINAPI *o_GetMessageA)(LPMSG, HWND, UINT, UINT);
static BOOL WINAPI h_GetMessageA(LPMSG m, HWND h, UINT a, UINT b)
{
    for (;;) {
        if (h_PeekMessageA(m, h, a, b, PM_REMOVE)) return m->message != WM_QUIT;
        MsgWaitForMultipleObjects(0, 0, FALSE, 10, QS_ALLINPUT);
    }
}
static BOOL (WINAPI *o_GetCursorPos)(LPPOINT);
static BOOL WINAPI h_GetCursorPos(LPPOINT p)
{
    if (!g_hwnd) return o_GetCursorPos(p);
    *p = g_mouse; ClientToScreen(g_hwnd, p);
    return TRUE;
}
static SHORT (WINAPI *o_GetKeyState)(int);
static SHORT WINAPI h_GetKeyState(int vk) { return g_keys[vk & 255] ? (SHORT)0xff80 : 0; }

static HWND (WINAPI *o_CreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static HWND WINAPI h_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD st, int x, int y, int w, int h,
                                     HWND par, HMENU menu, HINSTANCE in, LPVOID p)
{
    HWND r;
    logf_("CreateWindowExA %s %s par=%p", cls && HIWORD(cls) ? cls : "#", title ? title : "", par);
    r = o_CreateWindowExA(ex, cls, title, st, x, y, w, h, par, menu, in, p);
    if (r && !par && !g_hwnd) {
        g_hwnd = r;
        SetWindowTextA(r, "H3JP TEST");
        o_SetWindowPos(r, HWND_BOTTOM, -3000, 100, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);   /* keep it out of the way */
        if (g_shm) g_shm->hwnd = (LONG)(LONG_PTR)r;
        logf_("window %p %s full=%d", r, title ? title : "", *(int *)A->t_fullvar);
        {   uint32_t fn[] = { A->drawchar, A->drawline, A->textdraw, A->strwidth, A->linescount, A->totalwidth, A->maxlinewidth,
                              A->maxwordwidth, A->maxlinewidthw, A->splitlines, A->readstring, A->strncpy_ };
            int i; for (i = 0; i < 12; i++) logf_("hook %08x: %02x %08x", fn[i], *(uint8_t *)fn[i], *(uint32_t *)(fn[i] + 1) + fn[i] + 5); }
    }
    return r;
}

static LONG (WINAPI *o_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
static LONG WINAPI h_RegQueryValueExA(HKEY k, LPCSTR n, LPDWORD rs, LPDWORD t, LPBYTE d, LPDWORD cb)
{
    logf_("RegQuery %s", n ? n : "(null)");
    if (n && (!_stricmp(n, "Main Game Full Screen") || !_stricmp(n, "Show Intro") || !_stricmp(n, "Music Volume") ||
              !_stricmp(n, "Sound Volume") || !_stricmp(n, "Last Music Volume") || !_stricmp(n, "Last Sound Volume"))) {
        if (t) *t = REG_DWORD;
        if (d && cb && *cb >= 4) *(DWORD *)d = 0;
        if (cb) *cb = 4;
        return ERROR_SUCCESS;
    }
    return o_RegQueryValueExA(k, n, rs, t, d, cb);
}
static LONG WINAPI h_RegSetValueExA(HKEY k, LPCSTR n, DWORD r, DWORD t, const BYTE *d, DWORD cb)
{
    return ERROR_SUCCESS;   /* test runs never change the player's settings */
}

static BOOL WINAPI h_SetForegroundWindow(HWND h) { return TRUE; }
static BOOL WINAPI h_ClipCursor(const RECT *r) { return TRUE; }
static BOOL WINAPI h_SetCursorPos(int x, int y) { return TRUE; }
static BOOL WINAPI h_SetWindowPos(HWND h, HWND a, int x, int y, int w, int hh, UINT f)
{
    if (h == g_hwnd) { f |= SWP_NOACTIVATE | SWP_NOZORDER; f &= ~SWP_SHOWWINDOW; x = -3000; y = 100; f &= ~SWP_NOMOVE; }
    return o_SetWindowPos(h, a, x, y, w, hh, f);
}
static SHORT WINAPI h_GetAsyncKeyState(int vk) { return g_keys[vk & 255] ? (SHORT)0x8000 : 0; }
static BOOL (WINAPI *o_ShowWindow)(HWND, int);
static BOOL WINAPI h_ShowWindow(HWND h, int c)
{
    if (h == g_hwnd && c != SW_HIDE) { o_SetWindowPos(h, HWND_BOTTOM, -3000, 100, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE); c = SW_SHOWNOACTIVATE; }
    return o_ShowWindow(h, c);
}
static BOOL (WINAPI *o_MoveWindow)(HWND, int, int, int, int, BOOL);
static BOOL WINAPI h_MoveWindow(HWND h, int x, int y, int w, int hh, BOOL r) { return o_MoveWindow(h, h == g_hwnd || !g_hwnd ? -3000 : x, h == g_hwnd || !g_hwnd ? 100 : y, w, hh, r); }

void test_init(void)
{
    fakedd_init();
    o_SetWindowPos = api_hook("user32.dll", "SetWindowPos", h_SetWindowPos);
    api_hook("user32.dll", "SetForegroundWindow", h_SetForegroundWindow);
    api_hook("user32.dll", "ClipCursor", h_ClipCursor);
    api_hook("user32.dll", "SetCursorPos", h_SetCursorPos);
    api_hook("user32.dll", "GetAsyncKeyState", h_GetAsyncKeyState);
    o_ShowWindow = api_hook("user32.dll", "ShowWindow", h_ShowWindow);
    o_MoveWindow = api_hook("user32.dll", "MoveWindow", h_MoveWindow);
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, sizeof(Shm), "H3JP_TESTIO");
    if (m) g_shm = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm));
    if (g_shm) { memset(g_shm, 0, sizeof *g_shm); g_shm->magic = 0x504a3348; }
    if (!A->hota) {   /* GOG Complete: windowed needs the 16-bit desktop check to pass; skip videos */
        static const uint8_t r1[] = { 0xB0, 0x01, 0xC3 };
        uint8_t r2[] = { 0xC7, 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0xEB, 0x13 };   /* full screen := 0 */
        static const uint8_t r3[] = { 0xE9, 0x20, 0x01, 0x00, 0x00, 0x90 };
        static const uint8_t r4[] = { 0xE9, 0x63, 0x00, 0x00, 0x00, 0x90 };
        *(uint32_t *)(r2 + 2) = A->t_fullvar;
        patch_bytes(A->t_bpp16, r1, 3);
        if (*(uint8_t *)A->t_fullchk == 0xA1) patch_bytes(A->t_fullchk, r2, sizeof r2);
        if (*(uint16_t *)A->t_speedtest == 0x840F) patch_bytes(A->t_speedtest, r3, sizeof r3);
        {   /* H3JP_VIDEO=intro: play the opening movie; =endgame: play the Restoration ending in its place */
            char v[16] = "";
            GetEnvironmentVariableA("H3JP_VIDEO", v, sizeof v);
            if (!v[0]) { if (A->t_intro && *(uint16_t *)A->t_intro == 0x3539) patch_bytes(A->t_intro, r4, sizeof r4); }
            else if (A->t_intro && *(uint16_t *)A->t_intro == 0x3539) {
                static const uint8_t r5[] = { 0xEB, 0x0E };          /* always play */
                patch_bytes(A->t_intro, r5, sizeof r5);
                if (!_stricmp(v, "endgame") && *(uint32_t *)0x683c18 == 0x684920) {
                    uint32_t e = 0x68490c; patch_bytes(0x683c18, &e, 4);
                }
            }
        }
    }
    o_PeekMessageA = api_hook("user32.dll", "PeekMessageA", h_PeekMessageA);
    o_GetMessageA = api_hook("user32.dll", "GetMessageA", h_GetMessageA);
    o_GetCursorPos = api_hook("user32.dll", "GetCursorPos", h_GetCursorPos);
    o_GetKeyState = api_hook("user32.dll", "GetKeyState", h_GetKeyState);
    o_CreateWindowExA = api_hook("user32.dll", "CreateWindowExA", h_CreateWindowExA);
    o_RegQueryValueExA = iat_hook("advapi32.dll", "RegQueryValueExA", h_RegQueryValueExA);
    iat_hook("advapi32.dll", "RegSetValueExA", h_RegSetValueExA);
}
