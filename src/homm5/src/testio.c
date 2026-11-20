/* Test mode only (DSTJP_TEST=1): input injection through DirectInput and back-buffer screenshots,
   driven by tools/win.py through the shared memory "H5JP_TESTIO". The real keyboard/mouse are
   ignored so the game never reacts to what the user does in other windows. */
#include "h5t.h"
#include <stdio.h>
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <initguid.h>
#include <dinput.h>

typedef struct { LONG type, a, b, c; } Cmd;
typedef struct {
    volatile LONG magic;
    volatile LONG wr, rd;
    Cmd cmd[256];
    volatile LONG shot_req, shot_done;
    char shot_path[260];
    volatile LONG frame;
    volatile LONG busy;          /* 1 while commands are pending or a wait runs */
} Shm;

enum { C_KEYDOWN = 1, C_KEYUP, C_MOVE, C_BDOWN, C_BUP, C_WAIT, C_WHEEL, C_SETPOS };
static POINT g_cur = { 400, 300 };
void testio_cursor(POINT *p) { *p = g_cur; }

static Shm *g_shm;
static uint8_t g_keys[256];
static DIDEVICEOBJECTDATA g_kq[16][256]; static int g_kqn[16];   /* one queue per keyboard device */
static void pump_pub(void);
static uint8_t g_vk[256], g_vkpress[256];

static int dik2vk(int dik)
{
    switch (dik) {
    case 0xc8: return VK_UP;   case 0xd0: return VK_DOWN; case 0xcb: return VK_LEFT; case 0xcd: return VK_RIGHT;
    case 0xc9: return VK_PRIOR; case 0xd1: return VK_NEXT; case 0xc7: return VK_HOME; case 0xcf: return VK_END;
    case 0xd3: return VK_DELETE; case 0x9c: return VK_RETURN;
    }
    return MapVirtualKeyA(dik, MAPVK_VSC_TO_VK_EX);
}
static void set_key(int dik, int down)
{
    int vk = dik2vk(dik) & 255;
    g_keys[dik & 255] = down ? 0x80 : 0;
    {
        static DWORD seq;
        int q;
        seq++;
        for (q = 0; q < 16; q++) {
            DIDEVICEOBJECTDATA *e;
            if (g_kqn[q] >= 256) continue;
            e = &g_kq[q][g_kqn[q]++];
            e->dwOfs = dik & 255;
            e->dwData = down ? 0x80 : 0;
            e->dwTimeStamp = GetTickCount();
            e->dwSequence = seq;
            e->uAppData = 0;
        }
    }
    g_vk[vk] = (uint8_t)down;
    if (down) g_vkpress[vk] = 1;
    if (vk == VK_LSHIFT || vk == VK_RSHIFT) g_vk[VK_SHIFT] = (uint8_t)down;
    if (vk == VK_LCONTROL || vk == VK_RCONTROL) g_vk[VK_CONTROL] = (uint8_t)down;
    if (vk == VK_LMENU || vk == VK_RMENU) g_vk[VK_MENU] = (uint8_t)down;
}

static volatile LONG c_async, c_keystate, c_state[4], c_data[4], c_other;
static void *c_last;
static SHORT WINAPI h_GetAsyncKeyState(int vk)
{
    c_async++;
    SHORT r;
    pump_pub();
    vk &= 255;
    r = (SHORT)((g_vk[vk] ? 0x8000 : 0) | g_vkpress[vk]);
    g_vkpress[vk] = 0;
    return r;
}
static SHORT WINAPI h_GetKeyState(int vk) { c_keystate++; pump_pub(); return (SHORT)(g_vk[vk & 255] ? 0xff80 : 0); }
static LONG g_mx, g_my, g_mz; static uint8_t g_mb[8];  /* immediate mouse state (deltas since last read) */
static DIDEVICEOBJECTDATA g_mq[256]; static int g_mqn;
static DWORD g_wait_until;
static DWORD g_seq;
static void *g_kbds[16], *g_mice[16];
static HANDLE g_kev[16], g_mev[16];
static void signal_all(void) { int i; for (i = 0; i < 16; i++) { if (g_kev[i]) SetEvent(g_kev[i]); if (g_mev[i]) SetEvent(g_mev[i]); } }
static int kbd_index(void *d) { int i; for (i = 0; i < 16; i++) if (g_kbds[i] == d) return i; return 0; }
static int is_kbd(void *d) { int i; for (i = 0; i < 16; i++) if (g_kbds[i] == d) return 1; return 0; }
static int is_mouse(void *d) { int i; for (i = 0; i < 16; i++) if (g_mice[i] == d) return 1; return 0; }
static CRITICAL_SECTION g_cs;

void testio_init(void)
{
    HANDLE m;
    InitializeCriticalSection(&g_cs);
    m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Shm), "H5JP_TESTIO");
    if (!m) return;
    g_shm = (Shm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm));
    if (g_shm) { memset((void *)g_shm, 0, sizeof(Shm)); g_shm->magic = 0x54534454; }
    iat_hook_mod(GetModuleHandleA(NULL), "USER32.dll", "GetAsyncKeyState", (void *)h_GetAsyncKeyState);
    iat_hook_mod(GetModuleHandleA(NULL), "USER32.dll", "GetKeyState", (void *)h_GetKeyState);
}

static void pump(void);
static void pump_pub(void) { pump(); }
static void mq_push(DWORD ofs, DWORD data)
{
    if (g_mqn >= 256) return;
    g_mq[g_mqn].dwOfs = ofs;
    g_mq[g_mqn].dwData = data;
    g_mq[g_mqn].dwTimeStamp = GetTickCount();
    g_mq[g_mqn].dwSequence = ++g_seq;
    g_mq[g_mqn].uAppData = 0;
    g_mqn++;
}

/* called once per frame */
static void pump(void)
{
    if (!g_shm) return;
    EnterCriticalSection(&g_cs);
    while ((int)(GetTickCount() - g_wait_until) >= 0 && g_shm->rd != g_shm->wr) {
        Cmd c = g_shm->cmd[g_shm->rd & 255];
        switch (c.type) {
        case C_KEYDOWN: set_key(c.a, 1); break;
        case C_KEYUP:   set_key(c.a, 0); break;
        case C_SETPOS:  c.a -= g_cur.x; c.b -= g_cur.y;   /* absolute -> relative, then as C_MOVE */
        case C_MOVE:    g_mx += c.a; g_my += c.b; g_cur.x += c.a; g_cur.y += c.b;
                        if (c.a) mq_push(DIMOFS_X, (DWORD)c.a);
                        if (c.b) mq_push(DIMOFS_Y, (DWORD)c.b);
                        break;
        case C_WHEEL:   g_mz += c.a; mq_push(DIMOFS_Z, (DWORD)c.a); break;
        case C_BDOWN:   g_mb[c.a & 7] = 0x80; g_vk[c.a ? VK_RBUTTON : VK_LBUTTON] = 1; g_vkpress[c.a ? VK_RBUTTON : VK_LBUTTON] = 1; mq_push(DIMOFS_BUTTON0 + (c.a & 7), 0x80); break;
        case C_BUP:     g_mb[c.a & 7] = 0; g_vk[c.a ? VK_RBUTTON : VK_LBUTTON] = 0; mq_push(DIMOFS_BUTTON0 + (c.a & 7), 0); break;
        case C_WAIT:    g_wait_until = GetTickCount() + c.a; break;
        }
        InterlockedIncrement(&g_shm->rd);
        signal_all();
    }
    g_shm->busy = ((int)(GetTickCount() - g_wait_until) < 0 || g_shm->rd != g_shm->wr);
    LeaveCriticalSection(&g_cs);
}

/* ---------- screenshots ---------- */
static void save_bmp(const char *path, int w, int h, const uint8_t *bits, int pitch)
{
    FILE *f = fopen(path, "wb");
    BITMAPFILEHEADER fh = { 0 };
    BITMAPINFOHEADER ih = { 0 };
    int y;
    if (!f) return;
    fh.bfType = 0x4d42;
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + w * h * 4;
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    for (y = 0; y < h; y++) fwrite(bits + y * pitch, 4, w, f);
    fclose(f);
}

void testio_present(void *dv)
{
    static DWORD t;
    if (GetTickCount() - t > 5000) {
        t = GetTickCount();
        dlog("input calls: async %ld keystate %ld state k%ld m%ld o%ld data k%ld m%ld o%ld (other %p)", c_async, c_keystate, c_state[0], c_state[1], c_state[2], c_data[0], c_data[1], c_data[2], c_last);
        c_async = c_keystate = c_state[0] = c_state[1] = c_state[2] = c_data[0] = c_data[1] = c_data[2] = 0;
    }
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)dv;
    if (!g_shm) return;
    g_shm->frame++;
    pump();
    if (g_shm->shot_req != g_shm->shot_done) {
        IDirect3DSurface9 *bb = NULL, *sys = NULL, *rt = NULL;
        D3DSURFACE_DESC d;
        if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) {
            IDirect3DSurface9 *src = bb;
            IDirect3DSurface9_GetDesc(bb, &d);
            if (d.MultiSampleType != D3DMULTISAMPLE_NONE &&
                SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, d.Width, d.Height, d.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL))) {
                IDirect3DDevice9_StretchRect(dev, bb, NULL, rt, NULL, D3DTEXF_NONE);
                src = rt;
            }
            if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, NULL))) {
                D3DLOCKED_RECT lr;
                if (SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, src, sys)) &&
                    SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
                    save_bmp(g_shm->shot_path, d.Width, d.Height, (uint8_t *)lr.pBits, lr.Pitch);
                    IDirect3DSurface9_UnlockRect(sys);
                }
                IDirect3DSurface9_Release(sys);
            }
            if (rt) IDirect3DSurface9_Release(rt);
            IDirect3DSurface9_Release(bb);
        }
        g_shm->shot_done = g_shm->shot_req;
    }
}

/* ---------- DirectInput device hooks ---------- */
static HRESULT (STDMETHODCALLTYPE *o_CreateDeviceDI)(IDirectInput8A *, REFGUID, LPDIRECTINPUTDEVICE8A *, LPUNKNOWN);
static HRESULT (STDMETHODCALLTYPE *o_Acquire)(IDirectInputDevice8A *);
static HRESULT (STDMETHODCALLTYPE *o_GetDeviceState)(IDirectInputDevice8A *, DWORD, LPVOID);
static HRESULT (STDMETHODCALLTYPE *o_GetDeviceData)(IDirectInputDevice8A *, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
static HRESULT (STDMETHODCALLTYPE *o_SetCoop)(IDirectInputDevice8A *, HWND, DWORD);
static HRESULT (STDMETHODCALLTYPE *o_SetEvent)(IDirectInputDevice8A *, HANDLE);
static HRESULT STDMETHODCALLTYPE h_SetEvent(IDirectInputDevice8A *d, HANDLE e)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (g_kbds[i] == d) g_kev[i] = e;
        if (g_mice[i] == d) g_mev[i] = e;
    }
    dlog("SetEventNotification %p %p", d, e);
    return o_SetEvent(d, e);
}

static int ours(void *d) { return is_kbd(d) || is_mouse(d); }

static HRESULT STDMETHODCALLTYPE h_Acquire(IDirectInputDevice8A *d)
{
    if (ours(d)) return DI_OK;
    return o_Acquire(d);
}

static HRESULT STDMETHODCALLTYPE h_SetCoop(IDirectInputDevice8A *d, HWND w, DWORD fl)
{
    if (ours(d)) fl = DISCL_BACKGROUND | DISCL_NONEXCLUSIVE;
    return o_SetCoop(d, w, fl);
}

static HRESULT STDMETHODCALLTYPE h_GetDeviceState(IDirectInputDevice8A *d, DWORD n, LPVOID out)
{
    if (is_kbd(d)) c_state[0]++; else if (is_mouse(d)) c_state[1]++; else { c_state[2]++; c_last = d; }
    if (ours(d)) pump();
    if (is_kbd(d)) {
        EnterCriticalSection(&g_cs);
        memset(out, 0, n);
        memcpy(out, g_keys, n < 256 ? n : 256);
        LeaveCriticalSection(&g_cs);
        return DI_OK;
    }
    if (is_mouse(d)) {
        DIMOUSESTATE2 st = { 0 };
        EnterCriticalSection(&g_cs);
        st.lX = g_mx; st.lY = g_my; st.lZ = g_mz;
        memcpy(st.rgbButtons, g_mb, 8);
        g_mx = g_my = g_mz = 0;
        LeaveCriticalSection(&g_cs);
        memset(out, 0, n);
        memcpy(out, &st, n < sizeof st ? n : sizeof st);
        return DI_OK;
    }
    return o_GetDeviceState(d, n, out);
}

static HRESULT STDMETHODCALLTYPE h_GetDeviceData(IDirectInputDevice8A *d, DWORD sz, LPDIDEVICEOBJECTDATA out, LPDWORD inout, DWORD fl)
{
    if (is_kbd(d)) c_data[0]++; else if (is_mouse(d)) c_data[1]++; else { c_data[2]++; c_last = d; }
    if (is_mouse(d) || is_kbd(d)) {
        DWORD max = *inout, n = 0;
        pump();
        EnterCriticalSection(&g_cs);
        if (is_kbd(d)) {
            int q = kbd_index(d);
            while (n < max && n < (DWORD)g_kqn[q]) {
                if (out) memcpy((uint8_t *)out + n * sz, &g_kq[q][n], sz < sizeof(DIDEVICEOBJECTDATA) ? sz : sizeof(DIDEVICEOBJECTDATA));
                n++;
            }
            if (!(fl & DIGDD_PEEK)) { memmove(g_kq[q], g_kq[q] + n, (g_kqn[q] - n) * sizeof g_kq[q][0]); g_kqn[q] -= n; }
            LeaveCriticalSection(&g_cs);
            *inout = n;
            return DI_OK;
        }
        while (n < max && n < (DWORD)g_mqn) {
            if (out) memcpy((uint8_t *)out + n * sz, &g_mq[n], sz < sizeof(DIDEVICEOBJECTDATA) ? sz : sizeof(DIDEVICEOBJECTDATA));
            n++;
        }
        if (!(fl & DIGDD_PEEK)) {
            memmove(g_mq, g_mq + n, (g_mqn - n) * sizeof g_mq[0]);
            g_mqn -= n;
            if (g_mqn == 0) g_mx = g_my = g_mz = 0;
        }
        LeaveCriticalSection(&g_cs);
        *inout = n;
        return DI_OK;
    }
    return o_GetDeviceData(d, sz, out, inout, fl);
}

static void vtp(void **vt, int i, void *r, void **o)
{
    DWORD p;
    if (vt[i] == r) return;
    VirtualProtect(&vt[i], 4, PAGE_EXECUTE_READWRITE, &p);
    *o = vt[i]; vt[i] = r;
    VirtualProtect(&vt[i], 4, p, &p);
}

static HRESULT STDMETHODCALLTYPE h_CreateDeviceDI(IDirectInput8A *di, REFGUID g, LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN u)
{
    HRESULT hr = o_CreateDeviceDI(di, g, out, u);
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        int i;
        DIDEVICEINSTANCEA di;
        int type = 0;
        memset(&di, 0, sizeof di); di.dwSize = sizeof di;
        if (SUCCEEDED(IDirectInputDevice8_GetDeviceInfo(*out, &di))) type = (int)(di.dwDevType & 0xff);
        if (IsEqualGUID(g, &GUID_SysKeyboard) || type == DI8DEVTYPE_KEYBOARD) { for (i = 0; i < 16; i++) if (!g_kbds[i]) { g_kbds[i] = *out; break; } }
        else if (IsEqualGUID(g, &GUID_SysMouse) || type == DI8DEVTYPE_MOUSE) { for (i = 0; i < 16; i++) if (!g_mice[i]) { g_mice[i] = *out; break; } }
        vtp(vt, 7, (void *)h_Acquire, (void **)&o_Acquire);
        vtp(vt, 9, (void *)h_GetDeviceState, (void **)&o_GetDeviceState);
        vtp(vt, 10, (void *)h_GetDeviceData, (void **)&o_GetDeviceData);
        vtp(vt, 13, (void *)h_SetCoop, (void **)&o_SetCoop);
        vtp(vt, 12, (void *)h_SetEvent, (void **)&o_SetEvent);
        dlog("DI device %p kbd=%d mouse=%d", *out, is_kbd(*out), is_mouse(*out));
    }
    return hr;
}

HRESULT testio_wrap_dinput(void *di)
{
    vtp(*(void ***)di, 3, (void *)h_CreateDeviceDI, (void **)&o_CreateDeviceDI);
    return S_OK;
}
