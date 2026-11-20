/* Test mode only (H4JP_TEST=1): windowed (registry "full_screen" read as 0, registry writes dropped),
   fake DirectDraw (fakedd.c), input injected through the game's own PeekMessage calls (real input is dropped),
   driven by tools/win.py through the shared memory H4JP_TESTIO. */
#include "h4jp.h"
#include <string.h>
#include <intrin.h>

typedef struct { LONG type, a, b, c; } Cmd;
typedef struct {
    volatile LONG magic, wr, rd;
    Cmd cmd[256];
    volatile LONG frame, busy, hwnd;
} Shm;
enum { C_KEYDOWN = 1, C_KEYUP, C_MOVE, C_BDOWN, C_BUP, C_WAIT, C_CHAR };

static Shm *g_shm;
static HWND g_hwnd;
static POINT g_mouse = { 512, 384 };
static uint8_t g_keys[256], g_qkeys[256];   /* state as delivered / as queued */
static MSG g_q[64]; static int g_qh, g_qt;
static DWORD g_wait_until;

static BOOL CALLBACK find_cb(HWND h, LPARAM l)
{
    DWORD pid;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(h) && !GetParent(h)) { *(HWND *)l = h; return FALSE; }
    return TRUE;
}

static void qpush(UINT m, WPARAM w, LPARAM l)
{
    MSG *q = &g_q[g_qt & 63];
    if (g_qt - g_qh >= 64) return;
    memset(q, 0, sizeof *q);
    q->hwnd = g_hwnd; q->message = m; q->wParam = w; q->lParam = l; q->time = GetTickCount();
    q->pt = g_mouse;
    g_qt++;
}
static WPARAM mflags(void)
{
    WPARAM w = 0;
    if (g_qkeys[VK_LBUTTON]) w |= MK_LBUTTON;
    if (g_qkeys[VK_RBUTTON]) w |= MK_RBUTTON;
    if (g_qkeys[VK_SHIFT]) w |= MK_SHIFT;
    if (g_qkeys[VK_CONTROL]) w |= MK_CONTROL;
    return w;
}

static void pump(void)
{
    if (!g_shm) return;
    g_shm->frame++;
    if (!g_hwnd) {
        EnumWindows(find_cb, (LPARAM)&g_hwnd);
        if (!g_hwnd) return;
        SetWindowTextA(g_hwnd, "H4JP TEST");
        g_shm->hwnd = (LONG)(LONG_PTR)g_hwnd;
        qpush(WM_ACTIVATEAPP, 1, 0); qpush(WM_ACTIVATE, WA_ACTIVE, 0); qpush(WM_SETFOCUS, 0, 0);
    }
    if (g_wait_until && (LONG)(GetTickCount() - g_wait_until) < 0) return;
    g_wait_until = 0;
    while (g_shm->rd != g_shm->wr && g_qt - g_qh < 48) {
        Cmd c = g_shm->cmd[g_shm->rd & 255];
        g_shm->rd++;
        switch (c.type) {
        case C_KEYDOWN:
            g_qkeys[c.a & 255] = 0x80;
            qpush(WM_KEYDOWN, c.a, 1 | (MapVirtualKeyA(c.a, 0) << 16));
            break;
        case C_KEYUP:
            g_qkeys[c.a & 255] = 0;
            qpush(WM_KEYUP, c.a, 1 | (MapVirtualKeyA(c.a, 0) << 16) | 0xC0000000);
            break;
        case C_CHAR: qpush(WM_CHAR, c.a, 1); break;
        case 9:   /* second press of a double click */
            g_qkeys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0x80;
            qpush(c.a ? WM_RBUTTONDBLCLK : WM_LBUTTONDBLCLK, mflags(), MAKELPARAM(g_mouse.x, g_mouse.y));
            break;
        case 8: { void fakedd_log(int); fakedd_log(c.a); break; }
        case C_MOVE: g_mouse.x = c.a; g_mouse.y = c.b; qpush(WM_MOUSEMOVE, mflags(), MAKELPARAM(c.a, c.b)); break;
        case C_BDOWN:
            g_qkeys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0x80;
            qpush(c.a ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, mflags(), MAKELPARAM(g_mouse.x, g_mouse.y));
            break;
        case C_BUP:
            g_qkeys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0;
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
    static int once;
    if (!once) { once = 1; logf_("PeekMessage hook active"); }
    pump();
    if (g_qh != g_qt) {
        MSG *q = &g_q[g_qh & 63];
        /* the game ignores mouse messages handled by its busy-time pump 0x8d0f20 (flag 0xad2557) */
        int busy_pump = (uint32_t)_ReturnAddress() == 0x8d0f55;
        static void *lastra;
        if (_ReturnAddress() != lastra) { lastra = _ReturnAddress(); logf_("peek from %p", lastra); }
        if ((!a && !b || (q->message >= a && q->message <= b))
            && !(busy_pump && q->message >= WM_MOUSEFIRST && q->message <= WM_MOUSELAST)) {
            *m = *q;
            if (r & PM_REMOVE) {
                g_qh++;
                switch (m->message) {
                case WM_KEYDOWN: g_keys[m->wParam & 255] = 0x80; break;
                case WM_KEYUP: g_keys[m->wParam & 255] = 0; break;
                case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: g_keys[VK_LBUTTON] = 0x80; break;
                case WM_LBUTTONUP: g_keys[VK_LBUTTON] = 0; break;
                case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: g_keys[VK_RBUTTON] = 0x80; break;
                case WM_RBUTTONUP: g_keys[VK_RBUTTON] = 0; break;
                }
            }
            return TRUE;
        }
    }
    for (;;) {
        BOOL ok = o_PeekMessageA(m, h, a, b, r);
        if (!ok) return FALSE;
        if (is_input(m->message)) {
            if (!(r & PM_REMOVE)) o_PeekMessageA(m, h, m->message, m->message, PM_REMOVE);
            continue;   /* drop real input */
        }
        if ((m->message == WM_ACTIVATEAPP && !m->wParam) || m->message == WM_KILLFOCUS
            || (m->message == WM_ACTIVATE && LOWORD(m->wParam) == WA_INACTIVE)) {
            if (!(r & PM_REMOVE)) o_PeekMessageA(m, h, m->message, m->message, PM_REMOVE);
            continue;
        }
        return TRUE;
    }
}
/* the test window sits off-screen: the game sees screen coordinates = client coordinates */
static BOOL WINAPI h_GetCursorPos(LPPOINT p) { *p = g_mouse; return TRUE; }
static BOOL WINAPI h_ClientToScreen(HWND h, LPPOINT p) { return TRUE; }
static BOOL WINAPI h_GetWindowRect(HWND h, LPRECT r)
{
    if (!GetClientRect(h, r)) return FALSE;
    return TRUE;
}
static SHORT WINAPI h_GetKeyState(int vk) { return g_keys[vk & 255] ? (SHORT)0xff80 : 0; }
static HWND WINAPI h_GetActiveWindow(void) { return g_hwnd; }
static HWND WINAPI h_WindowFromPoint(POINT p) { return g_hwnd; }
static HWND WINAPI h_SetActiveWindow(HWND h) { return g_hwnd; }

static LONG (WINAPI *o_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
static LONG WINAPI h_RegQueryValueExA(HKEY k, LPCSTR n, LPDWORD r, LPDWORD t, LPBYTE d, LPDWORD c)
{
    static int once;
    if (!once) { once = 1; test_rehook(); }   /* before the window is created */
    if (n && !_stricmp(n, "full_screen")) {
        if (t) *t = REG_DWORD;
        if (d && c && *c >= 4) *(DWORD *)d = 0;
        if (c) *c = 4;
        return ERROR_SUCCESS;
    }
    return o_RegQueryValueExA(k, n, r, t, d, c);
}
static LONG WINAPI h_RegSetValueExA(HKEY k, LPCSTR n, DWORD r, DWORD t, const BYTE *d, DWORD c)
{
    return ERROR_SUCCESS;   /* keep the player's settings */
}

/* the GOG ddraw.dll wrapper patches PeekMessageA / GetCursorPos in the exe after our DllMain:
   called again from the (fake) DirectDrawCreateEx */
static void unwrap(void)
{
    /* IAT slots of the exe that point into the GOG ddraw.dll wrapper go back to the real functions */
    uint8_t *b = (uint8_t *)GetModuleHandleA(0);
    HMODULE w = GetModuleHandleA("ddraw.dll");
    IMAGE_NT_HEADERS *nt; IMAGE_IMPORT_DESCRIPTOR *d; uint32_t lo, hi; int n = 0;
    if (!w) return;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)w + ((IMAGE_DOS_HEADER *)w)->e_lfanew);
    lo = (uint32_t)w; hi = lo + nt->OptionalHeader.SizeOfImage;
    nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    d = (IMAGE_IMPORT_DESCRIPTOR *)(b + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *o = (IMAGE_THUNK_DATA *)(b + d->OriginalFirstThunk), *t = (IMAGE_THUNK_DATA *)(b + d->FirstThunk);
        HMODULE m;
        if (!_stricmp((char *)(b + d->Name), "ddraw.dll") || !d->OriginalFirstThunk) continue;
        m = GetModuleHandleA((char *)(b + d->Name));
        for (; o->u1.AddressOfData; o++, t++) {
            uint32_t v = (uint32_t)t->u1.Function; DWORD op; void *f;
            if (v < lo || v >= hi || (o->u1.Ordinal & IMAGE_ORDINAL_FLAG)) continue;
            f = (void *)GetProcAddress(m, (char *)((IMAGE_IMPORT_BY_NAME *)(b + o->u1.AddressOfData))->Name);
            if (!f) continue;
            VirtualProtect(&t->u1.Function, 4, PAGE_READWRITE, &op); t->u1.Function = (uint32_t)f; VirtualProtect(&t->u1.Function, 4, op, &op);
            n++;
        }
    }
    logf_("unwrapped %d imports from the ddraw wrapper", n);
}

void test_rehook(void)
{
    unwrap();
    if (*(void **)0x970294 != (void *)h_PeekMessageA) {
        o_PeekMessageA = iat_hook("user32.dll", "PeekMessageA", h_PeekMessageA);
        logf_("PeekMessageA hooked (was %p)", o_PeekMessageA);
    }
    if (*(void **)0x970258 != (void *)h_GetCursorPos) iat_hook("user32.dll", "GetCursorPos", h_GetCursorPos);
    if (*(void **)0x97021c != (void *)h_ClientToScreen) iat_hook("user32.dll", "ClientToScreen", h_ClientToScreen);
    if (*(void **)0x97022c != (void *)h_GetWindowRect) iat_hook("user32.dll", "GetWindowRect", h_GetWindowRect);
    if (*(void **)0x970230 != (void *)h_WindowFromPoint) iat_hook("user32.dll", "WindowFromPoint", h_WindowFromPoint);
}

static LRESULT (CALLBACK *o_wndproc)(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK h_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if ((m >= 0x201 && m <= 0x20a) || m == WM_KEYDOWN) {
        typedef uint8_t *(__cdecl *find_t)(int, int);
        uint8_t *wn = ((find_t)0x7212e0)(LOWORD(l), HIWORD(l));
        logf_("wndproc %x %x %x busy=%d win=%p vt=%p up=%p org=%d,%d", m, w, l, *(uint8_t *)0xad2557, wn,
              wn ? *(void **)wn : 0, wn ? (*(void ***)wn)[10] : 0,
              wn ? *(int *)(wn + 0x90) : 0, wn ? *(int *)(wn + 0x94) : 0);
    }
    return o_wndproc(h, m, w, l);
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    CONTEXT *c = e->ContextRecord;
    if (e->ExceptionRecord->ExceptionCode == 0xC0000005 || e->ExceptionRecord->ExceptionCode == 0xC0000094) {
        uint32_t *sp = (uint32_t *)c->Esp; int i;
        logf_("EXCEPTION %08x at %08x eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x esp=%08x",
              e->ExceptionRecord->ExceptionCode, c->Eip, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        for (i = 0; i < 24; i++) if (!IsBadReadPtr(sp + i, 4)) logf_("  [esp+%02x] %08x", i * 4, sp[i]);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void test_init(void)
{
    AddVectoredExceptionHandler(1, veh);
    o_wndproc = make_tramp(0x8d14e0, 7);
    hook_jmp(0x8d14e0, h_wndproc);
    HANDLE m;
    fakedd_init();
    o_RegQueryValueExA = iat_hook("advapi32.dll", "RegQueryValueExA", h_RegQueryValueExA);
    iat_hook("advapi32.dll", "RegSetValueExA", h_RegSetValueExA);
    test_rehook();
    iat_hook("user32.dll", "GetKeyState", h_GetKeyState);
    iat_hook("user32.dll", "GetActiveWindow", h_GetActiveWindow);
    iat_hook("user32.dll", "SetActiveWindow", h_SetActiveWindow);
    m = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, sizeof(Shm), "H4JP_TESTIO");
    if (m) g_shm = (Shm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm));
    if (g_shm) { memset((void *)g_shm, 0, sizeof *g_shm); g_shm->magic = 0x504a3448; }
}
