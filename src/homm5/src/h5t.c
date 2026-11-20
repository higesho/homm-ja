/* HoMM5 test harness (H5JP_TEST=1): runs the game windowed and off-screen, redirects
   "My Documents" (profiles, saves, options) to _jp\test\docs so the player's settings are never touched,
   and lets tools/win.py inject input and take back-buffer screenshots (testio.c).
   The Japanese patch itself is data only (a .pak); this DLL is never shipped. */
#include "h5t.h"
#include <stdio.h>
#include <intrin.h>
#include <d3d9.h>
#include <shlobj.h>

int g_test;
HWND g_hwnd;
static FILE *g_log;
static CRITICAL_SECTION g_logcs;

void dlog(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    EnterCriticalSection(&g_logcs);
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
    LeaveCriticalSection(&g_logcs);
}
void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD old;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}
void patch_jmp(uint32_t at, void *to)
{
    uint8_t b[5];
    b[0] = 0xe9; *(int32_t *)(b + 1) = (int32_t)((uint32_t)to - (at + 5));
    patch_bytes(at, b, 5);
}
void *iat_hook_mod(HMODULE mod, const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)mod;
    IMAGE_NT_HEADERS *nt; IMAGE_IMPORT_DESCRIPTOR *imp;
    if (!base) return NULL;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp((const char *)(base + imp->Name), dll)) continue;
        ot = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        ft = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn; DWORD old; void *o;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + ot->u1.AddressOfData);
            if (strcmp((const char *)ibn->Name, func)) continue;
            o = (void *)ft->u1.Function;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            return o;
        }
    }
    dlog("IAT %s!%s not found", dll, func);
    return NULL;
}
static void *iat_hook(const char *dll, const char *func, void *repl) { return iat_hook_mod(GetModuleHandleA(NULL), dll, func, repl); }

/* ---------------------------------------------------------------- windowed, off-screen */
static int g_wx = -3200, g_wy = 0;
static HRESULT (STDMETHODCALLTYPE *o_CreateDevice)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
static HRESULT (STDMETHODCALLTYPE *o_Reset)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
static HRESULT (STDMETHODCALLTYPE *o_Present)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);

static void force_windowed(D3DPRESENT_PARAMETERS *pp)
{
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (pp->SwapEffect == D3DSWAPEFFECT_FLIP) pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
}
static HRESULT STDMETHODCALLTYPE h_Present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    testio_present(d);
    return o_Present(d, a, b, w, r);
}
static HRESULT STDMETHODCALLTYPE h_Reset(IDirect3DDevice9 *d, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    force_windowed(pp);
    hr = o_Reset(d, pp);
    dlog("Reset %dx%d -> %08x", pp->BackBufferWidth, pp->BackBufferHeight, hr);
    return hr;
}
static int hotpatch(uint8_t *f, void *hook, void **orig, const char *what)
{
    static const uint8_t jshort[2] = { 0xeb, 0xf9 };
    if (f[0] == 0xeb && f[1] == 0xf9) return 1;
    if (f[0] != 0x8b || f[1] != 0xff || f[-5] != 0xcc || f[-1] != 0xcc) { dlog("%s: no hot-patch prologue", what); return 0; }
    *orig = (void *)(f + 2);
    patch_jmp((uint32_t)(f - 5), hook);
    patch_bytes((uint32_t)f, jshort, 2);
    return 1;
}
static HRESULT STDMETHODCALLTYPE h_CreateDevice(IDirect3D9 *d3d, UINT ad, D3DDEVTYPE t, HWND w, DWORD fl, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr;
    force_windowed(pp);
    if (!g_hwnd) g_hwnd = w;
    hr = o_CreateDevice(d3d, ad, t, w, fl, pp, out);
    dlog("CreateDevice %dx%d hwnd %p -> %08x", pp->BackBufferWidth, pp->BackBufferHeight, w, hr);
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        hotpatch((uint8_t *)vt[16], (void *)h_Reset, (void **)&o_Reset, "Reset");
        hotpatch((uint8_t *)vt[17], (void *)h_Present, (void **)&o_Present, "Present");
        SetWindowPos(w, HWND_NOTOPMOST, g_wx, g_wy, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return hr;
}
static IDirect3D9 *(WINAPI *o_Direct3DCreate9)(UINT);
static IDirect3D9 *WINAPI h_Direct3DCreate9(UINT v)
{
    IDirect3D9 *d = o_Direct3DCreate9(v);
    dlog("Direct3DCreate9(%u) -> %p", v, d);
    if (d && (*(void ***)d)[16] != (void *)h_CreateDevice) {
        void **vt = *(void ***)d; DWORD p;
        VirtualProtect(&vt[16], 4, PAGE_EXECUTE_READWRITE, &p);
        o_CreateDevice = (void *)vt[16]; vt[16] = (void *)h_CreateDevice;
        VirtualProtect(&vt[16], 4, p, &p);
    }
    return d;
}
static HWND (WINAPI *o_CreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static HWND WINAPI h_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD st, int x, int y, int w, int h, HWND par, HMENU m, HINSTANCE inst, LPVOID p)
{
    HWND r;
    if (!par && (st & WS_CHILD) == 0) {
        ex &= ~WS_EX_TOPMOST; ex |= WS_EX_NOACTIVATE; title = "H5JP TEST"; x = g_wx; y = g_wy;
        st = (st & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP;   /* client = window: no resize feedback */
    }
    r = o_CreateWindowExA(ex, cls, title, st, x, y, w, h, par, m, inst, p);
    if (!par && !g_hwnd) g_hwnd = r;
    dlog("CreateWindowExA %s %08x %d,%d %dx%d -> %p", cls && HIWORD(cls) ? cls : "#", st, x, y, w, h, r);
    return r;
}
static BOOL WINAPI h_ShowWindow(HWND w, int cmd)
{
    if (cmd != SW_HIDE) cmd = SW_SHOWNOACTIVATE;
    return ShowWindow(w, cmd);
}
static LONG WINAPI h_SetWindowLongA(HWND w, int i, LONG v)
{
    if (w == g_hwnd && (i == GWL_STYLE || i == GWL_EXSTYLE)) return GetWindowLongA(w, i);
    return SetWindowLongA(w, i, v);
}
static BOOL WINAPI h_SetWindowPos(HWND w, HWND after, int x, int y, int cx, int cy, UINT fl)
{
    if (w == g_hwnd) return SetWindowPos(w, HWND_NOTOPMOST, g_wx, g_wy, cx, cy, (fl & SWP_NOSIZE) | SWP_NOACTIVATE);
    return SetWindowPos(w, after, x, y, cx, cy, fl);
}
static BOOL WINAPI h_MoveWindow(HWND w, int x, int y, int cx, int cy, BOOL rp)
{
    if (w == g_hwnd) return MoveWindow(w, g_wx, g_wy, cx, cy, rp);
    return MoveWindow(w, x, y, cx, cy, rp);
}
static BOOL WINAPI h_True(void) { return TRUE; }
static BOOL WINAPI h_ClipCursor(const RECT *r) { (void)r; return TRUE; }
static BOOL WINAPI h_SetCursorPos(int x, int y) { (void)x; (void)y; return TRUE; }
static HWND WINAPI h_SetActiveWindow(HWND w) { (void)w; return g_hwnd; }
static BOOL WINAPI h_GetCursorPos(LPPOINT p)
{
    testio_cursor(p);
    if (g_hwnd) ClientToScreen(g_hwnd, p);
    return TRUE;
}

/* "My Documents" -> _jp\test\docs */
static char g_docs[MAX_PATH];
static HRESULT WINAPI h_SHGetFolderPathA(HWND w, int csidl, HANDLE tok, DWORD fl, LPSTR out)
{
    int c = csidl & 0xff;
    if (g_docs[0] && (c == CSIDL_PERSONAL || c == CSIDL_APPDATA || c == CSIDL_LOCAL_APPDATA || c == CSIDL_COMMON_APPDATA)) {
        strcpy(out, g_docs);
        dlog("SHGetFolderPathA %x -> %s", csidl, out);
        return S_OK;
    }
    return SHGetFolderPathA(w, csidl, tok, fl, out);
}

HRESULT WINAPI real_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u);
HRESULT WINAPI p_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u)
{
    HRESULT hr = real_DirectInput8Create(h, v, r, o, u);
    dlog("DirectInput8Create -> %08x", hr);
    if (g_test && SUCCEEDED(hr)) testio_wrap_dinput(*o);
    return hr;
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == 0xc0000005 || c == 0xc0000094 || c == 0xc000001d || c == 0xc00000fd) {
        CONTEXT *x = e->ContextRecord;
        dlog("EXCEPTION %08x at %08x eax %08x ecx %08x edx %08x esp %08x", c, x->Eip, x->Eax, x->Ecx, x->Edx, x->Esp);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static int WINAPI h_MessageBoxA(HWND w, LPCSTR t, LPCSTR c, UINT f) { dlog("MessageBoxA %s / %s", t, c); (void)w; (void)f; return IDOK; }

void proxy_init(void);
static void init(void)
{
    char buf[MAX_PATH];
    InitializeCriticalSection(&g_logcs);
    if (GetEnvironmentVariableA("H5JP_TEST", buf, sizeof buf) && buf[0] >= '1') g_test = buf[0] - '0';
    if (GetEnvironmentVariableA("H5JP_LOG", buf, sizeof buf)) g_log = fopen(buf, "w");
    if (!g_test) return;
    GetEnvironmentVariableA("H5JP_DOCS", g_docs, sizeof g_docs);
    dlog("h5 test start, docs %s", g_docs);
    AddVectoredExceptionHandler(1, veh);
    o_Direct3DCreate9 = iat_hook("d3d9.dll", "Direct3DCreate9", (void *)h_Direct3DCreate9);
    o_CreateWindowExA = iat_hook("USER32.dll", "CreateWindowExA", (void *)h_CreateWindowExA);
    iat_hook("USER32.dll", "ShowWindow", (void *)h_ShowWindow);
    iat_hook("USER32.dll", "SetWindowLongA", (void *)h_SetWindowLongA);
    iat_hook("USER32.dll", "SetWindowPos", (void *)h_SetWindowPos);
    iat_hook("USER32.dll", "MoveWindow", (void *)h_MoveWindow);
    iat_hook("USER32.dll", "SetActiveWindow", (void *)h_SetActiveWindow);
    iat_hook("USER32.dll", "ClipCursor", (void *)h_ClipCursor);
    iat_hook("USER32.dll", "SetCursorPos", (void *)h_SetCursorPos);
    iat_hook("USER32.dll", "GetCursorPos", (void *)h_GetCursorPos);
    iat_hook("USER32.dll", "MessageBoxA", (void *)h_MessageBoxA);
    iat_hook("SHELL32.dll", "SHGetFolderPathA", (void *)h_SHGetFolderPathA);
    testio_init();
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r; (void)h;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init();
    }
    return TRUE;
}
