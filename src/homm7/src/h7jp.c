/* dinput8.dll for the Might & Magic Heroes VII Japanese patch.
   - forwards every dinput8 export to the system dinput8.dll
   - starts the game in Japanese: "-LANGUAGE=JPN" is added to the command line the game reads
     (only when the Japanese text files are installed and the user gave no -LANGUAGE= of their own)
   - the cut-scene subtitles are drawn with EngineFonts.SmallFont, which has no Japanese glyphs:
     opening MMH7Game\CookedPC\EngineFonts.upk is redirected to our MMH7Game\CookedPC\H7JP\EngineFonts_JP.upk
     (our own fonts drawn from Noto Sans JP; the game files are not changed)
   Built with /DH7TEST it also contains the input injection used by the test harness (testio.inc). */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#ifdef H7TEST
#define DIRECTINPUT_VERSION 0x0800
#include <initguid.h>
#include <dinput.h>
#endif

static HMODULE real;
static FARPROC p_create, p_canunload, p_getclass, p_reg, p_unreg, p_joy;
static wchar_t g_dir[MAX_PATH];          /* folder of this DLL (Binaries\Win64\) */
static wchar_t g_root[MAX_PATH];         /* game folder */
static wchar_t g_font[MAX_PATH];         /* our EngineFonts replacement */
static wchar_t *g_cmdline;

#ifdef H7TEST
static wchar_t logpath[MAX_PATH];
static void dlog(const char *fmt, ...) {
    FILE *f = _wfopen(logpath, L"a");
    if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}
#else
#define dlog(...) ((void)0)
#endif

static void load_real(void) {
    wchar_t p[MAX_PATH];
    if (real) return;
    GetSystemDirectoryW(p, MAX_PATH);
    lstrcatW(p, L"\\dinput8.dll");
    real = LoadLibraryW(p);
    if (!real) return;
    p_create = GetProcAddress(real, "DirectInput8Create");
    p_canunload = GetProcAddress(real, "DllCanUnloadNow");
    p_getclass = GetProcAddress(real, "DllGetClassObject");
    p_reg = GetProcAddress(real, "DllRegisterServer");
    p_unreg = GetProcAddress(real, "DllUnregisterServer");
    p_joy = GetProcAddress(real, "GetdfDIJoystick");
}

static int file_exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

static int hook_iat(HMODULE mod, const char *dll, const char *fn, void *hook) {
    BYTE *b = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *id = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd.VirtualAddress);
    int n = 0;
    if (!dd.VirtualAddress) return 0;
    for (; id->Name; id++) {
        IMAGE_THUNK_DATA *names, *addrs;
        if (lstrcmpiA((char *)(b + id->Name), dll)) continue;
        names = (IMAGE_THUNK_DATA *)(b + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        addrs = (IMAGE_THUNK_DATA *)(b + id->FirstThunk);
        for (; names->u1.AddressOfData; names++, addrs++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(b + names->u1.AddressOfData);
            if (strcmp((char *)ibn->Name, fn)) continue;
            VirtualProtect(&addrs->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
            addrs->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&addrs->u1.Function, sizeof(void *), old, &old);
            n++;
        }
    }
    dlog("iat %s!%s: %d", dll, fn, n);
    return n;
}

/* ---------- language ---------- */
static LPWSTR WINAPI h_GetCommandLineW(void) {
    return g_cmdline ? g_cmdline : GetCommandLineW();
}

static void make_cmdline(void) {
    LPCWSTR c = GetCommandLineW();
    wchar_t chk[MAX_PATH];
    size_t n;
    /* our text must be installed: Content\JPN holds .JPN files (the game checks that folder) */
    lstrcpyW(chk, g_root); lstrcatW(chk, L"MMH7Game\\Localization\\Content\\JPN\\H7GlobalNames.JPN");
    if (!file_exists(chk)) return;
    if (wcsstr(c, L"LANGUAGE=") || wcsstr(c, L"language=") || wcsstr(c, L"Language=")) return;
    n = wcslen(c) + 32;
    g_cmdline = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, n * sizeof(wchar_t));
    if (!g_cmdline) return;
    lstrcpyW(g_cmdline, c);
    lstrcatW(g_cmdline, L" -LANGUAGE=JPN");
}

/* ---------- subtitle font ---------- */
static HANDLE WINAPI h_CreateFileW(LPCWSTR fn, DWORD acc, DWORD sh, LPSECURITY_ATTRIBUTES sa, DWORD cd, DWORD fl, HANDLE tpl) {
    static const wchar_t suf[] = L"\\CookedPC\\EngineFonts.upk";
    if (fn && g_font[0]) {
        size_t n = wcslen(fn), m = wcslen(suf);
        if (n >= m && !_wcsicmp(fn + n - m, suf)) {
            dlog("redirect %ls", fn);
            fn = g_font;
        }
    }
    return CreateFileW(fn, acc, sh, sa, cd, fl, tpl);
}

#ifdef H7TEST
#include "testio.inc"
#endif

/* exported under their real names through dinput8.def */
HRESULT WINAPI P_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u) {
    HRESULT hr;
    load_real();
    if (!p_create) return E_FAIL;
    hr = ((HRESULT (WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN))p_create)(h, v, r, o, u);
#ifdef H7TEST
    if (SUCCEEDED(hr) && o && *o) vtp(*(void ***)*o, 3, (void *)h_CreateDevice, (void **)&o_CreateDevice);
#endif
    return hr;
}
HRESULT WINAPI P_DllCanUnloadNow(void) { load_real(); return p_canunload ? ((HRESULT (WINAPI *)(void))p_canunload)() : S_FALSE; }
HRESULT WINAPI P_DllGetClassObject(REFCLSID c, REFIID r, LPVOID *o) { load_real(); return p_getclass ? ((HRESULT (WINAPI *)(REFCLSID, REFIID, LPVOID *))p_getclass)(c, r, o) : E_FAIL; }
HRESULT WINAPI P_DllRegisterServer(void) { load_real(); return p_reg ? ((HRESULT (WINAPI *)(void))p_reg)() : E_FAIL; }
HRESULT WINAPI P_DllUnregisterServer(void) { load_real(); return p_unreg ? ((HRESULT (WINAPI *)(void))p_unreg)() : E_FAIL; }
LPCVOID WINAPI P_GetdfDIJoystick(void) { load_real(); return p_joy ? ((LPCVOID (WINAPI *)(void))p_joy)() : NULL; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x) {
    if (r == DLL_PROCESS_ATTACH) {
        HMODULE exe = GetModuleHandleW(NULL);
        wchar_t exename[MAX_PATH];
        DWORD n;
        DisableThreadLibraryCalls(h);
        /* only act inside the game itself */
        GetModuleFileNameW(exe, exename, MAX_PATH);
        if (!wcsstr(exename, L"MMH7Game-Win64-Shipping.exe")) return TRUE;
        n = GetModuleFileNameW(h, g_dir, MAX_PATH);
        while (n && g_dir[n - 1] != L'\\') n--;
        g_dir[n] = 0;
        lstrcpyW(g_root, g_dir); lstrcatW(g_root, L"..\\..\\");          /* Binaries\Win64\..\..\ */
        lstrcpyW(g_font, g_root); lstrcatW(g_font, L"MMH7Game\\CookedPC\\H7JP\\EngineFonts_JP.upk");
        if (!file_exists(g_font)) g_font[0] = 0;
#ifdef H7TEST
        lstrcpyW(logpath, g_dir); lstrcatW(logpath, L"h7test_log.txt");
        testio_init(exe);
#endif
        make_cmdline();
        if (g_cmdline) hook_iat(exe, "KERNEL32.dll", "GetCommandLineW", h_GetCommandLineW);
        if (g_font[0]) hook_iat(exe, "KERNEL32.dll", "CreateFileW", h_CreateFileW);
        dlog("h7jp loaded, font %ls, cmdline %ls", g_font, g_cmdline ? g_cmdline : L"(unchanged)");
    }
    return TRUE;
}
