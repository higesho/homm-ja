/* TEST ONLY (compiled with /DH7TEST, never shipped): background input injection for tools/win.py.
   - cursor position comes from the shared memory "H7JP_TESTIO" (GetCursorPos/GetCursorInfo hooked)
   - the game always believes it is the foreground window, and may not move/clip the real cursor
     or bring itself to the front
   - DirectInput keyboard/mouse report only injected input (background, non-exclusive) */
/* ---------- shared memory ---------- */
typedef struct { LONG type, a, b, c; } Cmd;
typedef struct {
    volatile LONG magic;
    volatile LONG wr, rd;
    Cmd cmd[256];
    volatile LONG x, y;          /* cursor, client pixels */
    volatile LONG busy;
    volatile LONG counters[8];
} Shm;
enum { C_KEYDOWN = 1, C_KEYUP, C_MOVETO, C_BDOWN, C_BUP, C_WAIT, C_WHEEL };
static Shm *g_shm;
static CRITICAL_SECTION g_cs;
static HWND g_hwnd;

static HWND game_hwnd(void) {
    if (!g_hwnd || !IsWindow(g_hwnd)) {
        HWND h = NULL;
        DWORD pid;
        while ((h = FindWindowExW(NULL, h, L"LaunchUnrealUWindowsClient", NULL))) {
            GetWindowThreadProcessId(h, &pid);
            if (pid == GetCurrentProcessId()) { g_hwnd = h; break; }
        }
    }
    return g_hwnd;
}

static uint8_t g_keys[256];
static DIDEVICEOBJECTDATA g_kq[256]; static int g_kqn;
static LONG g_mx, g_my, g_mz; static uint8_t g_mb[8];
static DIDEVICEOBJECTDATA g_mq[256]; static int g_mqn;
static DWORD g_wait_until, g_seq;

static void q_push(DIDEVICEOBJECTDATA *q, int *n, DWORD ofs, DWORD data) {
    if (*n >= 256) return;
    q[*n].dwOfs = ofs; q[*n].dwData = data; q[*n].dwTimeStamp = GetTickCount(); q[*n].dwSequence = ++g_seq; q[*n].uAppData = 0;
    (*n)++;
}

static void post_mouse(UINT msg, WPARAM wp) {
    HWND h = game_hwnd();
    if (h) PostMessageW(h, msg, wp, MAKELPARAM(g_shm->x, g_shm->y));
}

static void pump(void) {
    if (!g_shm) return;
    EnterCriticalSection(&g_cs);
    while ((int)(GetTickCount() - g_wait_until) >= 0 && g_shm->rd != g_shm->wr) {
        Cmd c = g_shm->cmd[g_shm->rd & 255];
        switch (c.type) {
        case C_KEYDOWN: g_keys[c.a & 255] = 0x80; q_push(g_kq, &g_kqn, c.a & 255, 0x80); break;
        case C_KEYUP:   g_keys[c.a & 255] = 0; q_push(g_kq, &g_kqn, c.a & 255, 0); break;
        case C_MOVETO: {
            LONG dx = c.a - g_shm->x, dy = c.b - g_shm->y;
            g_shm->x = c.a; g_shm->y = c.b;
            g_mx += dx; g_my += dy;
            if (dx) q_push(g_mq, &g_mqn, DIMOFS_X, (DWORD)dx);
            if (dy) q_push(g_mq, &g_mqn, DIMOFS_Y, (DWORD)dy);
            post_mouse(WM_MOUSEMOVE, 0);
            break; }
        case C_WHEEL: g_mz += c.a; q_push(g_mq, &g_mqn, DIMOFS_Z, (DWORD)c.a); break;
        case C_BDOWN:
            g_mb[c.a & 7] = 0x80; q_push(g_mq, &g_mqn, DIMOFS_BUTTON0 + (c.a & 7), 0x80);
            if (c.b) post_mouse(c.a ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, c.a ? MK_RBUTTON : MK_LBUTTON);
            break;
        case C_BUP:
            g_mb[c.a & 7] = 0; q_push(g_mq, &g_mqn, DIMOFS_BUTTON0 + (c.a & 7), 0);
            if (c.b) post_mouse(c.a ? WM_RBUTTONUP : WM_LBUTTONUP, 0);
            break;
        case C_WAIT: g_wait_until = GetTickCount() + c.a; break;
        }
        InterlockedIncrement(&g_shm->rd);
    }
    g_shm->busy = ((int)(GetTickCount() - g_wait_until) < 0 || g_shm->rd != g_shm->wr);
    LeaveCriticalSection(&g_cs);
}

/* ---------- user32 hooks ---------- */
static BOOL WINAPI h_GetCursorPos(LPPOINT p) {
    HWND h = game_hwnd();
    g_shm->counters[0]++;
    pump();
    if (!p) return FALSE;
    p->x = g_shm->x; p->y = g_shm->y;
    if (h) ClientToScreen(h, p);
    return TRUE;
}
static BOOL WINAPI h_GetCursorInfo(PCURSORINFO ci) {
    BOOL r = GetCursorInfo(ci);
    if (ci) h_GetCursorPos(&ci->ptScreenPos);
    return r;
}
static HWND WINAPI h_GetForegroundWindow(void) { g_shm->counters[1]++; return game_hwnd(); }
static HWND WINAPI h_GetActiveWindow(void) { return game_hwnd(); }
static HWND WINAPI h_GetFocus(void) { return game_hwnd(); }
static BOOL WINAPI h_SetCursorPos(int x, int y) { return TRUE; }
static BOOL WINAPI h_ClipCursor(const RECT *r) { return TRUE; }
static BOOL WINAPI h_SetForegroundWindow(HWND h) { return TRUE; }
static HWND WINAPI h_SetActiveWindow(HWND h) { return game_hwnd(); }
static HWND WINAPI h_SetFocus(HWND h) { return game_hwnd(); }
static SHORT WINAPI h_GetKeyState(int vk) {
    pump();
    if (vk == VK_LBUTTON) return g_mb[0] ? (SHORT)0xff80 : 0;
    if (vk == VK_RBUTTON) return g_mb[1] ? (SHORT)0xff80 : 0;
    { UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC); if (sc && sc < 256 && g_keys[sc]) return (SHORT)0xff80; }
    return 0;
}

/* ---------- DirectInput ---------- */
static void *g_kbd, *g_mouse;
static HRESULT (STDMETHODCALLTYPE *o_CreateDevice)(IDirectInput8W *, REFGUID, LPDIRECTINPUTDEVICE8W *, LPUNKNOWN);
static HRESULT (STDMETHODCALLTYPE *o_Acquire)(IDirectInputDevice8W *);
static HRESULT (STDMETHODCALLTYPE *o_GetDeviceState)(IDirectInputDevice8W *, DWORD, LPVOID);
static HRESULT (STDMETHODCALLTYPE *o_GetDeviceData)(IDirectInputDevice8W *, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
static HRESULT (STDMETHODCALLTYPE *o_SetCoop)(IDirectInputDevice8W *, HWND, DWORD);
static int ours(void *d) { return d && (d == g_kbd || d == g_mouse); }

static HRESULT STDMETHODCALLTYPE h_Acquire(IDirectInputDevice8W *d) { if (ours(d)) return DI_OK; return o_Acquire(d); }
static HRESULT STDMETHODCALLTYPE h_SetCoop(IDirectInputDevice8W *d, HWND w, DWORD fl) {
    if (ours(d)) fl = DISCL_BACKGROUND | DISCL_NONEXCLUSIVE;
    return o_SetCoop(d, w, fl);
}
static HRESULT STDMETHODCALLTYPE h_GetDeviceState(IDirectInputDevice8W *d, DWORD n, LPVOID out) {
    if (!ours(d)) return o_GetDeviceState(d, n, out);
    pump();
    EnterCriticalSection(&g_cs);
    memset(out, 0, n);
    if (d == g_kbd) { g_shm->counters[2]++; memcpy(out, g_keys, n < 256 ? n : 256); }
    else {
        DIMOUSESTATE2 st = { 0 };
        g_shm->counters[3]++;
        st.lX = g_mx; st.lY = g_my; st.lZ = g_mz; memcpy(st.rgbButtons, g_mb, 8);
        g_mx = g_my = g_mz = 0;
        memcpy(out, &st, n < sizeof st ? n : sizeof st);
    }
    LeaveCriticalSection(&g_cs);
    return DI_OK;
}
static HRESULT STDMETHODCALLTYPE h_GetDeviceData(IDirectInputDevice8W *d, DWORD sz, LPDIDEVICEOBJECTDATA out, LPDWORD inout, DWORD fl) {
    DIDEVICEOBJECTDATA *q; int *qn; DWORD max, n = 0;
    if (!ours(d)) return o_GetDeviceData(d, sz, out, inout, fl);
    pump();
    EnterCriticalSection(&g_cs);
    if (d == g_kbd) { q = g_kq; qn = &g_kqn; g_shm->counters[4]++; } else { q = g_mq; qn = &g_mqn; g_shm->counters[5]++; }
    max = *inout;
    while (n < max && n < (DWORD)*qn) {
        if (out) memcpy((uint8_t *)out + n * sz, &q[n], sz < sizeof(DIDEVICEOBJECTDATA) ? sz : sizeof(DIDEVICEOBJECTDATA));
        n++;
    }
    if (!(fl & DIGDD_PEEK)) { memmove(q, q + n, (*qn - n) * sizeof q[0]); *qn -= n; }
    LeaveCriticalSection(&g_cs);
    *inout = n;
    return DI_OK;
}
static void vtp(void **vt, int i, void *r, void **o) {
    DWORD p;
    if (vt[i] == r) return;
    VirtualProtect(&vt[i], sizeof(void *), PAGE_EXECUTE_READWRITE, &p);
    *o = vt[i]; vt[i] = r;
    VirtualProtect(&vt[i], sizeof(void *), p, &p);
}
static HRESULT STDMETHODCALLTYPE h_CreateDevice(IDirectInput8W *di, REFGUID g, LPDIRECTINPUTDEVICE8W *out, LPUNKNOWN u) {
    HRESULT hr = o_CreateDevice(di, g, out, u);
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        if (IsEqualGUID(g, &GUID_SysKeyboard)) g_kbd = *out;
        else if (IsEqualGUID(g, &GUID_SysMouse)) g_mouse = *out;
        vtp(vt, 7, (void *)h_Acquire, (void **)&o_Acquire);
        vtp(vt, 9, (void *)h_GetDeviceState, (void **)&o_GetDeviceState);
        vtp(vt, 10, (void *)h_GetDeviceData, (void **)&o_GetDeviceData);
        vtp(vt, 13, (void *)h_SetCoop, (void **)&o_SetCoop);
        dlog("DI device %p kbd=%d mouse=%d", *out, *out == g_kbd, *out == g_mouse);
    }
    return hr;
}

static void testio_init(HMODULE exe) {
    HANDLE m;
    InitializeCriticalSection(&g_cs);
    m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Shm), "H7JP_TESTIO");
    g_shm = m ? (Shm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm)) : NULL;
    if (!g_shm) { static Shm dummy; g_shm = &dummy; }
    if (g_shm->magic != 0x48374a50) { memset((void *)g_shm, 0, sizeof(Shm)); g_shm->magic = 0x48374a50; g_shm->x = 720; g_shm->y = 450; }
    hook_iat(exe, "USER32.dll", "GetCursorPos", h_GetCursorPos);
    hook_iat(exe, "USER32.dll", "GetCursorInfo", h_GetCursorInfo);
    hook_iat(exe, "USER32.dll", "GetForegroundWindow", h_GetForegroundWindow);
    hook_iat(exe, "USER32.dll", "GetActiveWindow", h_GetActiveWindow);
    hook_iat(exe, "USER32.dll", "GetFocus", h_GetFocus);
    hook_iat(exe, "USER32.dll", "SetCursorPos", h_SetCursorPos);
    hook_iat(exe, "USER32.dll", "ClipCursor", h_ClipCursor);
    hook_iat(exe, "USER32.dll", "SetForegroundWindow", h_SetForegroundWindow);
    hook_iat(exe, "USER32.dll", "SetActiveWindow", h_SetActiveWindow);
    hook_iat(exe, "USER32.dll", "SetFocus", h_SetFocus);
    hook_iat(exe, "USER32.dll", "GetKeyState", h_GetKeyState);
}
